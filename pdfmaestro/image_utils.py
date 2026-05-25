"""
image_utils.py — Pure PIL + pikepdf image helpers shared across projects.
No GTK, no Qt — import safely from any environment.

Original authors: pdfarranger contributors (Konstantinos Poulios, Jérôme Robert).
"""
from __future__ import annotations
import tempfile
import zlib

import pikepdf
from PIL import Image, ImageDraw, ImageEnhance, ImageOps


def remove_bg(img: Image.Image, threshold: int = 200) -> Image.Image:
    """
    Remove light background via luminosity threshold.
    Pixels with luminosity > threshold become transparent; dark ink stays opaque
    with smooth anti-aliased edges.
    threshold: 0-255 luminosity cutoff (200 = near-white, 160 = aggressive).
    """
    img = img.convert("RGBA")
    r, g, b, _ = img.split()
    gray = img.convert("L")
    inverted = ImageOps.invert(gray)
    cutoff = 255 - threshold

    def _to_alpha(x: int) -> int:
        if x <= cutoff:
            return 0
        return min(255, round((x - cutoff) * 255 / max(1, 255 - cutoff)))

    alpha = inverted.point(_to_alpha)
    return Image.merge("RGBA", (r, g, b, alpha))


def boost_contrast(img: Image.Image, factor: float = 3.0) -> Image.Image:
    """Boost RGB contrast by factor, leaving alpha untouched."""
    img = img.convert("RGBA")
    r, g, b, a = img.split()
    rgb = ImageEnhance.Contrast(Image.merge("RGB", (r, g, b))).enhance(factor)
    r2, g2, b2 = rgb.split()
    return Image.merge("RGBA", (r2, g2, b2, a))


def autocrop_alpha(img: Image.Image, margin: int = 8) -> Image.Image:
    """Crop tightly to bounding box of non-transparent pixels plus a small margin."""
    _, _, _, a = img.split()
    bbox = a.getbbox()
    if bbox is None:
        return img
    left   = max(0, bbox[0] - margin)
    top    = max(0, bbox[1] - margin)
    right  = min(img.width,  bbox[2] + margin)
    bottom = min(img.height, bbox[3] + margin)
    return img.crop((left, top, right, bottom))


def rgba_pil_to_pdf(pil_img: Image.Image, tmp_dir: str) -> str:
    """
    Convert an RGBA PIL image to a single-page PDF with true transparency.
    Uses pikepdf with an SMask (soft mask) so the alpha channel is fully
    preserved when overlaid — no white background box.
    Returns the path to the temp PDF file (caller is responsible for cleanup).
    """
    pil_img = pil_img.convert("RGBA")
    w, h = pil_img.size

    r, g, b, a = pil_img.split()
    rgb_raw   = zlib.compress(Image.merge("RGB", (r, g, b)).tobytes())
    alpha_raw = zlib.compress(a.tobytes())

    pdf = pikepdf.Pdf.new()

    smask = pikepdf.Stream(pdf, alpha_raw)
    smask["/Type"]             = pikepdf.Name("/XObject")
    smask["/Subtype"]          = pikepdf.Name("/Image")
    smask["/Width"]            = w
    smask["/Height"]           = h
    smask["/ColorSpace"]       = pikepdf.Name("/DeviceGray")
    smask["/BitsPerComponent"] = 8
    smask["/Filter"]           = pikepdf.Name("/FlateDecode")
    smask_ref = pdf.make_indirect(smask)

    img_xobj = pikepdf.Stream(pdf, rgb_raw)
    img_xobj["/Type"]             = pikepdf.Name("/XObject")
    img_xobj["/Subtype"]          = pikepdf.Name("/Image")
    img_xobj["/Width"]            = w
    img_xobj["/Height"]           = h
    img_xobj["/ColorSpace"]       = pikepdf.Name("/DeviceRGB")
    img_xobj["/BitsPerComponent"] = 8
    img_xobj["/Filter"]           = pikepdf.Name("/FlateDecode")
    img_xobj["/SMask"]            = smask_ref
    img_ref = pdf.make_indirect(img_xobj)

    # 150 DPI: keeps signatures ~2 inches wide — easy to position
    dpi  = 150
    w_pt = round(w * 72 / dpi, 4)
    h_pt = round(h * 72 / dpi, 4)

    content = f"q {w_pt} 0 0 {h_pt} 0 0 cm /Im1 Do Q".encode()

    page_dict = pikepdf.Dictionary(
        Type      = pikepdf.Name.Page,
        MediaBox  = pikepdf.Array([0, 0, w_pt, h_pt]),
        Resources = pikepdf.Dictionary(
            XObject = pikepdf.Dictionary(Im1=img_ref)
        ),
        Contents  = pdf.make_stream(content),
    )
    pdf.pages.append(pikepdf.Page(pdf.make_indirect(page_dict)))

    tmp = tempfile.NamedTemporaryFile(suffix=".pdf", dir=tmp_dir, delete=False)
    pdf.save(tmp.name)
    tmp.close()
    return tmp.name
