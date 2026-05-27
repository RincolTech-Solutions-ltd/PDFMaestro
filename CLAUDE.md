# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project

PDFMaestro is a C++17 desktop PDF tool built with **Qt6 Widgets**, **Poppler-Qt6** (rendering), and **QPDF** (manipulation). It is the active codebase — the legacy Python/PySide6 version in `pdfmaestro/` is obsolete.

Active branch: `feature/cpp-rewrite`. `main` still holds the old Python code.

---

## Running

```bash
pdfmaestro              # open with no file
pdfmaestro file.pdf     # open a specific PDF
```

The binary lives at `/usr/local/bin/pdfmaestro` after install — no path setup needed. Just type `pdfmaestro` in any terminal.

Convenience script (also handles "not installed" gracefully):
```bash
bash "/media/genius/New Volume/Engineering/Programming/Scripts/run_pdfmaestro.sh" [file.pdf]
```

---

## Build & Install

```bash
# Full clean build + install (runs apt deps, cmake, ninja, installs binary)
sudo bash "/media/genius/New Volume/Engineering/Programming/Scripts/update_pdfmaestro.sh"

# Manual build steps
mkdir -p build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build --parallel $(nproc)
sudo cmake --install build
```

Build dependencies: `cmake ninja-build pkg-config qt6-base-dev qt6-tools-dev libpoppler-qt6-dev libqpdf-dev`

KDE Frameworks 6 (`extra-cmake-modules`, `libkf6*-dev`) are optional — CMake detects and uses them if present, otherwise falls back to plain Qt6.

---

## Architecture

### Data flow

```
File on disk
  └─► QPDF::processFile()        ← all mutations happen here (in memory)
  └─► QFile → QByteArray
        └─► Poppler::Document::loadFromData()  ← rendering only
              └─► PdfViewer (QGraphicsView)
```

After any QPDF mutation (rotate, delete, annotate, merge, etc.), call `Operations::toBytes(m_qpdf)` and reload Poppler via `PdfViewer::loadFromBytes()`. There are no temp files — the entire pipeline is in-memory.

### Key separation of concerns

| Class / Namespace | Role |
|---|---|
| `Operations` | All QPDF page manipulation (order, delete, rotate, crop, merge, split, signature overlay). Pure functions — take `QPDF&`, return results. |
| `Annotations` | QPDF-native annotation writer (highlight quads, sticky note, ink, stamp, redact/apply). |
| `PdfViewer` | `QGraphicsView` subclass. Renders via Poppler. Owns `AnnotationOverlay` on its viewport. Emits `annotationCommitted(QVariantMap)`. |
| `AnnotationOverlay` | Transparent `QWidget` over the viewport. Captures mouse events, draws live feedback, emits `annotationCommitted`. Passes through mouse events when tool == `"pointer"` via `WA_TransparentForMouseEvents`. |
| `PageManager` | `QListWidget` in icon mode. Drag-to-reorder emits `orderChanged(QVector<int>)`. Context menu emits `pageRotated`/`pageDeleted`. |
| `SearchBar` | Uses `Poppler::Page::search()`. Emits `matchSelected(pageIdx, QRectF)`. |
| `MainWindow` | Wires everything. Owns the `QPDF m_qpdf` instance. Receives all signals and dispatches to `Operations`/`Annotations`, then calls `reloadFromQpdf()`. |
| `Config` | `QObject` wrapping `QSettings("RincolTech", "PDFMaestro")`. Includes `coerceList()` fix — `QSettings` returns bare `QString` (not `QStringList`) when only one item is stored. |

### Annotation coordinate system

`AnnotationOverlay::toPdf(QPointF)` converts widget pixel coordinates to PDF user-space points (origin bottom-left, Y-axis up). `setPageContext()` must be called after every scroll, zoom, or page change — `PdfViewer` calls it from `pushPageContext()`.

### QPDF stream data

`Buffer::getBuffer()` returns `unsigned char*`. Use the `bufToStr` / `streamToStr` helpers in `operations.cpp` — never call `->str()` directly on a `Buffer` (it doesn't exist in QPDF 11).

`QPDF` is non-copyable. Functions that need to produce a new document use `std::shared_ptr<QPDF>` (see `extractPages` in `operations.cpp`).

---

## Branch & Commit conventions

- Work on `feature/<name>` or `fix/<name>` branches. Never commit directly to `main`.
- Commit messages: `feat:`, `fix:`, `refactor:`, `docs:`, `chore:`
- After fixing a build error: commit + push, then run the update script to retest.

---

## Known platform notes (Ubuntu 24.04)

- `libpoppler-qt6-dev` ships no `PopplerConfig.cmake` — use `pkg_check_modules(POPPLER_QT6 REQUIRED poppler-qt6)`, not `find_package(Poppler)`.
- `Poppler::Document::toc()` was removed in Poppler 24.x — use `doc->outline()` which returns `QVector<Poppler::OutlineItem>`.
- Add `-DPOINTERHOLDER_TRANSITION=4` to suppress the QPDF `PointerHolder.hh` warning.
- The Qt6 `QMenu::addAction(text, obj, slot, shortcut)` overload is deprecated — use explicit `QAction` construction with `setShortcut` + `connect`.
