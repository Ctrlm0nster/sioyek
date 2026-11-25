# Sioyek (Vulkan & Wayland Edition)

[![Linux CI](https://github.com/Ctrlm0nster/sioyek/actions/workflows/linux-cmake.yml/badge.svg)](https://github.com/Ctrlm0nster/sioyek/actions/workflows/linux-cmake.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

**Sioyek** is a keyboard-driven PDF viewer designed specifically for reading textbooks and technical research papers.

This edition is independently maintained by [@Ctrlm0nster](https://github.com/Ctrlm0nster) and based on the original [ahrm/Sioyek](https://github.com/ahrm/sioyek). It targets **desktop Linux** with Arch Linux packaging, Qt 6.7+, native Wayland and X11 support, and a Vulkan renderer built on Qt QRhi.

---

## Contents
- [Architecture & Modernization](#architecture--modernization)
- [Features & Workflow](#features--workflow)
- [Building from Source](#building-from-source)
- [Running Sioyek](#running-sioyek)
- [Arch Linux Packaging](#arch-linux-packaging)
- [Testing & Quality Assurance](#testing--quality-assurance)
- [Documentation & References](#documentation--references)
- [Upstream & Attribution](#upstream--attribution)

---

## Architecture & Modernization {#architecture--modernization}

The Linux edition uses Vulkan for rendering and condition variables to coordinate rasterization and search workers.

* **Vulkan Hardware Rendering via Qt QRhi**:
  Replaces legacy OpenGL with Qt 6's Rendering Hardware Interface (`QRhiWidget`), targeting Vulkan. Includes embedded SPIR-V shaders compiled ahead of time with `qsb`, eliminating runtime shader compilation and immediate-mode GL overhead.
* **Multi-Threaded Rasterization with Condition Variables**:
  Replaces CPU worker thread polling with condition-variable synchronization (`std::condition_variable`), eliminating CPU idle churn while ensuring zero-latency response for concurrent page rasterization and text search.
* **Robust MuPDF Concurrency & Reference Counting**:
  Built against pinned MuPDF 1.26.11 with thread-safe document reference management (`fz_keep_document`), cache invalidation epoch counters, zero-page cache guards, and 8-byte pointer alignment (`FZ_POINTER_ALIGN_MOD`) to prevent Type 3 font UBSan errors.
* **Native Wayland & Fractional Scaling**:
  Engineered for first-class Wayland compositors (Hyprland, Sway, GNOME, KDE Plasma) as well as X11/XWayland via `qt6-wayland` and `qt6-base`, preserving crisp text and overlays across fractional display scales.
* **Hardened Security & Toolchain**:
  Built with C++20 under GCC 14 / Clang with `-Werror`, `-fstack-protector-strong`, `-fcf-protection=full`, `-ftrivial-auto-var-init=zero`, and `_FORTIFY_SOURCE=3`.
* **Zero Platform Bloat**:
  Completely removed legacy Windows (`.bat`, `.dll`), macOS (`.plist`, `.mm`), Android (`gradle`, `AndroidManifest.xml`, JNI), and legacy OpenGL 2/3 shader pipelines (~10,000+ lines of obsolete code removed).

---

## Features & Workflow {#features--workflow}

### Quick Open
Search and jump immediately to any recently opened document or document in your library:

https://user-images.githubusercontent.com/6392321/125321111-9b29dc00-e351-11eb-873e-94ea30016a05.mp4

### Smart Jump
Jump directly to referenced figures, tables, or bibliography entries **even if the PDF does not contain hyperlinks**. Middle-click or Shift+middle-click bibliography names to search directly on Google Scholar or LibGen:

https://user-images.githubusercontent.com/6392321/125321419-e5ab5880-e351-11eb-873e-94ea30016a05.mp4

### Overview Panel
Right-click on references, equations, or figures to inspect them in a non-disruptive floating overview without losing your current reading location:

https://user-images.githubusercontent.com/6392321/154683015-0bae4f92-78e2-4141-8446-49dd7c2bd7c9.mp4

### Portals (Dual-Monitor / Multi-Window Reading)
Link two distant sections of a document (e.g., text referring to a figure 20 pages later). The portal window automatically mirrors the corresponding referenced region as you scroll:

https://user-images.githubusercontent.com/6392321/125322657-41c2ac80-e353-11eb-985e-8f3ce9808f67.mp4

### Vim-Style Marks & Global Bookmarks
* **Marks**: Single-letter vim-like bookmarks. Lowercase marks (`m[a-z]`, `'[a-z]`) are local to the current document; uppercase marks (`m[A-Z]`, `'[A-Z]`) are global across all documents.
* **Bookmarks**: Named text bookmarks searchable with fuzzy matching.

https://user-images.githubusercontent.com/6392321/125321811-505c9400-e352-11eb-85e0-ffc3ae5f8cb8.mp4

### Highlights
Highlight passages using multiple semantic highlight types and search/filter across all annotations in your library:

https://user-images.githubusercontent.com/6392321/130956728-7e0a87fa-4ada-4108-a8fc-9d9d04180f56.mp4

### Table of Contents
Instant fuzzy search across the document outline:

https://user-images.githubusercontent.com/6392321/125321313-cf050180-e351-11eb-9275-c2759c684af5.mp4

### Deep Customization
Keybindings and visual preferences can be customized by editing `~/.config/sioyek/keys_user.config` and `~/.config/sioyek/prefs_user.config`. Default templates are installed with the application:

https://user-images.githubusercontent.com/6392321/125337160-e4832700-e363-11eb-8801-0bee58121c2d.mp4

---

## Building from Source {#building-from-source}

### 1. Prerequisites (Arch Linux)
Ensure standard build tools, Qt 6.7+, and Vulkan loader libraries are installed:

```sh
sudo pacman -S --needed base-devel git python cmake ninja pkgconf unzip \
  qt6-base qt6-declarative qt6-svg qt6-speech qt6-wayland qt6-shadertools \
  vulkan-headers vulkan-icd-loader harfbuzz sqlite zlib libjpeg-turbo
```

> [!NOTE]
> Ensure you have an appropriate Vulkan driver installed for your hardware (e.g., `vulkan-intel` for Intel, `vulkan-radeon` for AMD, or NVIDIA proprietary driver). Run `vulkaninfo --summary` to verify.

### 2. Clone Repository & Submodules
MuPDF 1.26.11 and its vendored sources are included as submodules:

```sh
git clone https://github.com/Ctrlm0nster/sioyek.git
cd sioyek
git submodule update --init --recursive
```

### 3. Build with CMake + Ninja
Using the development build script:
```sh
scripts/dev-build.sh
```

Or using direct CMake commands:
```sh
cmake -S . -B build/dev -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSIOYEK_BUILD_TESTS=ON
cmake --build build/dev --parallel $(nproc)
```

---

## Running Sioyek {#running-sioyek}

### Development Runner
Launch Sioyek directly in an isolated instance (so an already running system reader does not swallow the launch):

```sh
scripts/dev-run.sh [document.pdf]
```

### Direct Execution
Execute the ELF binary directly from the build directory:

```sh
./build/dev/sioyek [document.pdf]
```

> [!IMPORTANT]
> `./build/dev/sioyek` is an ELF binary executable, **not** a shell script. Always invoke it with `./build/dev/sioyek` or through `scripts/dev-run.sh`. Do **not** source it with `. ` or `source `.

---

## Arch Linux Packaging {#arch-linux-packaging}

The Arch package is **`sioyek-ctrlm0nster`**. It provides `sioyek` and cleanly conflicts with the upstream AUR `sioyek` and `sioyek-git` packages.

To build an offline, checksummed Arch package snapshot:

```sh
git submodule update --init --recursive
python3 scripts/prepare-arch-package.py
cd build/arch
makepkg -s
```

Install the resulting `.pkg.tar.zst` package with `pacman`:

```sh
sudo pacman -U sioyek-ctrlm0nster-*.pkg.tar.zst
```

For more details on package lifecycle and pinning, see [PACKAGING.md](PACKAGING.md).

---

## Testing & Quality Assurance {#testing--quality-assurance}

### Headless CTest Suite
Run headless CTest unit tests:

```sh
ctest --test-dir build/dev --output-on-failure
```

### Interactive Wayland & Concurrency Smoke Tests
To run native Wayland integration and worker queue concurrency stress tests inside your active graphical desktop session:

```sh
# Smoke test on native Wayland (focuses a temporary instance, exercises keyboard navigation)
python3 scripts/test-wayland.py build/dev/sioyek

# Concurrency & cache invalidation stress test
python3 scripts/test-render-workers.py build/dev/sioyek
```

---

## Documentation & References {#documentation--references}

* [docs/architecture.md](docs/architecture.md) — Technical overview of Sioyek 3.0 Vulkan presentation, condition-variable worker queues, and concurrency model.
* **C++ API Reference**: Generate full Doxygen API reference and UML inheritance/call graphs locally with `mise run docs` (or `doxygen Doxyfile`). Documentation is emitted to `build/doxygen/html/index.html`.
* [BUILDING.md](BUILDING.md) — Detailed build options, compiler configurations (GCC/Clang), and sanitizer options.
* [PACKAGING.md](PACKAGING.md) — Offline packaging architecture and reproducible build manifests.
* [RELEASE.md](RELEASE.md) — Release validation checklists, verification logs, and signing procedures.
* [CHANGELOG.md](CHANGELOG.md) — Detailed commit and version change history.
* [LICENSES.md](LICENSES.md) — Upstream licenses, third-party component attributions, and GPL-3 / AGPL notices.

---

## Upstream & Attribution {#upstream--attribution}

Sioyek was originally created by **Ali H. Rad** ([@ahrm](https://github.com/ahrm)) and released under the GNU General Public License v3.0.
* Upstream repository: [github.com/ahrm/sioyek](https://github.com/ahrm/sioyek)
* Upstream documentation: [sioyek-documentation.readthedocs.io](https://sioyek-documentation.readthedocs.io/)
* If you appreciate Sioyek, consider supporting the upstream author: [Buy Me a Coffee](https://www.buymeacoffee.com/ahrm).
