# 📖 Sioyek (Vulkan & Wayland Edition)

<div align="center">

[![Linux CI](https://github.com/Ctrlm0nster/sioyek/actions/workflows/linux-cmake.yml/badge.svg)](https://github.com/Ctrlm0nster/sioyek/actions/workflows/linux-cmake.yml)
[![Documentation](https://img.shields.io/badge/Documentation-GitHub%20Pages-informational.svg)](https://Ctrlm0nster.github.io/sioyek/)
<a href="LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPL v3" /></a>
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux%20%7C%20Wayland%20%7C%20X11-purple.svg)](https://archlinux.org)
[![Graphics: Vulkan](https://img.shields.io/badge/Renderer-Vulkan%20%28Qt%20QRhi%29-red.svg)](https://www.vulkan.org/)
[![C++ Standard: 20](https://img.shields.io/badge/C%2B%2B-20-brightgreen.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)

**The keyboard-driven PDF reader purpose-built for reading research papers, technical textbooks, and complex documents.**

🌐 **Live Documentation Website:** [**https://Ctrlm0nster.github.io/sioyek/**](https://Ctrlm0nster.github.io/sioyek/)

[🌐 Online Website](https://Ctrlm0nster.github.io/sioyek/) • [Quick Start](#quick-start) • [Core Superpowers](#core-superpowers) • [Keybindings](#cheat-sheet) • [Documentation Hub](docs/hub.md) • [Architecture](#architecture)

</div>

---

## 🌟 Overview

**Sioyek** is a document viewer engineered specifically for technical reading. Unlike standard PDF readers built for signing contracts or casually viewing slides, Sioyek eliminates the cognitive friction of navigating dense, multi-page cross-references in textbooks and academic papers.

This edition is independently modernized and maintained by [@Ctrlm0nster](https://github.com/Ctrlm0nster), based on the original work by [ahrm/Sioyek](https://github.com/ahrm/sioyek). It targets modern **desktop Linux** with native **Wayland** integration, Arch Linux packaging, and a high-performance **Vulkan hardware renderer** built on Qt 6 QRhi.

---

## ✨ Core Superpowers {#core-superpowers}

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│  🌀 PORTALS                 │ Link two distant sections side-by-side. Read text on     │
│                             │ page 3 while keeping Figure 8 on page 24 pinned in view. │
├─────────────────────────────┼──────────────────────────────────────────────────────────┤
│  ⚡ SMART JUMP               │ Jump straight to referenced figures, tables, or papers   │
│                             │ even if the author never included hyperlinks.            │
├─────────────────────────────┼──────────────────────────────────────────────────────────┤
│  🔍 OVERVIEW WINDOW         │ Right-click equations or citations to preview them in a   │
│                             │ non-disruptive floating card without losing your place.  │
├─────────────────────────────┼──────────────────────────────────────────────────────────┤
│  📏 VISUAL FOCUS RULER      │ Lock onto lines with a subtle focus bar and step line-by-│
│                             │ line with j/k to eliminate fatigue in 2-column papers.   │
├─────────────────────────────┼──────────────────────────────────────────────────────────┤
│  ⌨️ VIM-STYLE NAVIGATION    │ Muscle-memory movement: local marks (ma), global marks   │
│                             │ (mA), search history, and fuzzy chapter finding.         │
├─────────────────────────────┼──────────────────────────────────────────────────────────┤
│  🔄 LATEX & SYNCTEX SYNC    │ Bidirectional jump between LaTeX source and PDF in       │
│                             │ Neovim, VS Code, and Emacs.                              │
└─────────────────────────────┴──────────────────────────────────────────────────────────┘
```

---

## ⚡ Quick Start in 60 Seconds {#quick-start}

### 1. Prerequisites (Arch Linux)
Install build tools, Qt 6.7+, and the Vulkan loader:

```sh
sudo pacman -S --needed base-devel git python cmake ninja pkgconf unzip \
  qt6-base qt6-declarative qt6-svg qt6-speech qt6-wayland qt6-shadertools \
  vulkan-headers vulkan-icd-loader harfbuzz sqlite zlib libjpeg-turbo
```

> [!TIP]
> Ensure you have an appropriate Vulkan driver installed for your GPU (e.g., `vulkan-intel` for Intel, `vulkan-radeon` for AMD, or `nvidia-utils`). Run `vulkaninfo --summary` to verify.

### 2. Clone & Initialize Submodules
```sh
git clone https://github.com/Ctrlm0nster/sioyek.git
cd sioyek
git submodule update --init --recursive
```

### 3. Build & Run
Using the convenient development script:
```sh
scripts/dev-build.sh
scripts/dev-run.sh tutorial.pdf
```

Or using **[mise](https://mise.jdx.dev/)** task runner:
```sh
mise run build
mise run run tutorial.pdf
```

---

## ⌨️ Essential Cheat Sheet {#cheat-sheet}

Sioyek is entirely keyboard-driven. Here are the most essential commands to get you flying:

| Key | Action | Description |
| :--- | :--- | :--- |
| `Space` / `Shift+Space` | **Page Step** | Scroll half a screen down/up (preserves reading continuity) |
| `j` / `k` | **Fine Scroll** | Move down or up smoothly |
| `F10` | **Smart Width** | Crop white margins automatically to maximize reading text |
| `zz` | **Column Snap** | Snap viewport to top of right-hand column in 2-column papers |
| `p` | **Portal** | Press on source text, jump to figure/equation, press `p` again to link |
| `Tab` / `Backspace` | **Jump / Return** | Toggle portal view or jump back to previous reading position |
| `Right-Click` | **Peek Overview** | Inspect referenced equation/citation without moving your cursor |
| `Shift+F` | **Smart Jump** | Show keyboard badges on unlinked citations and figures |
| `Middle-Click` | **Scholar Search** | Search clicked citation directly on Google Scholar |
| `ma` / `'a` | **Local Mark** | Set local mark `a` / return to mark `a` in current document |
| `mA` / `'A` | **Global Mark** | Set global mark `A` across your entire PDF library |
| `t` | **Outline / TOC** | Fuzzy-searchable Table of Contents |
| `/` | **Search** | Fuzzy search text (e.g., `/<10,25>keyword` searches pages 10–25) |
| `F8` | **Dark Mode** | Invert colors for comfortable late-night reading |
| `:` | **Command Palette**| Fuzzy-find and run any of Sioyek's 130+ internal commands |

👉 *For the complete exhaustive table, see the [Keybindings Reference Guide](docs/keybindings.md).*

---

## 📚 Complete Documentation Hub

> [!TIP]
> 🌐 **Interactive Online Website**: Browse the complete documentation at **[https://Ctrlm0nster.github.io/sioyek/](https://Ctrlm0nster.github.io/sioyek/)** featuring an interactive light/dark theme, live architecture Mermaid diagrams, and searchable C++ API reference.

Detailed guides are also available locally in the [`docs/`](docs/hub.md) directory:

- 📖 **[User Guide & Workflows](docs/user-guide.md)** — Step-by-step walkthrough of Portals, Smart Jump, Overviews, and Annotations.
- ⌨️ **[Keybindings Reference](docs/keybindings.md)** — Full searchable directory of all 130+ commands and default keys.
- 🎨 **[Configuration & Curated Themes](docs/configuration.md)** — Settings guide with ready-to-use themes (*Catppuccin, Nord, Tokyo Night, Gruvbox, Solarized*).
- ⚡ **[LaTeX & SyncTeX Integration](docs/synctex.md)** — Complete bidirectional setup for Neovim (vimtex), VS Code, Emacs, and Helix.
- 🛠️ **[Troubleshooting & FAQ](docs/troubleshooting.md)** — Practical fixes for Wayland scaling, Vulkan ICD selection, and cache management.
- 🏗️ **[Architecture & Internals](docs/architecture.md)** — Deep dive into Vulkan QRhi, MuPDF context cloning, and epoch caching.
- 🔨 **[BUILDING.md](BUILDING.md)** — Advanced compiler configurations, Clang/GCC flags, and sanitizers.
- 📦 **[PACKAGING.md](PACKAGING.md)** — Arch Linux packaging (`sioyek-ctrlm0nster`) and reproducible manifests.

---

## 🏗️ Modernized Architecture {#architecture}

The Linux edition incorporates modern systems engineering designed for high throughput and zero CPU idle churn:

* **Vulkan Hardware Renderer (`QRhiWidget`)**:
  Replaces legacy OpenGL with Qt 6's Rendering Hardware Interface targeting Vulkan. Shaders are pre-compiled into SPIR-V bytecodes via `qsb` ahead-of-time, eliminating runtime shader compilation.
* **Zero-Idle Concurrency Engine**:
  Replaces CPU worker sleep-polling with OS synchronization primitives (`std::condition_variable`), eliminating CPU idle churn while delivering instant rasterization response.
* **MuPDF 1.26.11 Concurrency & 8-Byte Pointer Alignment**:
  Thread-safe document reference management (`fz_keep_document`), epoch-based cache invalidation, and native 8-byte pointer alignment (`FZ_POINTER_ALIGN_MOD`) preventing Type 3 font UBSan traps.
* **Native Wayland & Fractional Scaling**:
  Engineered for first-class Wayland compositors (Hyprland, Sway, KDE Plasma, GNOME) with crisp subsurfaces and fractional scale support.
* **Hardened Security Toolchain**:
  Compiled with modern C++20 under `-Werror`, `-fstack-protector-strong`, `-fcf-protection=full`, `-ftrivial-auto-var-init=zero`, and `_FORTIFY_SOURCE=3`.
* **Zero Platform Bloat**:
  Completely purged obsolete Windows, macOS, Android, and legacy OpenGL 2/3 codebases (~10,000+ lines removed).

---

## 📦 Arch Linux Packaging

The official package for this edition is **`sioyek-ctrlm0nster`**. It replaces and provides `sioyek` while conflicting cleanly with outdated AUR recipes.

To build an offline, checksummed Arch package snapshot:

```sh
git submodule update --init --recursive
python3 scripts/prepare-arch-package.py
cd build/arch
makepkg -s
sudo pacman -U sioyek-ctrlm0nster-*.pkg.tar.zst
```

---

## 🧪 Testing & Verification

Run the headless CTest unit suite:
```sh
ctest --test-dir build/dev --output-on-failure
```

Run Wayland integration and worker queue concurrency stress tests:
```sh
# Native Wayland input and window focus smoke test
python3 scripts/test-wayland.py build/dev/sioyek

# Multi-threaded worker queue and cache invalidation stress test
python3 scripts/test-render-workers.py build/dev/sioyek
```

---

## 🤝 Upstream & Attribution

**Sioyek** was originally created by **Ali H. Rad** ([@ahrm](https://github.com/ahrm)) and released under the GNU General Public License v3.0.
* Upstream repository: [github.com/ahrm/sioyek](https://github.com/ahrm/sioyek)
* Upstream documentation: [sioyek-documentation.readthedocs.io](https://sioyek-documentation.readthedocs.io/)
* If you find Sioyek valuable in your research work, consider supporting the upstream author: [Buy Me a Coffee](https://www.buymeacoffee.com/ahrm).

---

<div align="center">
Distributed under the <b>GNU General Public License v3.0</b>. See <a href="LICENSE">LICENSE</a> for details.
</div>
