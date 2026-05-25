"""
annotations.py — Read and write PDF annotations via pikepdf.
No Qt imports. All coordinates are in PDF user space (points, origin bottom-left).
"""
from __future__ import annotations
import pikepdf


# ── Helpers ───────────────────────────────────────────────────────────────────

def _ensure_annots(page: pikepdf.Page) -> pikepdf.Array:
    """Return the page's /Annots array, creating it if absent."""
    if "/Annots" not in page:
        page["/Annots"] = pikepdf.Array()
    return page["/Annots"]


def _rect(x0: float, y0: float, x1: float, y1: float) -> pikepdf.Array:
    return pikepdf.Array([
        round(v, 3) for v in (x0, y0, x1, y1)
    ])


def _color(r: float, g: float, b: float) -> pikepdf.Array:
    return pikepdf.Array([round(c, 4) for c in (r, g, b)])


# ── Writers ───────────────────────────────────────────────────────────────────

def add_highlight(
    pdf: pikepdf.Pdf,
    page_idx: int,
    quads: list[tuple[float, float, float, float, float, float, float, float]],
    color: tuple[float, float, float] = (1.0, 0.9, 0.0),
    opacity: float = 0.4,
) -> None:
    """
    Add a highlight annotation.
    quads: list of 8-tuples (x1,y1,x2,y2,x3,y3,x4,y4) in PDF user space,
           one per highlighted line rectangle (bottom-left → counter-clockwise).
    """
    page = pdf.pages[page_idx]
    annots = _ensure_annots(page)

    flat = [round(v, 3) for quad in quads for v in quad]
    xs = flat[0::2]
    ys = flat[1::2]

    annot = pikepdf.Dictionary(
        Type          = pikepdf.Name("/Annot"),
        Subtype       = pikepdf.Name("/Highlight"),
        Rect          = _rect(
            min(float(v) for v in xs), min(float(v) for v in ys),
            max(float(v) for v in xs), max(float(v) for v in ys),
        ),
        QuadPoints    = pikepdf.Array(flat),
        C             = _color(*color),
        CA            = round(opacity, 3),
        F             = 4,
    )
    annots.append(pdf.make_indirect(annot))


def add_text_note(
    pdf: pikepdf.Pdf,
    page_idx: int,
    x: float,
    y: float,
    contents: str,
    color: tuple[float, float, float] = (1.0, 0.9, 0.0),
) -> None:
    """Add a sticky-note (Text) annotation at (x, y) in PDF user space."""
    page = pdf.pages[page_idx]
    annots = _ensure_annots(page)

    annot = pikepdf.Dictionary(
        Type     = pikepdf.Name("/Annot"),
        Subtype  = pikepdf.Name("/Text"),
        Rect     = _rect(x, y, x + 24, y + 24),
        Contents = pikepdf.String(contents),
        C        = _color(*color),
        F        = 4,
        Open     = False,
        Name     = pikepdf.Name("/Note"),
    )
    annots.append(pdf.make_indirect(annot))


def add_ink(
    pdf: pikepdf.Pdf,
    page_idx: int,
    strokes: list[list[tuple[float, float]]],
    color: tuple[float, float, float] = (0.0, 0.0, 0.8),
    width: float = 2.0,
) -> None:
    """
    Add a freehand ink annotation.
    strokes: list of polylines, each a list of (x, y) in PDF user space.
    """
    page = pdf.pages[page_idx]
    annots = _ensure_annots(page)

    ink_list = pikepdf.Array()
    all_xs, all_ys = [], []
    for stroke in strokes:
        pts = pikepdf.Array([round(v, 3) for pt in stroke for v in pt])
        ink_list.append(pts)
        all_xs.extend(p[0] for p in stroke)
        all_ys.extend(p[1] for p in stroke)

    if not all_xs:
        return

    annot = pikepdf.Dictionary(
        Type    = pikepdf.Name("/Annot"),
        Subtype = pikepdf.Name("/Ink"),
        Rect    = _rect(min(all_xs) - 2, min(all_ys) - 2,
                        max(all_xs) + 2, max(all_ys) + 2),
        InkList = ink_list,
        BS      = pikepdf.Dictionary(W=round(width, 2)),
        C       = _color(*color),
        F       = 4,
    )
    annots.append(pdf.make_indirect(annot))


def add_stamp(
    pdf: pikepdf.Pdf,
    page_idx: int,
    x: float,
    y: float,
    width: float = 120.0,
    height: float = 40.0,
    name: str = "Approved",
    color: tuple[float, float, float] = (0.0, 0.6, 0.0),
) -> None:
    """Add a rubber-stamp annotation. name: Approved | Draft | Confidential | etc."""
    page = pdf.pages[page_idx]
    annots = _ensure_annots(page)

    annot = pikepdf.Dictionary(
        Type    = pikepdf.Name("/Annot"),
        Subtype = pikepdf.Name("/Stamp"),
        Rect    = _rect(x, y, x + width, y + height),
        Name    = pikepdf.Name(f"/{name}"),
        C       = _color(*color),
        F       = 4,
    )
    annots.append(pdf.make_indirect(annot))


