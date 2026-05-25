"""
signature.py — Signature dialog for PDFMaestro.
PySide6 port of pdfarranger/pdfarranger/signature.py (GTK).
UI layer replaced with Qt; all image processing is kept identical.
Credits: pdfarranger (Konstantinos Poulios, Jérôme Robert).
"""
from __future__ import annotations
import subprocess
from pathlib import Path

from pdfmaestro.image_utils import (
    remove_bg as _remove_bg,
    boost_contrast as _boost_contrast,
    autocrop_alpha as _autocrop_alpha,
    rgba_pil_to_pdf,
)
from PIL import Image, ImageDraw, ImageFont

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPen, QPixmap, QImage, QFont
from PySide6.QtWidgets import (
    QDialog, QDialogButtonBox, QVBoxLayout, QHBoxLayout, QGridLayout,
    QTabWidget, QWidget, QLabel, QLineEdit, QPushButton, QCheckBox,
    QScrollArea, QButtonGroup, QRadioButton, QFileDialog, QFrame,
    QSizePolicy,
)

# ── Persistence ───────────────────────────────────────────────────────────────
_CONFIG_DIR = Path.home() / ".config" / "pdfmaestro"
_SAVED_SIG  = _CONFIG_DIR / "signature.png"

# ── Ink colours: ((r, g, b 0-1), hex string) ─────────────────────────────────
COLORS = [
    ((0.40, 0.55, 1.00), "#6699FF"),
    ((0.20, 0.40, 0.85), "#3366D9"),
    ((0.10, 0.25, 0.70), "#1940B2"),
    ((0.05, 0.15, 0.55), "#0D268C"),
    ((0.27, 0.27, 0.27), "#454545"),
    ((0.50, 0.50, 0.50), "#808080"),
    ((0.00, 0.00, 0.00), "#000000"),
]

# ── Font specs: (fc-pattern, Qt-family, bold, italic, pt-size) ────────────────
FONTS = [
    ("URW Chancery L:style=Italic",      "URW Chancery L",     False, True,  28),
    ("URW Chancery L:style=Bold Italic", "URW Chancery L",     True,  True,  26),
    ("Noto Serif:style=Italic",          "Noto Serif",         False, True,  22),
    ("Noto Serif:style=Bold Italic",     "Noto Serif",         True,  True,  22),
    ("DejaVu Serif:style=Italic",        "DejaVu Serif",       False, True,  20),
    ("DejaVu Serif:style=Bold Italic",   "DejaVu Serif",       True,  True,  20),
    ("Liberation Serif:style=Italic",    "Liberation Serif",   False, True,  22),
    ("FreeSerif:style=Italic",           "FreeSerif",          False, True,  22),
    ("Bitstream Charter:style=Italic",   "Bitstream Charter",  False, True,  20),
    ("FreeMono:style=Italic",            "FreeMono",           False, True,  18),
    ("DejaVu Sans:style=Oblique",        "DejaVu Sans",        False, True,  20),
]


# ── Custom widgets ────────────────────────────────────────────────────────────

