"""
toc.py — Read the PDF document outline (table of contents) via pypdfium2.
No Qt imports. Returns a flat list of TocEntry with level/page info.
"""
from __future__ import annotations
from dataclasses import dataclass
import pypdfium2 as pdfium


@dataclass
class TocEntry:
    title:     str
    page_idx:  int        # 0-based; -1 if destination unknown
    level:     int        # 0 = top-level chapter


def read_toc(doc: pdfium.PdfDocument) -> list[TocEntry]:
    """
    Return the document outline as a flat list ordered by appearance.
    level encodes nesting depth (0 = root, 1 = child, 2 = grandchild …).
    """
    entries: list[TocEntry] = []
    try:
        for bookmark in doc.get_toc():
            title = bookmark.get_title() or "(untitled)"
            dest  = bookmark.get_dest()
            page_idx = dest.get_index() if dest is not None else -1
            entries.append(TocEntry(
                title    = title,
                page_idx = page_idx,
                level    = bookmark.level,
            ))
    except Exception:
        pass
    return entries
