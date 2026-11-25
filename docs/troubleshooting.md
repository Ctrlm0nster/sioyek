# 🛠️ Sioyek Troubleshooting & FAQ Guide

Solutions to common questions and issues when running **Sioyek (Vulkan & Wayland Edition)** on Linux desktops.

---

## 📑 Common Topics

1. [Vulkan Setup & GPU Driver Diagnostics](#sec-vulkan-setup)
2. [Wayland Compositors & Fractional Scaling](#sec-wayland-scaling)
3. [Hyprland / Sway Window Rules for Portals](#sec-window-rules)
4. [Qt Updates & ABI Compatibility (`pacman -Syu`)](#sec-qt-updates)
5. [Cache Cleaning & Preserving User Data](#sec-cache-cleaning)
6. [Missing Submodules or Build Failures](#sec-missing-submodules)

---

## 1. Vulkan Setup & GPU Driver Diagnostics {#sec-vulkan-setup}

Sioyek 3.0 renders its canvas using Vulkan via Qt's `QRhiWidget`. A functioning Vulkan loader and hardware ICD driver are required.

### Verifying Vulkan Hardware Support
Run the standard diagnostic tool:
```sh
vulkaninfo --summary
```
If you see your GPU listed under `Devices:`, your driver is operational.

### Missing Vulkan ICD Driver
If Sioyek exits with `Failed to create Vulkan instance` or `VK_ERROR_INITIALIZATION_FAILED`, ensure you have the driver package for your GPU installed:

* **Intel GPUs**: `sudo pacman -S vulkan-intel`
* **AMD GPUs**: `sudo pacman -S vulkan-radeon`
* **NVIDIA GPUs**: `sudo pacman -S nvidia-utils` (or `nvidia-open`)
* **Software Fallback (VMs/Testing)**: `sudo pacman -S vulkan-swrast` (Lavapipe)

### Multi-GPU Laptops (Intel/AMD iGPU + NVIDIA dGPU)
By default, Vulkan selects the primary integrated GPU to save battery life. If you wish to force discrete GPU execution:
```sh
# For NVIDIA PRIME offload
prime-run sioyek [document.pdf]

# Or explicitly select Vulkan ICD
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.json sioyek [document.pdf]
```

---

## 2. Wayland Compositors & Fractional Scaling {#sec-wayland-scaling}

Sioyek runs natively on Wayland without XWayland overhead.

### Blurry Text on Fractional Displays (125%, 150%, 175%)
Ensure `qt6-wayland` is installed:
```sh
sudo pacman -S qt6-wayland
```
Qt 6 automatically queries the compositor for the correct fractional scale factor. Ensure your environment specifies Wayland:
```sh
export QT_QPA_PLATFORM="wayland;xcb"
```

### Forcing X11 / XWayland
If running into compositor-specific subsurface bugs:
```sh
QT_QPA_PLATFORM=xcb sioyek [document.pdf]
```

---

## 3. Hyprland / Sway Window Rules for Portals {#sec-window-rules}

When using Sioyek's dual-window portal feature (`toggle_window_configuration` or `F12`), tiling window managers may tile the small helper window into your main layout.

### Hyprland Configuration (`~/.config/hypr/hyprland.conf`)
```ini
# Keep the main reader tiled or maximize it, but float portal helper previews if desired
windowrulev2 = float, class:^(sioyek)$, title:^(.*Overview.*)$
windowrulev2 = size 700 500, class:^(sioyek)$, title:^(.*Overview.*)$
```

### Sway Configuration (`~/.config/sway/config`)
```ini
for_window [app_id="sioyek" title=".*Overview.*"] floating enable
```

---

## 4. Qt Updates & ABI Compatibility (`pacman -Syu`) {#sec-qt-updates}

### Why does Sioyek complain or refuse to launch after a major Qt update?
The Vulkan canvas uses `Qt6::GuiPrivate` (`QRhiWidget`), which interacts directly with Qt's low-level memory layout. The Qt project **does not guarantee ABI stability** for private internal headers across minor releases (e.g. from Qt 6.7 to 6.8).

Sioyek's Arch package explicitly enforces `qt6-base=X.Y.Z` dependency locks so your system won't silently launch an incompatible binary and segfault.

### Rebuilding after a Qt update:
Simply rebuild the Arch package:
```sh
python3 scripts/prepare-arch-package.py
cd build/arch
makepkg -s
sudo pacman -U sioyek-ctrlm0nster-*.pkg.tar.zst
```
Or for local development:
```sh
scripts/dev-build.sh
```

---

## 5. Cache Cleaning & Preserving User Data {#sec-cache-cleaning}

If you suspect corrupted render caches or wish to start fresh:

### Safe Cache Reset (Preserves your highlights, marks, and bookmarks):
```sh
rm -rf ~/.cache/sioyek
```
The render cache will be cleanly regenerated on next launch.

### User Data Storage (Highlights, Bookmarks, Reading Positions):
Your persistent database is stored at:
```
~/.local/share/sioyek/shared.db
```
**Never delete this file** unless you want to erase all your annotations and reading history across all documents.

---

## 6. Missing Submodules or Build Failures {#sec-missing-submodules}

### Error: `mupdf/Makefile: No such file or directory`
Sioyek vendors a pinned version of MuPDF 1.26.11 as a git submodule:
```sh
git submodule update --init --recursive
```

### Error: `Do not run build_linux.sh`
`build_linux.sh` is a legacy QMake build script that is deprecated. Use CMake + Ninja:
```sh
scripts/dev-build.sh
# or:
mise run build
```
