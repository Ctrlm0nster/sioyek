# Sioyek 3.0 Architecture & Internals

This document provides a technical overview of Sioyek's modernized Linux-focused architecture (v3.0.0-dev), covering the Vulkan hardware renderer, multi-threaded MuPDF rasterization, condition-variable concurrency model, and native Wayland presentation.

---

## 1. High-Level System Architecture

Sioyek follows a decoupled, multi-threaded architecture separating GUI input handling, Vulkan GPU composition, and background CPU document rasterization:

```
┌─────────────────────────────────────────────────────────────┐
│                       GUI Thread (Qt 6)                     │
│  - Wayland / XCB Window & Input Events                      │
│  - Modal Vim-like Command Dispatcher (input.cpp)            │
│  - Document Layout & View State (document_view.cpp)         │
│  - Vulkan Presentation Canvas (vulkan_canvas.cpp)           │
│  - Transparent QPainter Overlay for Text/Icons              │
└───────────────┬─────────────────────────────▲───────────────┘
                │ IPC / Queue                 │ Invalidation
                ▼                             │ Signal
┌─────────────────────────────────────────────┴───────────────┐
│              PdfRenderer Concurrency Engine                 │
│  - Thread-Safe Shared Document Cache (fz_keep_document)     │
│  - std::condition_variable Synchronization                  │
│  - Epoch-Based Cache Invalidation                           │
└───────────────┬─────────────────────────────▲───────────────┘
                │ Work Dispatch               │ Completed Pixmaps
                ▼                             │
┌───────────────────────────────┐ ┌───────────────────────────┐
│   Render Worker Threads (N)   │ │    Search Worker Thread   │
│  - Cloned MuPDF Contexts      │ │  - Non-Blocking Search    │
│  - Tile & Slice Rasterization │ │  - Regex & Smart-Case     │
│  - Native 8-Byte Pointers     │ │  - Cooperative Cancel     │
└───────────────────────────────┘ └───────────────────────────┘
```

---

## 2. Vulkan Hardware Rendering (`VulkanCanvas` & `QRhiWidget`)

### 2.1 Why QRhi?
Previous versions of Sioyek relied on legacy OpenGL immediate-mode or early programmable pipelines. In v3.0, the renderer has transitioned completely to Qt 6's **Qt Rendering Hardware Interface (QRhi)** targeting the **Vulkan** backend:
* Eliminates runtime GLSL compilation overhead: Shaders (`canvas.vert` and `canvas.frag`) are compiled ahead of time into SPIR-V bytecodes via Qt's `qsb` tool and embedded as binary resources (`:/shaders/vulkan/*.qsb`).
* Native Wayland integration: `QRhiWidget` interfaces directly with Wayland subsurfaces and Vulkan swapchains, avoiding XWayland translation layers.
* Robust pipeline state caching: Graphics pipelines, sampler bindings, and dynamic uniform buffers are cached using discrete `PipelineKey` descriptors.

### 2.2 Transparent Painter Overlay Composition
To blend Qt's vector text rendering (status bar, document search highlights, portal borders, and command hints) with Vulkan-rendered PDF pages:
1. `VulkanCanvas::overlayPainter()` lazily initializes a physical-resolution `QImage::Format_RGBA8888_Premultiplied` surface.
2. During the render pass, `VulkanCanvas::flushPainter()` scans non-empty pixel bounds, crops the active rectangle, and uploads it to an ephemeral Vulkan texture.
3. Unused transparent pixels are never uploaded to the GPU, preserving PCIe bandwidth and GPU memory.
4. The overlay is composed over the document geometry using pre-multiplied alpha blending (`One` / `OneMinusSrcAlpha`).

