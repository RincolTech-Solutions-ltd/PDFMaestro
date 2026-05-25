"""
search.py — Full-text search across a PDF via pypdfium2.
Returns match positions as (page_idx, [(left, bottom, right, top), ...]) tuples
in PDF user space (origin bottom-left), suitable for annotation-style highlighting.
No Qt imports — pure data layer.
"""
from __future__ import annotations
import pypdfium2 as pdfium


# ── Result type ───────────────────────────────────────────────────────────────

class SearchMatch:
    """One occurrence of the search term inside the document."""
    __slots__ = ("page_idx", "rects")

    def __init__(self, page_idx: int, rects: list[tuple[float, float, float, float]]):
        self.page_idx = page_idx
        self.rects    = rects   # each rect: (left, bottom, right, top) in PDF pts


# ── Search engine ─────────────────────────────────────────────────────────────

def find_all(
    doc: pdfium.PdfDocument,
    query: str,
    match_case: bool = False,
    whole_word: bool = False,
) -> list[SearchMatch]:
    """
    Search every page for query. Returns all matches in document order.
    Each match carries one or more bounding rectangles (a single word can
    span multiple lines, giving multiple rects).
    """
    if not query:
        return []

    results: list[SearchMatch] = []

    for page_idx in range(len(doc)):
        page      = doc[page_idx]
        page_h    = page.get_height()
        textpage  = page.get_textpage()
        searcher  = textpage.search(query, match_case=match_case,
                                    match_whole_word=whole_word)
        while True:
            occurrence = searcher.get_next()   # (start_char_idx, char_count) | None
            if occurrence is None:
                break
            start, count = occurrence
            n_rects = textpage.count_rects(start, count)
            rects: list[tuple[float, float, float, float]] = []
            for ri in range(n_rects):
                # get_rect returns (left, top, right, bottom) in PDF coords
                # where origin is top-left — convert to bottom-left origin
                left, top, right, bottom = textpage.get_rect(ri)
                # PDF user space: flip Y
                pdf_bottom = page_h - bottom
                pdf_top    = page_h - top
                rects.append((left, pdf_bottom, right, pdf_top))
            if rects:
                results.append(SearchMatch(page_idx, rects))
        searcher.close()
        textpage.close()

    return results
