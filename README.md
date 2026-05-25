# PDFMaestro

**The unified PDF powerhouse.** View, annotate, rearrange, sign, merge, split, and crop PDF documents — all in one app.

PDFMaestro combines the deep document viewing of [Okular](https://okular.kde.org) with the page manipulation engine of [pdfarranger](https://github.com/pdfarranger/pdfarranger), rebuilt as a single Python/PySide6 desktop application.

![License](https://img.shields.io/badge/license-GPL--3.0-blue)
![Python](https://img.shields.io/badge/python-3.10%2B-blue)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![Status](https://img.shields.io/badge/status-alpha-orange)

---

## Features

### Viewing
- Full-quality PDF rendering via pypdfium2
- Continuous scroll, single page, and two-page modes
- Zoom: fit-width, fit-page, percentage, keyboard shortcuts
- Table of contents (bookmarks) panel
- Full-text search with highlighted matches across all pages
- Form field support (fill, sign, submit)
- Presentation / full-screen slideshow mode

### Page Management
- Interactive thumbnail grid — drag and drop pages to reorder
- Rotate, delete, and extract individual pages
- Crop pages with a visual overlay
- Merge multiple PDFs into one
- Split a PDF into separate files by page range

### Annotation
- Highlight, underline, and strikethrough text
- Sticky notes (pop-up annotations)
- Freehand ink drawing
- Stamps (approved, draft, confidential, etc.)
- Redaction (permanently remove sensitive content)
- All annotations saved as standard PDF annotations — readable in any viewer

### Signature
- **Type** your name in multiple signature font styles
- **Draw** freehand with mouse or stylus
- **Upload** a photo of your handwritten signature
- True PDF transparency via pikepdf SMask — no white box on any background colour
- Nine placement positions with preview

### Document Tools
- Edit document metadata (title, author, subject, keywords)
- Password-protect or unlock encrypted PDFs
- Compare two PDFs side by side
- Export pages as images (PNG, JPG)
- Page size and dimension info in status bar

---

## Screenshots

> Screenshots will be added once the UI is complete.

---

## Installation

### One-liner (Ubuntu / Debian / Linux Mint)

```bash
sudo apt-get install -y python3-pip python3-pyside6.qtwidgets libpoppler-dev && pip install --user pdfmaestro
```

### One-liner (Arch Linux)

```bash
sudo pacman -S python-pyside6 && pip install --user pdfmaestro
```

### One-liner (Fedora)

```bash
sudo dnf install -y python3-pyside6 && pip install --user pdfmaestro
```

For detailed installation instructions, virtual environment setup, and building from source, see **[docs/INSTALL.md](docs/INSTALL.md)**.

---

## Quick Start

```bash
# Run after install
pdfmaestro

# Open a file directly
pdfmaestro /path/to/document.pdf
```

1. Click **Open** or drag a PDF onto the window.
2. Use the **Pages** panel on the left to rearrange pages by dragging.
3. Use the **Toolbar** to merge, split, annotate, or sign.
4. Press **Ctrl+S** to save changes back to the PDF.

For a full guide see **[docs/USAGE.md](docs/USAGE.md)**.

---

## Keyboard Shortcuts

| Action | Shortcut |
|---|---|
| Open file | `Ctrl+O` |
| Save | `Ctrl+S` |
| Next page | `→` / `Page Down` |
| Previous page | `←` / `Page Up` |
| Zoom in / out | `Ctrl++` / `Ctrl+-` |
| Fit width | `Ctrl+W` |
| Fit page | `Ctrl+Shift+W` |
| Full screen | `F11` |
| Presentation mode | `F5` |
| Search | `Ctrl+F` |
| Insert signature | `Ctrl+Shift+G` |
| Quit | `Ctrl+Q` |

---

## Project Structure

```
pdfmaestro/
├── __main__.py        entry point
├── main_window.py     main window, menus, toolbar
├── viewer.py          PDF rendering (pypdfium2)
├── page_manager.py    thumbnail panel, drag-and-drop
├── operations.py      merge, split, crop, rotate
├── signature.py       type / draw / upload signature dialog
├── annotation.py      annotation engine + pikepdf save
├── search.py          full-text search
└── config.py          persistent settings

data/
└── icons/
    └── pdfmaestro.svg  app icon
```

---

## Development Roadmap

- [x] Phase 1 — Project scaffold, icon, dependency spec
- [ ] Phase 2 — Core PDF viewer (render, scroll, zoom, keyboard nav)
- [ ] Phase 3 — Page Manager panel (thumbnail grid, drag-to-reorder)
- [ ] Phase 4 — Merge / Split / Crop operations
- [ ] Phase 5 — Signature dialog (Qt port)
- [ ] Phase 6 — Full annotation engine
- [ ] Phase 7 — Text search
- [ ] Phase 8 — TOC / Bookmarks panel
- [ ] Phase 9 — Polish (dark mode, settings, welcome screen, OCR stub)

---

## Built On

- [pypdfium2](https://github.com/pypdfium2-team/pypdfium2) — PDF rendering
- [pikepdf](https://github.com/pikepdf/pikepdf) — PDF manipulation
- [PySide6](https://doc.qt.io/qtforpython/) — Qt6 UI framework
- [Pillow](https://python-pillow.org/) — image processing

---

## Credits and Acknowledgements

PDFMaestro would not exist without the foundational work of two outstanding open-source projects and their communities:

### Okular
**[Okular](https://okular.kde.org)** is the universal document viewer developed by the **KDE community**.
The viewer architecture, annotation model, multi-format rendering philosophy, and presentation mode in PDFMaestro are directly inspired by Okular's design. Okular is maintained by dozens of contributors across the KDE project — full credit and thanks to everyone who built it.
- Source: [invent.kde.org/graphics/okular](https://invent.kde.org/graphics/okular)
- License: GPL-2.0-or-later

### pdfarranger
**[pdfarranger](https://github.com/pdfarranger/pdfarranger)** was originally created by **Konstantinos Poulios** and has been maintained and extended by **Jérôme Robert** and a wide community of contributors since 2018.
The page rearrangement engine, merge/split/crop operations, and the pikepdf-based export pipeline in PDFMaestro are modelled on pdfarranger's architecture. The signature insertion feature (type, draw, upload with true PDF transparency via pikepdf SMask) was built on top of pdfarranger by **Rincol Tech Solutions Ltd** and is included here in its Qt-ported form.
- Source: [github.com/pdfarranger/pdfarranger](https://github.com/pdfarranger/pdfarranger)
- License: GPL-3.0-or-later

---

PDFMaestro is an independent project and is not affiliated with, endorsed by, or a fork of either Okular or pdfarranger. It is a new application that draws inspiration and architectural ideas from both.

---

## Contributing

Pull requests are welcome. Please read [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) first.

---

## License

PDFMaestro is free software, released under the **GNU General Public License v3.0 or later**.
See [COPYING](COPYING) for the full text.

© 2026 Rincol Tech Solutions Ltd
