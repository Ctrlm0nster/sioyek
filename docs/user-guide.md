# 📖 Sioyek User Guide & Core Workflows

Welcome to the **Sioyek User Guide**. Unlike traditional PDF viewers designed for reading casual e-books or signing contracts, Sioyek is purpose-built from the ground up for reading **research papers, technical textbooks, mathematics dissertations, and engineering specifications**.

This guide covers real-world reading workflows and explains how to harness Sioyek's superpowers.

---

## 📑 Contents

1. [The Philosophy of Sioyek](#sec-philosophy)
2. [Everyday Reading Workflow](#sec-workflow)
3. [Mastering Portals: Solving the "20 Pages Later" Problem](#sec-portals)
4. [Smart Jump & Deep Citations](#sec-smart-jump)
5. [The Overview Window](#sec-overview)
6. [Visual Ruler & Visual Scroll: Eliminating Eye Strain](#sec-visual-ruler)
7. [Marks & Bookmarks: Your Reading Memory](#sec-marks)
8. [Semantic Highlighting & PDF Export](#sec-highlighting)
9. [Presentations & Dual Monitor Setups](#sec-presentations)

---

## 1. The Philosophy of Sioyek {#sec-philosophy}

When reading a dense academic paper or textbook, your brain is forced to context-switch constantly:
* "See Figure 4 on page 19" (while you are on page 3).
* "As shown by Smith et al. [23]..." (who was that again?).
* "Recall Equation (3.12)..." (where was chapter 3?).

Traditional PDF readers make you scroll away, lose your reading spot, search through endless pages, and struggle to find your way back.

**Sioyek solves this through non-destructive navigation:**
- You never lose your reading location.
- References can be previewed without moving away.
- Distant sections can be linked side-by-side using **Portals**.
- Every action is controllable with simple, muscle-memory keystrokes.

---

## 2. Everyday Reading Workflow {#sec-workflow}

### Opening and Finding Documents
- Press `o` to pick a file using your desktop file dialog.
- Press `Ctrl+O` to use the lightning-fast fuzzy finder rooted in your current directory.
- Press `Shift+O` (`O`) to view and reopen your recent reading history.

### Fast Navigation
- **Scrolling**: Use `j` and `k` or your arrow keys for fine movements.
- **Reading by Screen**: Press `Space` to jump down half a screen, and `Shift+Space` to jump up. This guarantees you never lose the sentence you were reading.
- **Smart Width Fit (`F10`)**: Standard PDF viewers fit the whole page, including 2-inch white margins. Press `F10` (`fit_to_page_width_smart`) and Sioyek crops out the blank margins automatically, making text significantly larger and sharper.
- **Two-Column Papers (`zz`)**: When reading a two-column layout, press `zz` to instantly snap the viewport to the top of the right-hand column.

---

## 3. Mastering Portals: Solving the "20 Pages Later" Problem {#sec-portals}

Portals are Sioyek's most iconic feature. A **Portal** creates a persistent visual link between two different locations in a document (or even between two different documents).

```
   ┌──────────────────────────┐         ┌──────────────────────────┐
   │ Main Reading Window      │         │ Portal Destination Window│
   │ (e.g. Page 4)            │         │ (e.g. Page 28: Figure 7) │
   │                          │         │                          │
   │ "...as illustrated in   │         │ ┌──────────────────────┐ │
   │  Figure 7, the loss..."  │ =======>│ │   [FIGURE 7 GRAPH]   │ │
   │                          │ (Linked)│ └──────────────────────┘ │
   │ Text continues here...   │         │ Caption explanation...   │
   └──────────────────────────┘         └──────────────────────────┘
```

### How to Create a Portal
1. Navigate to the place where the reference is made (e.g., page 4 mentioning "Figure 7").
2. Press **`p`**. Sioyek enters *portal creation mode* and remembers this source position.
3. Scroll or jump to the referenced figure (e.g., page 28).
4. Press **`p`** again. The portal is now created!

### Interacting with Portals
* Press **`Tab`** to jump between the source sentence and the destination figure.
* Press **`Backspace`** to return immediately.
* Press **`F12`** (`toggle_window_configuration`) to open a side-by-side helper window. The helper window automatically displays the destination of any portal near your current cursor!
* Press **`dp`** (`delete_portal`) to remove the portal when finished.

---

## 4. Smart Jump & Deep Citations {#sec-smart-jump}

Most legacy PDFs (especially older papers or pre-prints) do not contain clickable hyperlinks for citations or figures. Sioyek uses OCR-aware layout heuristics to identify and jump to them anyway:

1. **Keyboard Smart Jump (`Shift+F`)**:
   Press `Shift+F` (`F`). Sioyek scans the current page for citations, figures, and section headers, labeling each with an interactive letter badge. Type the letter to jump directly to that figure or bibliography entry.

2. **Google Scholar & LibGen One-Click Lookup**:
   * **Middle-Click** on any paper title or citation author to open a Google Scholar search in your browser.
   * **Shift+Middle-Click** on a book reference to search Library Genesis.
   * Highlight any text and press `s` to perform an external search.

---

## 5. The Overview Window {#sec-overview}

Often you don't even want to jump; you just want to see what Equation (4) was before reading the next theorem.

- **Right-Click** on an equation number or figure reference in the text.
- Sioyek pops up a clean, non-disruptive **floating overview** showing the exact target snippet.
- Keep reading your current paragraph. As soon as you click or move away, the overview disappears.
- Your reading flow is never interrupted.

---

## 6. Visual Ruler & Visual Scroll: Eliminating Eye Strain {#sec-visual-ruler}

Dense two-column layouts make it easy to skip lines or lose your spot, especially on ultra-wide monitors.

### The Focus Ruler
- **Right-click and hold** on any line to draw a subtle, semi-transparent highlight bar across the line.
- Release to dismiss, or press `F7` to lock into **Visual Scroll Mode**.

### Visual Scroll Mode (`F7`)
Once locked into Visual Scroll:
- `j` and `k` move the focus bar down or up one line at a time.
- The page automatically scrolls to keep your focus line centered.
- Press `l` to open an overview of any reference found in that highlighted line.
- Press `]` to spawn a portal directly from that line.

---

## 7. Marks & Bookmarks: Your Reading Memory {#sec-marks}

Sioyek provides two complementary bookmarking systems:

### 1. Single-Key Vim Marks
- **Document-Local Marks**:
  - `ma` marks your current location as `a`.
  - `'a` jumps back to mark `a` from anywhere in this document.
  - Lowercase marks `[a-z]` are unique per document.
- **Global Marks**:
  - `mA` marks this location across your entire computer.
  - `'A` jumps back to `A` even if you are currently reading a completely different PDF! Sioyek automatically switches files and restores the exact scroll position.

### 2. Searchable Text Bookmarks
- Press **`b`** to create a named bookmark (e.g. "Proof of Lemma 3.2").
- Press **`gb`** to open a fuzzy-search list of all bookmarks in the current document.
- Press **`gB`** to search bookmarks across **all documents** in your entire library.

---

## 8. Semantic Highlighting & PDF Export {#sec-highlighting}

Sioyek supports 26 distinct highlight types (`a` through `z`), each customizable with its own color and semantic meaning:

```
  h + y  ──> Yellow: General interesting points
  h + r  ──> Red: Critiques / doubts / follow-up required
  h + g  ──> Green: Key results / empirical findings
  h + b  ──> Blue: Definitions and formal notation
```

### Managing Highlights
1. Select text with your mouse.
2. Press `h` followed by any letter (e.g., `hy` for yellow).
3. Jump between highlights using `gnh` (next highlight) and `gNh` (previous highlight).
4. Search all highlights using `gh` (current file) or `gH` (entire library).

### Exporting Annotations
By default, highlights and bookmarks are stored in Sioyek's high-performance SQLite database (`~/.local/share/sioyek/`), leaving your original PDF files pristine.

If you need to send your annotated document to a colleague using Acrobat or iPad:
1. Press `:` to open the command palette.
2. Type `embed_annotations` and press Enter.
3. Sioyek generates a standard PDF containing native PDF annotation layers compatible with all standard viewers.

---

## 9. Presentations & Dual Monitor Setups {#sec-presentations}

### Presentation Mode (`F5`)
Giving a talk from Beamer slides or PDF handouts?
- Press **`F5`** to toggle Presentation Mode.
- Pages automatically snap to full screen height and width.
- Movement keys (`j`, `k`, `Space`, arrow keys) flip exactly one slide at a time.
- Press `F11` for borderless fullscreen display.

### Dual Monitor Workflows
If you have a dual-monitor setup:
1. Set `should_use_multiple_monitors 1` in `prefs_user.config`.
2. Launch Sioyek.
3. The main reading pane opens on your primary monitor while the portal/helper window opens on your secondary screen.
4. As you navigate the main text, diagrams and references automatically stay pinned on your secondary display.