def add_redact(
    pdf: pikepdf.Pdf,
    page_idx: int,
    x0: float,
    y0: float,
    x1: float,
    y1: float,
) -> None:
    """
    Add a redaction annotation marking text/content for removal.
    Call apply_redactions() to permanently burn it in.
    """
    page = pdf.pages[page_idx]
    annots = _ensure_annots(page)

    annot = pikepdf.Dictionary(
        Type        = pikepdf.Name("/Annot"),
        Subtype     = pikepdf.Name("/Redact"),
        Rect        = _rect(x0, y0, x1, y1),
        IC          = _color(0.0, 0.0, 0.0),
        OverlayText = pikepdf.String(""),
        F           = 4,
    )
    annots.append(pdf.make_indirect(annot))


def apply_redactions(pdf: pikepdf.Pdf, page_idx: int) -> None:
    """
    Burn in redaction annotations on a page by painting black rectangles
    over the marked areas and removing the annotation objects.
    Note: this removes the visual content but does not strip underlying text
    streams — for full redaction, use a dedicated PDF redaction library.
    """
    page = pdf.pages[page_idx]
    annots_raw = page.get("/Annots")
    if annots_raw is None:
        return

    redact_rects: list[tuple[float, float, float, float]] = []
    surviving: list = []

    for annot in annots_raw:
        if str(annot.get("/Subtype", "")) == "/Redact":
            mb = annot.get("/Rect")
            if mb:
                redact_rects.append(tuple(float(v) for v in mb))
        else:
            surviving.append(annot)

    if not redact_rects:
        return

    # Paint solid black rectangles over each redacted area
    ops = ""
    for x0, y0, x1, y1 in redact_rects:
        ops += f"0 g {x0:.3f} {y0:.3f} {x1 - x0:.3f} {y1 - y0:.3f} re f "

    existing = page.get("/Contents")
    if existing is None:
        page["/Contents"] = pdf.make_stream(ops.encode())
    elif isinstance(existing, pikepdf.Stream):
        combined = existing.read_bytes() + b"\n" + ops.encode()
        page["/Contents"] = pdf.make_stream(combined)
    else:
        combined = b"\n".join(item.read_bytes() for item in existing) + b"\n" + ops.encode()
        page["/Contents"] = pdf.make_stream(combined)

    page["/Annots"] = pikepdf.Array(surviving)


# ── Reader (for viewer rendering) ─────────────────────────────────────────────

def read_annotations(pdf: pikepdf.Pdf, page_idx: int) -> list[dict]:
    """
    Return a list of annotation dicts for rendering in the viewer.
    Each dict has at minimum: 'subtype', 'rect' (x0,y0,x1,y1 in PDF space).
    Additional keys depend on type.
    """
    page = pdf.pages[page_idx]
    annots_raw = page.get("/Annots")
    if annots_raw is None:
        return []

    result = []
    for annot in annots_raw:
        try:
            subtype = str(annot.get("/Subtype", "")).lstrip("/")
            rect_raw = annot.get("/Rect")
            if rect_raw is None:
                continue
            rect = tuple(float(v) for v in rect_raw)

            entry: dict = {"subtype": subtype, "rect": rect}

            if subtype == "Highlight":
                qp = annot.get("/QuadPoints")
                if qp:
                    entry["quad_points"] = [float(v) for v in qp]
                c = annot.get("/C")
                entry["color"] = tuple(float(v) for v in c) if c else (1.0, 0.9, 0.0)
                ca = annot.get("/CA")
                entry["opacity"] = float(ca) if ca else 0.4

            elif subtype == "Text":
                entry["contents"] = str(annot.get("/Contents", ""))
                c = annot.get("/C")
                entry["color"] = tuple(float(v) for v in c) if c else (1.0, 0.9, 0.0)

            elif subtype == "Ink":
                ink_list = annot.get("/InkList", pikepdf.Array())
                strokes = []
                for stroke in ink_list:
                    pts = [float(v) for v in stroke]
                    strokes.append(list(zip(pts[0::2], pts[1::2])))
                entry["strokes"] = strokes
                c = annot.get("/C")
                entry["color"] = tuple(float(v) for v in c) if c else (0.0, 0.0, 0.8)
                bs = annot.get("/BS")
                entry["width"] = float(bs.get("/W", 2.0)) if bs else 2.0

            elif subtype == "Stamp":
                entry["name"] = str(annot.get("/Name", "/Approved")).lstrip("/")
                c = annot.get("/C")
                entry["color"] = tuple(float(v) for v in c) if c else (0.0, 0.6, 0.0)

            elif subtype == "Redact":
                entry["color"] = (0.0, 0.0, 0.0)

            result.append(entry)
        except Exception:
            continue

    return result