class _ColorButton(QPushButton):
    """Circular colour swatch — checkable, toggles selection ring."""

    def __init__(self, rgb_float: tuple, parent=None):
        super().__init__(parent)
        self._rgb = rgb_float
        self.setFixedSize(32, 32)
        self.setCheckable(True)
        self.setFlat(True)
        self.setCursor(Qt.PointingHandCursor)

    def paintEvent(self, _event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        r, g, b = (int(c * 255) for c in self._rgb)
        p.setBrush(QColor(r, g, b))
        p.setPen(QPen(Qt.black, 2.5 if self.isChecked() else 0.5))
        p.drawEllipse(3, 3, 26, 26)


class _FontCell(QFrame):
    """Clickable font preview cell for the Type-tab grid."""

    clicked = Signal(int)

    def __init__(self, index: int, parent=None):
        super().__init__(parent)
        self._index = index
        self.setFrameShape(QFrame.StyledPanel)
        self.setFixedSize(218, 64)
        self.setCursor(Qt.PointingHandCursor)
        self.set_selected(False)

        layout = QHBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        self._label = QLabel("Your Signature")
        self._label.setAlignment(Qt.AlignCenter)
        self._label.setWordWrap(True)
        layout.addWidget(self._label)

    def set_selected(self, selected: bool):
        if selected:
            self.setStyleSheet(
                "QFrame { border: 2px solid #2196F3; background: #E3F2FD; }"
                " QLabel { background: transparent; }"
            )
        else:
            self.setStyleSheet(
                "QFrame { border: 1px solid #CCCCCC; background: white; }"
                " QLabel { background: transparent; }"
            )

    def update_display(
        self, text: str, family: str, bold: bool, italic: bool,
        size: int, hexcol: str,
    ):
        font = QFont(family, size)
        font.setBold(bold)
        font.setItalic(italic)
        self._label.setFont(font)
        self._label.setStyleSheet(
            f"color: {hexcol}; background: transparent; border: none;"
        )
        self._label.setText(text or "Your Signature")

    def mousePressEvent(self, _event):
        self.clicked.emit(self._index)


class _DrawCanvas(QWidget):
    """Freehand signature drawing canvas."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(660, 210)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setCursor(Qt.CrossCursor)
        self._strokes: list[list[tuple[float, float]]] = []
        self._current: list[tuple[float, float]] | None = None
        self._color_idx = 6

    def set_color(self, idx: int):
        self._color_idx = idx

    def clear(self):
        self._strokes = []
        self._current = None
        self.update()

    def has_strokes(self) -> bool:
        return bool(self._strokes)

    def get_image(self) -> Image.Image | None:
        if not self._strokes:
            return None
        w, h = self.width(), self.height()
        img = Image.new("RGBA", (w, h), (255, 255, 255, 255))
        draw = ImageDraw.Draw(img)
        r_f, g_f, b_f = COLORS[self._color_idx][0]
        ink = (int(r_f * 255), int(g_f * 255), int(b_f * 255), 255)
        for stroke in self._strokes:
            for i in range(len(stroke) - 1):
                draw.line([stroke[i], stroke[i + 1]], fill=ink, width=3)
        return img

    def paintEvent(self, _event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        p.fillRect(self.rect(), Qt.white)

        r_f, g_f, b_f = COLORS[self._color_idx][0]
        pen = QPen(
            QColor(int(r_f * 255), int(g_f * 255), int(b_f * 255)),
            2.5, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin,
        )
        p.setPen(pen)
        for stroke in self._strokes:
            self._paint_stroke(p, stroke)
        if self._current and len(self._current) >= 2:
            self._paint_stroke(p, self._current)

    def _paint_stroke(self, p: QPainter, stroke: list):
        for i in range(len(stroke) - 1):
            x0, y0 = stroke[i]
            x1, y1 = stroke[i + 1]
            p.drawLine(int(x0), int(y0), int(x1), int(y1))

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            pos = event.position()
            self._current = [(pos.x(), pos.y())]

    def mouseMoveEvent(self, event):
        if self._current is not None:
            pos = event.position()
            self._current.append((pos.x(), pos.y()))
            self.update()

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.LeftButton and self._current:
            self._strokes.append(self._current)
            self._current = None
            self.update()


# ── Main dialog ───────────────────────────────────────────────────────────────

class SignatureDialog(QDialog):

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Insert Signature")
        self.setMinimumSize(720, 560)
        self.resize(720, 560)

        self._color_idx        = 6
        self._font_idx         = 0
        self._upload_variants  = [None, None, None]
        self._selected_variant = 1
        self._font_cells: list[_FontCell] = []

        self._build_ui()
        self._refresh_font_cells()
        self._load_saved()

    # ── Layout ────────────────────────────────────────────────────────────────

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(12, 12, 12, 8)
        root.setSpacing(8)

        self._tabs = QTabWidget()
        self._tabs.addTab(self._build_type_tab(),   "⌨  Type")
        self._tabs.addTab(self._build_draw_tab(),   "✍  Draw")
        self._tabs.addTab(self._build_upload_tab(), "📄  Upload Image")
        root.addWidget(self._tabs)

        bar = QHBoxLayout()
        bar.setContentsMargins(4, 4, 4, 0)
        hint = QLabel("Position set interactively after clicking Insert")
        hint.setEnabled(False)
        bar.addWidget(hint)
        bar.addStretch()
        self._save_check = QCheckBox("Save signature")
        self._save_check.setChecked(True)
        bar.addWidget(self._save_check)
        root.addLayout(bar)

        btns = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        btns.button(QDialogButtonBox.Ok).setText("Insert")
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)
        root.addWidget(btns)

    # ── Type tab ──────────────────────────────────────────────────────────────

    def _build_type_tab(self) -> QWidget:
        w = QWidget()
        vbox = QVBoxLayout(w)
        vbox.setContentsMargins(12, 14, 12, 8)
        vbox.setSpacing(10)

        self._type_entry = QLineEdit()
        self._type_entry.setPlaceholderText("Your full name")
        self._type_entry.textChanged.connect(self._on_type_changed)
        vbox.addWidget(self._type_entry)

        palette = QHBoxLayout()
        palette.setSpacing(8)
        palette.addStretch()
        self._color_group = QButtonGroup(self)
        for i, ((r, g, b), _) in enumerate(COLORS):
            btn = _ColorButton((r, g, b))
            btn.setChecked(i == self._color_idx)
            self._color_group.addButton(btn, i)
            palette.addWidget(btn)
        palette.addStretch()
        self._color_group.idClicked.connect(self._on_color_selected)
        vbox.addLayout(palette)

        style_lbl = QLabel("Choose a style:")
        vbox.addWidget(style_lbl)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

        grid_w = QWidget()
        self._font_grid = QGridLayout(grid_w)
        self._font_grid.setSpacing(4)
        self._font_grid.setContentsMargins(4, 4, 4, 4)

        for i in range(len(FONTS)):
            cell = _FontCell(i)
            cell.clicked.connect(self._on_font_selected)
            self._font_cells.append(cell)
            self._font_grid.addWidget(cell, i // 3, i % 3)

        self._font_cells[0].set_selected(True)
        scroll.setWidget(grid_w)
        scroll.setMinimumHeight(230)
        vbox.addWidget(scroll)
        return w

    def _on_type_changed(self, _text: str):
        self._refresh_font_cells()

    def _on_color_selected(self, idx: int):
        self._color_idx = idx
        self._refresh_font_cells()
        if hasattr(self, "_draw_canvas"):
            self._draw_canvas.set_color(idx)
            self._draw_canvas.update()

    def _on_font_selected(self, idx: int):
        self._font_cells[self._font_idx].set_selected(False)
        self._font_idx = idx
        self._font_cells[idx].set_selected(True)

    def _refresh_font_cells(self):
        text = self._type_entry.text().strip() if hasattr(self, "_type_entry") else ""
        _, hexcol = COLORS[self._color_idx]
        for i, (_fc, family, bold, italic, size) in enumerate(FONTS):
            self._font_cells[i].update_display(text, family, bold, italic, size, hexcol)

    def _get_type_image(self) -> Image.Image | None:
        text = self._type_entry.text().strip()
        if not text:
            return None
        fc_pattern, _family, bold, italic, _size = FONTS[self._font_idx]
        (r_f, g_f, b_f), _ = COLORS[self._color_idx]
        ink = (int(r_f * 255), int(g_f * 255), int(b_f * 255), 255)

        try:
            font_path = subprocess.run(
                ["fc-match", "--format=%{file}", fc_pattern],
                capture_output=True, text=True, timeout=3,
            ).stdout.strip()
            pil_font = ImageFont.truetype(font_path, 60)
        except Exception:
            pil_font = ImageFont.load_default()

        tmp = Image.new("RGBA", (1, 1))
        bbox = ImageDraw.Draw(tmp).textbbox((0, 0), text, font=pil_font)
        pad = 20
        w = bbox[2] - bbox[0] + pad * 2
        h = bbox[3] - bbox[1] + pad * 2
        img = Image.new("RGBA", (w, h), (255, 255, 255, 0))
        ImageDraw.Draw(img).text(
            (pad - bbox[0], pad - bbox[1]), text, font=pil_font, fill=ink,
        )
        return img

    # ── Draw tab ──────────────────────────────────────────────────────────────

    def _build_draw_tab(self) -> QWidget:
        w = QWidget()
        vbox = QVBoxLayout(w)
        vbox.setContentsMargins(12, 12, 12, 8)
        vbox.setSpacing(8)

        vbox.addWidget(QLabel("Draw your signature below:"))

        frame = QFrame()
        frame.setFrameShape(QFrame.StyledPanel)
        frame.setFrameShadow(QFrame.Sunken)
        fl = QVBoxLayout(frame)
        fl.setContentsMargins(1, 1, 1, 1)
        self._draw_canvas = _DrawCanvas()
        fl.addWidget(self._draw_canvas)
        vbox.addWidget(frame, stretch=1)

        clear_btn = QPushButton("Clear")
        clear_btn.setFixedWidth(80)
        clear_btn.clicked.connect(self._draw_canvas.clear)
        row = QHBoxLayout()
        row.addStretch()
        row.addWidget(clear_btn)
        vbox.addLayout(row)
        return w

    # ── Upload tab ────────────────────────────────────────────────────────────

    def _build_upload_tab(self) -> QWidget:
        w = QWidget()
        vbox = QVBoxLayout(w)
        vbox.setContentsMargins(12, 16, 12, 8)
        vbox.setSpacing(12)

        row = QHBoxLayout()
        row.addWidget(QLabel("Select image:"))
        self._file_btn = QPushButton("Browse…")
        self._file_btn.clicked.connect(self._on_browse)
        self._file_lbl = QLabel("No file selected")
        self._file_lbl.setEnabled(False)
        row.addWidget(self._file_btn)
        row.addWidget(self._file_lbl, stretch=1)
        vbox.addLayout(row)

        vbox.addWidget(QLabel("Choose an image version:"), alignment=Qt.AlignHCenter)

        thumbs = QHBoxLayout()
        thumbs.setSpacing(20)
        thumbs.addStretch()
        self._var_radio_group = QButtonGroup(self)
        self._var_previews: list[QLabel] = []
        self._var_radios: list[QRadioButton] = []

        for i, caption in enumerate(["Original", "Transparent A", "Transparent B"]):
            col = QVBoxLayout()
            col.setSpacing(6)
            col.setAlignment(Qt.AlignHCenter)

            preview = QLabel()
            preview.setFixedSize(190, 130)
            preview.setAlignment(Qt.AlignCenter)
            preview.setFrameShape(QFrame.StyledPanel)
            preview.setStyleSheet("background: white;")
            col.addWidget(preview)

            radio = QRadioButton(caption)
            radio.setChecked(i == self._selected_variant)
            self._var_radio_group.addButton(radio, i)
            col.addWidget(radio, alignment=Qt.AlignHCenter)

            self._var_previews.append(preview)
            self._var_radios.append(radio)
            thumbs.addLayout(col)

        thumbs.addStretch()
        self._var_radio_group.idClicked.connect(self._on_variant_selected)
        vbox.addLayout(thumbs, stretch=1)
        return w

    def _on_variant_selected(self, idx: int):
        self._selected_variant = idx

    def _on_browse(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Signature Image", "",
            "Images (*.png *.jpg *.jpeg *.bmp *.tiff)",
        )
        if not path:
            return
        try:
            orig = Image.open(path).convert("RGBA")
        except Exception:
            return
        self._file_lbl.setText(Path(path).name)
        self._file_lbl.setEnabled(True)
        trans_a = _autocrop_alpha(_remove_bg(orig, threshold=200))
        trans_b = _autocrop_alpha(_remove_bg(_boost_contrast(orig), threshold=160))
        self._upload_variants = [orig, trans_a, trans_b]
        for i, img in enumerate(self._upload_variants):
            self._var_previews[i].setPixmap(_pil_to_qpixmap(img, 190, 130))
        self._var_radios[1].setChecked(True)
        self._selected_variant = 1

    # ── Saved signature ───────────────────────────────────────────────────────

    def _load_saved(self):
        if _SAVED_SIG.exists():
            try:
                img = Image.open(_SAVED_SIG).convert("RGBA")
                self._upload_variants[1] = img
                self._var_previews[1].setPixmap(_pil_to_qpixmap(img, 190, 130))
                self._var_radios[1].setChecked(True)
                self._tabs.setCurrentIndex(2)
            except Exception:
                pass

    def _save_signature(self, img: Image.Image):
        _CONFIG_DIR.mkdir(parents=True, exist_ok=True)
        img.save(_SAVED_SIG)

    # ── Public result ─────────────────────────────────────────────────────────

    def get_result(self) -> tuple[Image.Image | None, None]:
        """Return (PIL RGBA Image, None). Position is chosen interactively after Insert."""
        tab = self._tabs.currentIndex()
        img = None
        if tab == 0:
            img = self._get_type_image()
        elif tab == 1:
            img = self._draw_canvas.get_image()
        elif tab == 2:
            img = self._upload_variants[self._selected_variant]

        if img is not None and self._save_check.isChecked():
            self._save_signature(img.convert("RGBA"))

        return img, None


# ── Qt-specific image helper ──────────────────────────────────────────────────
# _remove_bg, _boost_contrast, _autocrop_alpha, rgba_pil_to_pdf
# are imported from pdfarranger.image_utils — single source of truth.

def _pil_to_qpixmap(img: Image.Image, max_w: int, max_h: int) -> QPixmap:
    img = img.convert("RGBA")
    img.thumbnail((max_w, max_h), Image.LANCZOS)
    w, h = img.size
    raw = bytes(img.tobytes("raw", "RGBA"))
    qimg = QImage(raw, w, h, w * 4, QImage.Format_RGBA8888).copy()
    return QPixmap.fromImage(qimg)
