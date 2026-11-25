# 🎨 Sioyek Configuration & Curated Themes

Sioyek is completely customizable via lightweight plain-text configuration files. You can customize colors, keybindings, fonts, external search engines, and multi-monitor behavior.

---

## 📁 File Locations

On Linux, user configurations are stored under standard XDG paths:

| File | Purpose | Location |
| :--- | :--- | :--- |
| **Preferences** | Visual appearance, colors, font sizes, behavior | `~/.config/sioyek/prefs_user.config` |
| **Keybindings** | Custom keyboard shortcuts and commands | `~/.config/sioyek/keys_user.config` |
| **Databases** | Local marks, bookmarks, portals, and highlight database | `~/.local/share/sioyek/` |

> [!NOTE]
> System defaults are stored in `/usr/share/sioyek/prefs.config` and `keys.config`. Never edit system files directly; always put overrides in your `_user.config` files. Sioyek automatically merges your user configuration on top of defaults.

---

## 🎨 Curated Color Themes

Copy any of the following theme snippets directly into your `~/.config/sioyek/prefs_user.config`.

### 🌿 1. Catppuccin Mocha (Modern Pastel Dark)

```ini
# Catppuccin Mocha Theme for Sioyek
background_color            0.118 0.118 0.180
dark_mode_background_color  0.118 0.118 0.180
dark_mode_contrast          0.85

text_highlight_color        0.976 0.886 0.686
visual_mark_color           0.345 0.357 0.443 0.35
search_highlight_color      0.651 0.890 0.631
link_highlight_color        0.537 0.706 0.980
synctex_highlight_color     0.961 0.761 0.906

custom_background_color     0.118 0.118 0.180
custom_text_color           0.804 0.839 0.957

status_bar_color            0.094 0.094 0.145
status_bar_text_color       0.804 0.839 0.957
```

### ❄️ 2. Nord (Arctic, North-Blush Clean)

```ini
# Nord Theme for Sioyek
background_color            0.180 0.204 0.251
dark_mode_background_color  0.180 0.204 0.251
dark_mode_contrast          0.82

text_highlight_color        0.922 0.796 0.545
visual_mark_color           0.263 0.298 0.369 0.35
search_highlight_color      0.639 0.745 0.549
link_highlight_color        0.533 0.753 0.816
synctex_highlight_color     0.706 0.557 0.678

custom_background_color     0.180 0.204 0.251
custom_text_color           0.847 0.871 0.914

status_bar_color            0.149 0.169 0.208
status_bar_text_color       0.847 0.871 0.914
```

### 🌃 3. Tokyo Night (Cyberpunk Elegance)

```ini
# Tokyo Night Theme for Sioyek
background_color            0.102 0.106 0.149
dark_mode_background_color  0.102 0.106 0.149
dark_mode_contrast          0.88

text_highlight_color        0.878 0.690 0.435
visual_mark_color           0.255 0.275 0.380 0.35
search_highlight_color      0.447 0.816 0.573
link_highlight_color        0.478 0.667 1.000
synctex_highlight_color     0.733 0.612 0.984

custom_background_color     0.102 0.106 0.149
custom_text_color           0.753 0.792 0.957

status_bar_color            0.086 0.090 0.125
status_bar_text_color       0.753 0.792 0.957
```

### 🌲 4. Gruvbox Dark (Warm & Retro)

```ini
# Gruvbox Dark Theme for Sioyek
background_color            0.157 0.157 0.157
dark_mode_background_color  0.157 0.157 0.157
dark_mode_contrast          0.85

text_highlight_color        0.980 0.741 0.184
visual_mark_color           0.314 0.286 0.271 0.35
search_highlight_color      0.722 0.733 0.149
link_highlight_color        0.514 0.647 0.596
synctex_highlight_color     0.827 0.525 0.608

custom_background_color     0.157 0.157 0.157
custom_text_color           0.922 0.859 0.698

status_bar_color            0.110 0.110 0.110
status_bar_text_color       0.922 0.859 0.698
```

### 🖤 5. Pure Black OLED (True Deep Black for High Battery Life)

```ini
# OLED Deep Black Theme for Sioyek
background_color            0.0 0.0 0.0
dark_mode_background_color  0.0 0.0 0.0
dark_mode_contrast          0.90

text_highlight_color        1.0 0.9 0.2
visual_mark_color           0.2 0.2 0.2 0.4
search_highlight_color      0.2 1.0 0.4
link_highlight_color        0.3 0.6 1.0
synctex_highlight_color     1.0 0.4 0.8

custom_background_color     0.0 0.0 0.0
custom_text_color           0.95 0.95 0.95

status_bar_color            0.05 0.05 0.05
status_bar_text_color       0.90 0.90 0.90
```

---

## 🛠️ Essential Preferences Reference

Add these options to `~/.config/sioyek/prefs_user.config`:

### Typography & Display
```ini
# Use your preferred desktop font for UI elements (menus, statusbar, hints)
ui_font                     Inter
font_size                   14
status_bar_font_size        13

# Increase MuPDF internal cache in MiB (default: 64, recommended for 16GB+ RAM: 256)
mupdf_store_mib             256

# Rerender overview window for razor-sharp equation previews
rerender_overview           1

# Zoom factor per step (default: 1.2)
zoom_inc_factor             1.15

# Zoom centered on mouse cursor rather than window center
wheel_zoom_on_cursor        1
```

### Startup & Window Behavior
```ini
# Auto-start with visual scroll and dark mode activated
startup_commands            toggle_visual_scroll;toggle_dark_mode

# Open documents in existing window or spawn new ones (0 = reuse, 1 = new)
should_launch_new_window    1

# Automatically launch portal helper window on secondary monitor if detected
should_use_multiple_monitors 0
```

### External Search Providers
You can configure one-click search engines accessed via keyboard shortcut `s` or middle-clicks:

```ini
# Define custom search URLs
search_url_s    https://scholar.google.com/scholar?q=
search_url_a    https://arxiv.org/search/?searchtype=all&source=header&query=
search_url_g    https://www.google.com/search?q=
search_url_l    https://libgen.is/search.php?req=
search_url_w    https://en.wikipedia.org/wiki/Special:Search?search=

# Default search engine for middle-click (s = Google Scholar)
middle_click_search_engine          s

# Default search engine for Shift+middle-click (l = LibGen)
shift_middle_click_search_engine    l
```

---

## ⌨️ Customizing Keybindings

Edit `~/.config/sioyek/keys_user.config` to remap any command:

```ini
# Map custom navigation
move_down       j
move_up         k
screen_down     d
screen_up       u

# Map easy toggle for dark mode
toggle_dark_mode    <C-d>

# Map fit to smart width
fit_to_page_width_smart  w

# Map close window to Esc or q
quit            q
```

---

## 🔌 Custom Extensions & Shell Commands

You can bind custom shell scripts to Sioyek commands:

```ini
# In ~/.config/sioyek/prefs_user.config:
# %{file_name} = full path to current document
# %{selected_text} = currently highlighted text
new_command _open_in_xopp  xournalpp %{file_name}
new_command _ocr_extract   python3 ~/.local/bin/sioyek-ocr.py %{file_name}

# In ~/.config/sioyek/keys_user.config:
_open_in_xopp   <C-e>
_ocr_extract    <C-x>
```