### 2.3 Qt Versioning, QRhi ABI Stability & Vulkan Validation
Because `QRhiWidget` is part of Qt's private GUI layer (`Qt6::GuiPrivate`), it is subject to the [Qt QRhi Compatibility Contract](https://doc.qt.io/qt-6/qrhi.html):
* **No Private Binary Compatibility**: Qt guarantees no ABI stability across Qt releases—even between patch releases—for private headers. Sioyek's Arch Linux packaging strictly pins `qt6-base=X.Y.Z` (enforced by `scripts/verify-release.py`) to prevent runtime crashes from incompatible struct layouts.
* **Qt 6.12.0 Texture Initial Layout (QTBUG-150039)**: Unmodified Qt 6.12.0 creates optimal-tiled Vulkan images with `VK_IMAGE_LAYOUT_PREINITIALIZED`, violating Vulkan specification `VUID-VkImageCreateInfo-initialLayout-12478`. An optional patch (`patches/qt/6.12.0-vulkan-initial-layout.patch`) corrects this for local testing on 6.12.0. Qt 6.12.1 incorporates the upstream fix, allowing test suites to strictly enforce zero Vulkan validation errors without workarounds.

---

## 3. Concurrency & Multi-Threaded Engine

### 3.1 Eliminating Busy Polling
Legacy rendering loops relied on short sleep cycles to poll for pending render jobs. Sioyek 3.0 replaces polling with synchronization primitives:
* `std::condition_variable render_work_ready` wakes idle workers only when new slice/page requests are queued.
* `std::condition_variable search_work_ready` wakes the background search worker and synchronizes cooperative search cancellation.

### 3.2 MuPDF Context Cloning & Reference Counting
MuPDF requires careful threading hygiene:
* **Context Isolation**: Each worker thread receives its own cloned `fz_context` sharing the central allocator and font store locks with the master context.
* **Document Reference Counting**: Opening and caching documents across threads uses `fz_keep_document` and `fz_drop_document`. In-flight rendering jobs retain ownership of the document handle even if an active document view invalidates the cache during file reloads or rapid scrolling.
* **Native 8-Byte Alignment**: Pinned MuPDF static libraries are compiled with `FZ_POINTER_ALIGN_MOD=FZ_MEMORY_BLOCK_ALIGN_MOD`, ensuring Type 3 font glyph paths align on 8-byte boundaries and preventing UBSan misalignment traps.
* **C23 Standard Compliance**: An out-of-tree patch (`patches/mupdf/1.26.11-svg-const-qualifier.patch`) updates `source/svg/svg-parse.c` to preserve `const char *` qualifiers returned by C23 type-generic `strstr()`, preventing `-Wdiscarded-qualifiers` compiler warnings under GCC 15 and 16.

### 3.3 Epoch-Based Cache Invalidation
When a document is reloaded (e.g. from an external LaTeX compilation) or closed:
1. An atomic `document_epoch` counter increments.
2. Pending worker tasks comparing against older epochs discard their completed pixmaps instead of repopulating the cache with stale data.
3. Deferred pixmap disposal (`queue_pixmap_drop`) releases memory safely on the worker's own context.

---

## 4. Input & Modal Tree Engine

Sioyek processes input using a prefix tree of commands:
* Mode transitions (`ruler`, `overview`, `visual_scroll`, `portal`) alter the active root of the input tree.
* Unbound keys reset the prefix parser to root without swallowing or corrupting key events.
* Key logging formats non-printable and special keys using `QKeySequence` names (e.g. `Return`, `Escape`, `Shift+K`) rather than raw control characters, ensuring clean console diagnostics.

---

## 5. Security & Build Hardening

All targets are compiled with modern C++20 and hardened toolchain flags:
* `-fstack-protector-strong` & `-fstack-clash-protection` against stack overflow attacks.
* `-fcf-protection=full` for Intel CET / control-flow enforcement.
* `-ftrivial-auto-var-init=zero` eliminating uninitialized stack variable vulnerabilities.
* `_FORTIFY_SOURCE=3` for compile-time and runtime buffer overrun detection.
* `-Werror` ensuring strict zero-warning compilation under GCC 14 and Clang.
