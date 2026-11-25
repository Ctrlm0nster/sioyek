# ⌨️ Sioyek Keybindings & Command Reference

Sioyek uses modal, Vim-inspired keyboard shortcuts designed to let you navigate dense documents rapidly without touching your mouse.

---

## 📑 Quick Navigation

- [Movement & Navigation](#movement--navigation)
- [Search & Discovery](#search--discovery)
- [Smart Jump & Definitions](#smart-jump--definitions)
- [Portals & Overviews](#portals--overviews)
- [Marks & Bookmarks](#marks--bookmarks)
- [Highlights & Annotations](#highlights--annotations)
- [Visual Ruler & Focus Aids](#visual-ruler--focus-aids)
- [Zoom & Display Controls](#zoom--display-controls)
- [Window & Session Management](#window--session-management)
- [Command Palette & Extensions](#command-palette--extensions)

---

## Movement & Navigation {#movement--navigation}

| Keybinding | Command | Description |
| :--- | :--- | :--- |
| `j` / `Down` | `move_down` | Move view downward by a configured distance |
| `k` / `Up` | `move_up` | Move view upward by a configured distance |
| `h` / `Left` | `move_left` | Pan view left |
| `l` / `Right` | `move_right` | Pan view right |
| `Space` / `PageDown` | `screen_down` | Scroll down by half-screen ratio (avoids losing reading context) |
| `Shift+Space` / `PageUp` | `screen_up` | Scroll up by half-screen ratio |
| `gg` | `goto_beginning` | Jump to the very beginning of the document (or `[N]gg` to page `N`) |
| `G` / `End` | `goto_end` | Jump to the end of the document |
| `Home` | `goto_page_with_page_number` | Open prompt to jump directly to a page number |
| `gc` | `next_chapter` | Jump forward to the start of the next chapter |
| `gC` | `prev_chapter` | Jump backward to the start of the previous chapter |
| `t` | `goto_toc` | Open searchable Table of Contents with fuzzy finding |
| `Backspace` / `Ctrl+Left` | `prev_state` | Jump backward in navigation history (like `Ctrl+O` in Vim) |
| `Shift+Backspace` / `Ctrl+Right` | `next_state` | Jump forward in navigation history (like `Ctrl+I` in Vim) |
| `^` | `goto_left_smart` | Snap left margin to text boundary (ignoring blank page borders) |
| `$` | `goto_right_smart` | Snap right margin to text boundary |
| `zz` | `goto_top_of_page;goto_right_smart` | Jump to top-right of page (ideal for reading 2-column papers) |

> [!TIP]
> **Numeric Prefixes**: Many commands accept numeric counts!
> - `42gg` jumps straight to page 42.
> - `5Space` scrolls down 5 screen widths.
> - `10j` scrolls down 10 vertical increments.

---

## Search & Discovery {#search--discovery}

| Keybinding | Command | Description |
| :--- | :--- | :--- |
| `/` or `Ctrl+F` | `search` | Open search prompt for fuzzy or exact text search |
| `c/` or `c<Ctrl+F>` | `chapter_search` | Search exclusively within the current chapter |
| `n` | `next_item` | Jump to next search occurrence |
| `N` | `previous_item` | Jump to previous search occurrence |
| `:` then `regex_search` | `regex_search` | Perform regular expression search |

> [!NOTE]
> **Page Range Searching**: You can prefix your query with a page range in `/`:
> - `/<10,25>convolution` will search for "convolution" only between pages 10 and 25.

---

## Smart Jump & Definitions {#smart-jump--definitions}

Smart Jump lets you navigate references, figures, citations, and equations **even if the author didn't add hyperlinks**.

| Keybinding / Action | Command | Description |
| :--- | :--- | :--- |
| `Shift+F` | `keyboard_smart_jump` | Trigger keyboard-based Smart Jump to references on screen |
| `f` | `open_link` | Display letter badges on PDF links to open with keyboard |
| **Middle-Click** | `external_search` | Search clicked citation or word on Google Scholar |
| **Shift+Middle-Click** | `external_search` | Search clicked citation or book on LibGen |
| `s` | `external_search` | Search currently selected text with default search engine |
| `Ctrl+]` | `goto_definition` | Jump to the definition or figure named in the active visual ruler line |

---

## Portals & Overviews {#portals--overviews}

Portals link two distant locations together so you can read main text while seeing a figure or table 30 pages away.

| Keybinding | Command | Description |
| :--- | :--- | :--- |
| `p` | `portal` | Step 1: Set portal source; Step 2: Jump to destination and press `p` to link |
| `Tab` / `gp` | `goto_portal` | Toggle view to the destination of the nearest portal |
| `Shift+Tab` / `P` | `edit_portal` | Edit or retarget the nearest portal |
| `dp` | `delete_portal` | Delete the nearest portal |
| `F12` | `toggle_window_configuration` | Toggle side-by-side portal helper window |
| `l` | `overview_definition` | Open small floating overview of the figure/definition under ruler |
| **Right-Click** | Context Overview | Inspect referenced equation, theorem, or citation without leaving your line |

---

## Marks & Bookmarks {#marks--bookmarks}

Vim-style marks give you lightning-fast position anchors.

| Keybinding | Command | Description |
| :--- | :--- | :--- |
| `m[a-z]` | `set_mark` | Set a **document-local mark** (e.g. `ma` marks position as `a`) |
| `'[a-z]` or `` `[a-z] `` | `goto_mark` | Jump back to local mark `a` in the current document |
| `m[A-Z]` | `set_mark` | Set a **global mark** across your entire library |
| `'[A-Z]` or `` `[A-Z] `` | `goto_mark` | Jump to global mark `A` (opens the document if not open) |
| `b` | `add_bookmark` | Add a named text bookmark at the current location |
| `db` | `delete_bookmark` | Delete current bookmark |
| `gb` | `goto_bookmark` | Open fuzzy list of bookmarks in current document |
| `gB` | `goto_bookmark_g` | Search across bookmarks from **all documents** in your library |

---

## Highlights & Annotations {#highlights--annotations}

| Keybinding | Command | Description |
| :--- | :--- | :--- |
| `h` + `[a-z]` | `add_highlight` | Highlight selected text with category letter `a` through `z` |
| `dh` | `delete_highlight` | Click on a highlight and press `dh` to delete it |
| `gh` | `goto_highlight` | Fuzzy search and jump through highlights in current file |
| `gH` | `goto_highlight_g` | Search through all highlights across your whole library |
| `gnh` | `goto_next_highlight` | Jump to next highlight in document |
| `gNh` | `goto_prev_highlight` | Jump to previous highlight in document |
| `:` then `embed_annotations` | `embed_annotations` | Bake annotations directly into a standalone PDF file |

---

## Visual Ruler & Focus Aids {#visual-ruler--focus-aids}

The visual ruler is an indispensable reading guide for dense, multi-column academic text.

| Keybinding | Command | Description |
| :--- | :--- | :--- |
| **Right-Click & Hold** | Visual Mark | Displays the horizontal highlight ruler across the active line |
| `F7` | `toggle_visual_scroll` | Toggle visual scroll mode (locks ruler and routes wheel/j/k to ruler) |
| `j` (in ruler mode) | `move_visual_mark_down` | Move the focus ruler down one line |
| `k` (in ruler mode) | `move_visual_mark_up` | Move the focus ruler up one line |
| `l` (in ruler mode) | `overview_definition` | Pop open overview window for reference on current ruler line |
| `]` (in ruler mode) | `portal_to_definition` | Open a portal to reference on current ruler line |

---

## Zoom & Display Controls {#zoom--display-controls}

| Keybinding | Command | Description |
| :--- | :--- | :--- |
| `+` / `=` | `zoom_in` | Zoom in |
| `-` | `zoom_out` | Zoom out |
| `F9` | `fit_to_page_width` | Fit page width to screen |
| `F10` | `fit_to_page_width_smart` | Fit page width ignoring margins (maximizes readability) |
| `F8` | `toggle_dark_mode` | Toggle inverted dark mode |
| `r` | `rotate_clockwise` | Rotate document 90° clockwise |
| `R` | `rotate_counterclockwise` | Rotate document 90° counter-clockwise |
| `F1` | `toggle_highlight` | Toggle visibility of link highlight boxes |
| `F5` | `toggle_presentation_mode` | Toggle presentation mode (fits page, arrow keys advance slides) |
| `F11` | `toggle_fullscreen` | Toggle borderless fullscreen |

---

## Window & Session Management {#window--session-management}

| Keybinding | Command | Description |
| :--- | :--- | :--- |
| `o` | `open_document` | Open system file picker |
| `Ctrl+o` | `open_document_embedded` | Open Sioyek's built-in fast fuzzy file browser |
| `Ctrl+Shift+O` | `open_document_embedded_from_current_path` | Open fuzzy browser in current document directory |
| `O` | `open_prev_doc` | Search and reopen recently opened documents |
| `Ctrl+T` | `new_window` | Open a new Sioyek window |
| `Ctrl+W` | `close_window` | Close current Sioyek window |
| `q` | `quit` | Exit Sioyek |

---

## Command Palette & Extensions {#command-palette--extensions}

| Keybinding | Action |
| :--- | :--- |
| `:` | Open interactive command palette with tab-completion for all internal commands |
| `F4` | Toggle SyncTeX mode (for LaTeX editor bidirectional sync) |
| `F6` | Toggle mouse drag mode (pans page instead of selecting text) |
