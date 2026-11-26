# 📚 Sioyek Documentation Hub

Welcome to the comprehensive documentation for **Sioyek (Vulkan & Wayland Edition)** — a keyboard-driven PDF reader tailored for reading textbooks, research papers, and technical manuals.

---

## 🧭 Navigation & Guides

Whether you are a new user getting started, a researcher customizing your daily reading setup, or a developer contributing code, explore the guides below:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        SIOYEK DOCUMENTATION HUB                        │
├──────────────────────────┬─────────────────────────────────────────────┤
│ 📖 User Guide            │ Core concepts: Portals, Smart Jump, Rulers  │
│ ⌨️ Keybindings Reference │ Complete searchable cheat sheet of commands │
│ 🎨 Configuration & Themes│ Custom themes (Catppuccin, Nord, etc.)      │
│ ⚡ LaTeX & SyncTeX       │ Seamless integration with Neovim, VS Code   │
│ 🛠️ Troubleshooting & FAQ │ Wayland, Vulkan ICDs, and scaling solutions │
│ 🏗️ Architecture          │ Vulkan QRhi, MuPDF concurrency, internals   │
│ 🔨 Building & Packaging  │ Compilation and Arch Linux package guides   │
└──────────────────────────┴─────────────────────────────────────────────┘
```

---

## 📑 Core Documentation Index

### 1. [📖 User Guide](user-guide.md)
* **What is Sioyek?**: The design philosophy of distraction-free reading.
* **Smart Jump**: Jumping to unlinked figures, tables, and citations automatically.
* **Portals**: Pinning distant equations or appendices in a synced side view.
* **Overview Window**: Instant non-destructive reference peeking.
* **Visual Ruler & Line Reading**: Focus assistance for dense two-column papers.
* **Annotations & Highlights**: Multi-type semantic highlighting with SQLite & PDF export.
* **Presentation Mode**: Fullscreen slide presentation with page-turn navigation.

### 2. [⌨️ Keybindings Reference](keybindings.md)
* **Vim Navigation**: `gg`, `G`, `space`, `Shift+space`, `j`, `k`, and numeric prefixes (e.g. `150gg`).
* **Search**: Fuzzy document search (`/`), chapter search (`c/`), and regex search.
* **Marks & Bookmarks**: Local lowercase marks (`ma` / `'a`), global uppercase marks (`mA` / `'A`), and named bookmarks.
* **Portals & Overviews**: `p` (create portal), `Tab` (jump portal), `l` (overview definition).
* **Display Controls**: Dark mode toggle (`F8`), visual scroll (`F7`), zoom width smart fit (`F10`).
* **Command Mode**: Running any internal command through the `:` palette.

### 3. [🎨 Configuration & Themes](configuration.md)
* **Configuration Files**: Setting up `~/.config/sioyek/prefs_user.config` and `keys_user.config`.
* **Curated Themes**: Ready-to-copy color palettes:
  - 🌿 *Catppuccin Mocha & Macchiato*
  - ❄️ *Nord*
  - 🌃 *Tokyo Night*
  - 🌲 *Gruvbox Dark*
  - ☀️ *Solarized Dark*
  - 🖤 *Pure Black OLED (High Contrast)*
* **UI & Typography**: Custom fonts, font sizes, status bar styling, and ruler padding.
* **External Search Engines**: Middle-click search for Google Scholar, arXiv, and LibGen.
* **Custom Scripts**: Extending Sioyek with Python scripts via `new_command`.

### 4. [⚡ LaTeX & SyncTeX Integration](synctex.md)
* **Forward Search**: Jump from editor cursor to PDF line.
* **Inverse Search**: Right-click in PDF to jump straight to source line in your editor.
* **Editor Setups**: Complete copy-paste configurations for:
  - **Neovim** (`vimtex` & Lua config)
  - **VS Code** (`LaTeX Workshop` extension)
  - **Emacs** (`AUCTeX`)
  - **Helix** & terminal editors

### 5. [🛠️ Troubleshooting & FAQ](troubleshooting.md)
* **Wayland & Scaling**: Crisp text under Hyprland, Sway, KDE Plasma, and GNOME.
* **Vulkan Driver Selection**: Resolving `VK_ERROR_INITIALIZATION_FAILED` and ICD setup.
* **Qt Private ABI**: Why rebuilding after `pacman -Syu` Qt updates prevents crashes.
* **Corrupt Databases / Cache**: How to reset user data cleanly without losing bookmarks.

### 6. [🏗️ Technical Architecture](architecture.md)
* **Qt 6 QRhi Vulkan Canvas**: SPIR-V ahead-of-time compiled shaders and swapchain lifecycle.
* **MuPDF Threading & Reference Counting**: Condition-variable queue and cloned contexts.
* **Epoch Cache Invalidation**: Zero-cost document reloading and deferred pixmap release.
* **Hardened C++20 Build**: Stack protection, control-flow integrity, and fortify source.

### 7. Developer & Packaging References
* [BUILDING.md](../BUILDING.md) — Comprehensive compiler options, CMake targets, and Sanitizers.
* [PACKAGING.md](../PACKAGING.md) — Offline, reproducible Arch Linux packaging guide.
* [RELEASE.md](../RELEASE.md) — Release validation protocols and verification checklists.
* [CHANGELOG.md](../CHANGELOG.md) — Full version history and release notes.

---

> [!TIP]
> Press `:` inside Sioyek at any time to open the command palette with fuzzy autocompletion across all 130+ commands.
