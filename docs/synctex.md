# ⚡ LaTeX & SyncTeX Integration Guide

Sioyek provides first-class support for **SyncTeX**, enabling seamless bidirectional navigation between your LaTeX source code and the compiled PDF.

---

## 🔄 How SyncTeX Works

```
           Forward Search: Editor cursor line ──> Highlights corresponding line in PDF
      ┌──────────────────────────┐             ┌──────────────────────────┐
      │     LaTeX Source         │             │       Sioyek PDF         │
      │   (Neovim / VS Code)     │             │    (Vulkan Renderer)     │
      └──────────────────────────┘             └──────────────────────────┘
           Inverse Search: Right-Click in PDF ──> Jumps to source line in editor
```

1. **Forward Search**: While writing LaTeX in your editor, execute your editor's jump command to highlight the exact matching sentence in Sioyek.
2. **Inverse Search**: While reading the PDF in Sioyek, right-click on any paragraph (or press `F4` to toggle SyncTeX mode and click) to automatically focus your editor and place your cursor on the corresponding `.tex` line.

---

## 🛠️ Configuring Inverse Search in Sioyek

To tell Sioyek how to communicate back to your editor, add `inverse_search_command` to your `~/.config/sioyek/prefs_user.config`:

```ini
# %1 expands to the full path of the source .tex file
# %2 expands to the target line number
inverse_search_command    <your_editor_command>
```

Choose your editor from the configurations below.

---

## 💻 Editor Configurations

### 1. Neovim with `vimtex`

#### In `~/.config/sioyek/prefs_user.config`:
```ini
inverse_search_command    nvr --remote-silent +%2 "%1"
```
*(Requires `neovim-remote` / `pip install neovim-remote` so Sioyek connects to your running Neovim instance instead of opening a duplicate).*

#### In Neovim configuration (`~/.config/nvim/after/ftplugin/tex.lua` or `init.lua`):
```lua
vim.g.vimtex_view_method = 'sioyek'
vim.g.vimtex_view_sioyek_exe = 'sioyek'
```

Or for traditional Vim script (`~/.vim/ftplugin/tex.vim`):
```vim
let g:vimtex_view_method = 'sioyek'
let g:vimtex_view_sioyek_exe = 'sioyek'
```

---

### 2. Visual Studio Code with LaTeX Workshop

#### In `~/.config/sioyek/prefs_user.config`:
```ini
inverse_search_command    code -r -g "%1:%2"
```

#### In VS Code `settings.json`:
```json
{
  "latex-workshop.view.pdf.viewer": "external",
  "latex-workshop.view.pdf.external.viewer.command": "sioyek",
  "latex-workshop.view.pdf.external.viewer.args": [
    "--reuse-window",
    "--forward-search-file",
    "%DIR%/%DOCFILE%.tex",
    "--forward-search-line",
    "%LINE%",
    "%OUTDIR%/%DOCFILE%.pdf"
  ],
  "latex-workshop.view.pdf.external.synctex.command": "sioyek",
  "latex-workshop.view.pdf.external.synctex.args": [
    "--reuse-window",
    "--forward-search-file",
    "%TEX%",
    "--forward-search-line",
    "%LINE%",
    "%PDF%"
  ]
}
```

---

### 3. Emacs with AUCTeX

#### In `~/.config/sioyek/prefs_user.config`:
```ini
inverse_search_command    emacsclient --no-wait +%2 "%1"
```

#### In Emacs configuration (`init.el`):
```elisp
(with-eval-after-load 'tex
  (add-to-list 'TeX-view-program-list
               '("Sioyek" ("sioyek --reuse-window --forward-search-file %b --forward-search-line %n %o")))
  (setq TeX-view-program-selection
        '((output-pdf "Sioyek"))))
```

---

### 4. Helix Editor

#### In `~/.config/sioyek/prefs_user.config`:
```ini
inverse_search_command    wezterm cli spawn -- hx "%1":%2
```
*(Or replace `wezterm cli spawn` with your terminal emulator's command, such as `kitty -e hx "%1":%2` or `alacritty -e hx "%1":%2`).*

---

## 🎯 Command Line Forward Search

You can also trigger a forward search directly from any script, terminal, or build system:

```sh
sioyek \
  --reuse-window \
  --forward-search-file "paper.tex" \
  --forward-search-line 142 \
  "paper.pdf"
```

If Sioyek is already open with `paper.pdf`, it smoothly pans and flashes a high-visibility highlight rect on line 142.
