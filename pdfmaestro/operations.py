"""
Pure pikepdf operations — no Qt imports.
Every function takes an open pikepdf.Pdf and mutates it in place.
Call _reload_viewer() in MainWindow after any mutating operation.
"""

from __future__ import annotations
from pathlib import Path
import pikepdf


# ── Page order & deletion ─────────────────────────────────────────────────────

def apply_page_order(pdf: pikepdf.Pdf, order: list[int]) -> None:
    """Reorder pages in-place. order[i] = original 0-based index to put at position i."""
    original = list(pdf.pages)
    while pdf.pages:
        del pdf.pages[0]
    for i in order:
        pdf.pages.append(original[i])


def delete_pages(pdf: pikepdf.Pdf, indices: set[int]) -> None:
    """Remove pages at the given 0-based indices."""
    for i in sorted(indices, reverse=True):
        del pdf.pages[i]


# ── Rotation ──────────────────────────────────────────────────────────────────

def rotate_page(pdf: pikepdf.Pdf, page_index: int, degrees: int) -> None:
    """Rotate a page. degrees must be a multiple of 90 (positive = CW)."""
    page = pdf.pages[page_index]
    current = int(page.get("/Rotate", 0))
    new_rotation = (current + degrees) % 360
    if new_rotation == 0:
        page.pop("/Rotate", None)
    else:
        page["/Rotate"] = new_rotation


def rotate_pages(pdf: pikepdf.Pdf, indices: list[int], degrees: int) -> None:
    for i in indices:
        rotate_page(pdf, i, degrees)


# ── Crop ──────────────────────────────────────────────────────────────────────

def _get_media_box(page: pikepdf.Page) -> tuple[float, float, float, float]:
    mb = page.mediabox
    return tuple(float(v) for v in mb)  # left, bottom, right, top


def crop_page(
    pdf: pikepdf.Pdf,
    page_index: int,
    margin_left: float,
    margin_bottom: float,
    margin_right: float,
    margin_top: float,
) -> None:
    """Crop a single page by inset margins (in PDF points, 72pt = 1 inch)."""
    page = pdf.pages[page_index]
    l, b, r, t = _get_media_box(page)
    page["/CropBox"] = pikepdf.Array([
        pikepdf.Decimal(str(round(l + margin_left, 3))),
        pikepdf.Decimal(str(round(b + margin_bottom, 3))),
        pikepdf.Decimal(str(round(r - margin_right, 3))),
        pikepdf.Decimal(str(round(t - margin_top, 3))),
    ])


def crop_all_pages(
    pdf: pikepdf.Pdf,
    margin_left: float,
    margin_bottom: float,
    margin_right: float,
    margin_top: float,
) -> None:
    for i in range(len(pdf.pages)):
        crop_page(pdf, i, margin_left, margin_bottom, margin_right, margin_top)


# ── Merge ─────────────────────────────────────────────────────────────────────

def merge_into(
    target_pdf: pikepdf.Pdf,
    other_paths: list[str],
    insert_at: int,
) -> None:
    """Insert all pages from other_paths into target_pdf starting at insert_at."""
    pos = insert_at
    for path in other_paths:
        with pikepdf.Pdf.open(path) as other:
            for page in other.pages:
                target_pdf.pages.insert(pos, target_pdf.copy_foreign(page))
                pos += 1


# ── Split ─────────────────────────────────────────────────────────────────────

def split_by_ranges(
    pdf: pikepdf.Pdf,
    ranges: list[tuple[int, int]],
    out_dir: str,
    base_name: str,
) -> list[str]:
    """Save each range as a separate PDF. Ranges are 0-based inclusive (start, end)."""
    out_paths = []
    out_dir_path = Path(out_dir)
    out_dir_path.mkdir(parents=True, exist_ok=True)
    for i, (start, end) in enumerate(ranges, 1):
        new_pdf = pikepdf.Pdf.new()
        for idx in range(start, min(end + 1, len(pdf.pages))):
            new_pdf.pages.append(new_pdf.copy_foreign(pdf.pages[idx]))
        out_path = str(out_dir_path / f"{base_name}_{i:03d}.pdf")
        new_pdf.save(out_path)
        out_paths.append(out_path)
    return out_paths


def split_every_n(
    pdf: pikepdf.Pdf,
    n: int,
    out_dir: str,
    base_name: str,
) -> list[str]:
    total = len(pdf.pages)
    ranges = [(i, min(i + n - 1, total - 1)) for i in range(0, total, n)]
    return split_by_ranges(pdf, ranges, out_dir, base_name)


def split_each_page(pdf: pikepdf.Pdf, out_dir: str, base_name: str) -> list[str]:
    return split_by_ranges(
        pdf,
        [(i, i) for i in range(len(pdf.pages))],
        out_dir,
        base_name,
    )


# ── Helpers ───────────────────────────────────────────────────────────────────

def parse_ranges(text: str, total_pages: int) -> list[tuple[int, int]]:
    """Parse '1-5, 6-10, 15' into 0-based inclusive tuples [(0,4),(5,9),(14,14)]."""
    ranges = []
    for part in text.split(","):
        part = part.strip()
        if not part:
            continue
        try:
            if "-" in part:
                a, b = part.split("-", 1)
                s, e = int(a.strip()) - 1, int(b.strip()) - 1
            else:
                s = e = int(part.strip()) - 1
            if 0 <= s <= e < total_pages:
                ranges.append((s, e))
        except ValueError:
            continue
    return ranges
