# PDFMaestro — Usage Guide

## Opening a Document

- **Toolbar:** Click **Open** or press `Ctrl+O`.
- **Drag and drop:** Drag a PDF file from your file manager onto the PDFMaestro window.
- **Command line:** `pdfmaestro /path/to/document.pdf`

PDFMaestro remembers recently opened files and shows them on the welcome screen.

---

## Navigating Pages

| Action | Keyboard | Mouse |
|---|---|---|
| Next page | `→` or `Page Down` | Scroll down |
| Previous page | `←` or `Page Up` | Scroll up |
| Jump to page | Type page number in status bar field | Click page in Pages panel |
| First page | `Home` | — |
| Last page | `End` | — |

---

## Zoom

| Action | Shortcut |
|---|---|
| Zoom in | `Ctrl++` |
| Zoom out | `Ctrl+-` |
| Fit to width | `Ctrl+W` |
| Fit whole page | `Ctrl+Shift+W` |
| Reset to 100% | `Ctrl+0` |

You can also type a percentage directly into the zoom field in the toolbar.

---

## The Pages Panel (Left Sidebar)

The **Pages** tab shows a scrollable thumbnail grid of all pages in the document.

### Reordering pages

Click and drag any thumbnail to a new position. The page order in the document updates immediately. Press `Ctrl+S` to save.

### Right-click menu on a page thumbnail

| Option | What it does |
|---|---|
| Rotate 90° CW / CCW | Rotates just that page |
| Delete page | Removes the page permanently |
| Extract page | Saves that page as a new PDF |
| Insert blank page | Inserts an empty page before or after |

---

## Merging PDFs

1. Open the first PDF.
2. Go to **File > Merge PDFs...** or click **Merge** in the toolbar.
3. Select one or more additional PDF files.
4. Choose where to insert them: beginning, end, or after a specific page.
5. Press `Ctrl+S` to save.

---

## Splitting a PDF

1. Open the PDF you want to split.
2. Go to **File > Split PDF...** or click **Split** in the toolbar.
3. Enter the page ranges for each output file (e.g. `1-3`, `4-7`, `8-`).
4. Choose the output folder and file names.
5. Click **Split**.

---

## Cropping Pages

1. Click **Crop** in the toolbar.
2. Drag a selection box on the page to define the new crop boundary.
3. Optionally apply the same crop to all pages or a range.
4. Click **Apply Crop**. Press `Ctrl+S` to save.

---

## Rotating Pages

- Right-click any page thumbnail and choose **Rotate 90° CW/CCW**.
- Or go to **Tools > Rotate Pages...** to rotate a range of pages at once.

---

## Inserting a Signature

1. Click **Sign** in the toolbar or press `Ctrl+Shift+G`.
2. The Signature dialog opens with three tabs:

   **Type** — choose a font style and colour, then type your name. A preview updates live.

   **Draw** — draw freehand with your mouse or stylus on the canvas.

   **Upload** — load a photo of your handwritten signature. PDFMaestro automatically removes the white background (true PDF transparency — no white box on coloured pages).

3. Select a placement position (9-point grid, default: bottom-right).
4. Click **Insert**. The signature is placed on the current page.
5. Drag to reposition if needed. Press `Ctrl+S` to save.

---

## Annotations

Select an annotation tool from the **Annotate** menu or toolbar:

| Tool | How to use |
|---|---|
| Highlight | Click and drag over text |
| Underline | Click and drag over text |
| Strikethrough | Click and drag over text |
| Sticky note | Click anywhere on the page |
| Freehand ink | Click and draw |
| Stamp | Click to place (choose type from menu) |
| Redact | Drag over sensitive content, then confirm |

All annotations are saved as standard PDF annotations, visible in Okular, Adobe Reader, and other PDF viewers.

### Editing annotations

- **Move:** Click and drag the annotation.
- **Delete:** Right-click > Delete, or select and press `Delete`.
- **Change colour:** Right-click > Properties.

---

## Text Search

Press `Ctrl+F` to open the search bar at the bottom of the viewer.

- Type your search term and press `Enter`.
- Matches are highlighted in yellow across all pages.
- Press `F3` or click the arrow to jump to the next match.
- Press `Shift+F3` for the previous match.
- Press `Escape` to close the search bar.

---

## Bookmarks / Table of Contents

The **Bookmarks** tab in the left panel shows the document's built-in table of contents (if it has one). Click any entry to jump directly to that page.

To add your own bookmark: right-click a page thumbnail > **Add Bookmark**, or press `Ctrl+B` on the current page.

---

## Presentation Mode

Press `F5` to enter full-screen presentation mode.

- `→` / `Space` — next slide
- `←` / `Backspace` — previous slide
- `Escape` — exit

---

## Saving

| Action | Shortcut |
|---|---|
| Save (overwrite original) | `Ctrl+S` |
| Save As (new file) | `Ctrl+Shift+S` |

---

## Document Properties

Go to **Tools > Document Properties** to view and edit the PDF metadata: title, author, subject, keywords, and creation date.

---

## Full Keyboard Shortcut Reference

| Action | Shortcut |
|---|---|
| Open | `Ctrl+O` |
| Save | `Ctrl+S` |
| Save As | `Ctrl+Shift+S` |
| Merge PDFs | — |
| Split PDF | — |
| Quit | `Ctrl+Q` |
| Next page | `→` / `Page Down` |
| Previous page | `←` / `Page Up` |
| First page | `Home` |
| Last page | `End` |
| Zoom in | `Ctrl++` |
| Zoom out | `Ctrl+-` |
| Fit width | `Ctrl+W` |
| Fit page | `Ctrl+Shift+W` |
| 100% zoom | `Ctrl+0` |
| Full screen | `F11` |
| Presentation mode | `F5` |
| Search | `Ctrl+F` |
| Next search match | `F3` |
| Previous match | `Shift+F3` |
| Insert signature | `Ctrl+Shift+G` |
| Add bookmark | `Ctrl+B` |
