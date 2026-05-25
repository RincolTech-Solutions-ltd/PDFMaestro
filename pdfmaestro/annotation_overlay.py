"""
annotation_overlay.py — Transparent QWidget overlay on top of PDFViewer.
Handles mouse interaction for all annotation tools and emits committed signals.
Coordinate conversion between Qt screen space and PDF user space lives here.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, Signal, QRectF, QPointF, QTimer
from PySide6.QtGui import QColor, QPainter, QPen, QBrush, QFont, QCursor
from PySide6.QtGui import QAction
from PySide6.QtWidgets import (
    QWidget, QInputDialog, QLineEdit, QMenu, QDialog,
    QVBoxLayout, QLabel, QDialogButtonBox, QComboBox,
)

# ── Tool constants ────────────────────────────────────────────────────────────
TOOL_POINTER   = "pointer"
TOOL_HIGHLIGHT = "highlight"
TOOL_NOTE      = "note"
TOOL_INK       = "ink"
TOOL_STAMP     = "stamp"
TOOL_REDACT    = "redact"

_CURSORS = {
    TOOL_POINTER:   Qt.ArrowCursor,
    TOOL_HIGHLIGHT: Qt.IBeamCursor,
    TOOL_NOTE:      Qt.PointingHandCursor,
    TOOL_INK:       Qt.CrossCursor,
    TOOL_STAMP:     Qt.PointingHandCursor,
    TOOL_REDACT:    Qt.CrossCursor,
}

_HIGHLIGHT_COLOR = QColor(255, 230, 0, 100)
_REDACT_COLOR    = QColor(0,   0,   0, 160)
_INK_COLOR       = QColor(0,   0,   200, 200)
_STAMP_COLOR     = QColor(0,   153, 0,  180)


class AnnotationOverlay(QWidget):
    """
    Transparent widget stacked on top of the viewer.
    Emits annotation_committed(...) with a dict describing what to write.
    MainWindow connects this signal to the pikepdf writers in annotations.py.
    """

    annotation_committed = Signal(dict)   # payload varies by tool

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TransparentForMouseEvents, False)
        self.setAttribute(Qt.WA_NoSystemBackground, True)
        self.setAttribute(Qt.WA_TranslucentBackground, True)
        self.setMouseTracking(True)

        self._tool       = TOOL_POINTER
        self._page_idx   = 0
        self._page_rect  = QRectF()   # viewer coords: where the current page is drawn
        self._page_h_pt  = 792.0     # PDF page height in points (for Y-flip)
        self._zoom       = 1.0

        # Per-tool draw state
        self._drag_start: QPointF | None = None
        self._drag_cur:   QPointF | None = None
        self._ink_strokes: list[list[tuple[float, float]]] = []
        self._ink_current: list[tuple[float, float]] = []

    # ── Public API ────────────────────────────────────────────────────────────

    def set_tool(self, tool: str):
        self._tool = tool
        self.setCursor(QCursor(_CURSORS.get(tool, Qt.ArrowCursor)))
        # Pointer: pass events through to QGraphicsView for panning/scrolling.
        # Any other tool: overlay captures events for drawing.
        self.setAttribute(Qt.WA_TransparentForMouseEvents, tool == TOOL_POINTER)
        self._reset_draw_state()
        self.update()

    def set_page_context(self, page_idx: int, page_rect: QRectF,
                         page_h_pt: float, zoom: float):
        """Called by the viewer whenever the page layout changes."""
        self._page_idx  = page_idx
        self._page_rect = page_rect
        self._page_h_pt = page_h_pt
        self._zoom      = zoom

    # ── Coordinate helpers ────────────────────────────────────────────────────

    def _to_pdf(self, qpt: QPointF) -> tuple[float, float]:
        """Convert overlay widget coords → PDF user space (origin bottom-left)."""
        pr = self._page_rect
        if pr.width() == 0 or pr.height() == 0:
            return 0.0, 0.0
        rel_x = (qpt.x() - pr.x()) / pr.width()
        rel_y = (qpt.y() - pr.y()) / pr.height()
        # PDF Y-axis is bottom-up; Qt is top-down
        pdf_x = rel_x * self._page_w_pt()
        pdf_y = (1.0 - rel_y) * self._page_h_pt
        return pdf_x, pdf_y

    def _page_w_pt(self) -> float:
        pr = self._page_rect
        if pr.height() == 0:
            return 595.0
        return self._page_h_pt * pr.width() / pr.height()

    # ── Draw state ────────────────────────────────────────────────────────────

    def _reset_draw_state(self):
        self._drag_start  = None
        self._drag_cur    = None
        self._ink_strokes = []
        self._ink_current = []

    # ── Mouse events ─────────────────────────────────────────────────────────

    def mousePressEvent(self, event):
        try:
            if self._tool == TOOL_POINTER or not self._page_rect.contains(event.position()):
                event.ignore()
                return
            pos = event.position()
            if self._tool in (TOOL_HIGHLIGHT, TOOL_REDACT):
                self._drag_start = pos
                self._drag_cur   = pos
            elif self._tool == TOOL_INK:
                x, y = self._to_pdf(pos)
                self._ink_current = [(x, y)]
            elif self._tool == TOOL_NOTE:
                self._commit_note(pos)
            elif self._tool == TOOL_STAMP:
                self._commit_stamp(pos)
            self.update()
        except Exception:
            import traceback; traceback.print_exc()

    def mouseMoveEvent(self, event):
        try:
            pos = event.position()
            if self._tool in (TOOL_HIGHLIGHT, TOOL_REDACT) and self._drag_start:
                self._drag_cur = pos
                self.update()
            elif self._tool == TOOL_INK and self._ink_current:
                x, y = self._to_pdf(pos)
                self._ink_current.append((x, y))
                self.update()
        except Exception:
            import traceback; traceback.print_exc()

    def mouseReleaseEvent(self, event):
        try:
            pos = event.position()
            if self._tool == TOOL_HIGHLIGHT and self._drag_start:
                self._commit_highlight(self._drag_start, pos)
                self._reset_draw_state()
                self.update()
            elif self._tool == TOOL_REDACT and self._drag_start:
                self._commit_redact(self._drag_start, pos)
                self._reset_draw_state()
                self.update()
            elif self._tool == TOOL_INK:
                if len(self._ink_current) >= 2:
                    self._ink_strokes.append(list(self._ink_current))
                self._ink_current = []
                self.update()
        except Exception:
            import traceback; traceback.print_exc()

    def mouseDoubleClickEvent(self, event):
        try:
            if self._tool == TOOL_INK and self._ink_strokes:
                self._commit_ink()
                self._reset_draw_state()
                self.update()
        except Exception:
            import traceback; traceback.print_exc()

    # ── Commit helpers ────────────────────────────────────────────────────────

    def _commit_highlight(self, p1: QPointF, p2: QPointF):
        x0, y0 = self._to_pdf(QPointF(min(p1.x(), p2.x()), min(p1.y(), p2.y())))
        x1, y1 = self._to_pdf(QPointF(max(p1.x(), p2.x()), max(p1.y(), p2.y())))
        if abs(x1 - x0) < 2 or abs(y1 - y0) < 2:
            return
        # QuadPoints: four corners of the rectangle (BL, BR, TL, TR)
        quad = (x0, y0, x1, y0, x0, y1, x1, y1)
        self.annotation_committed.emit({
            "type":     "highlight",
            "page_idx": self._page_idx,
            "quads":    [quad],
            "color":    (1.0, 0.9, 0.0),
            "opacity":  0.4,
        })

    def _commit_note(self, pos: QPointF):
        x, y = self._to_pdf(pos)
        text, ok = QInputDialog.getMultiLineText(
            self, "Sticky Note", "Note text:"
        )
        if ok and text.strip():
            self.annotation_committed.emit({
                "type":     "note",
                "page_idx": self._page_idx,
                "x":        x,
                "y":        y,
                "contents": text.strip(),
            })

    def _commit_ink(self):
        if not self._ink_strokes:
            return
        self.annotation_committed.emit({
            "type":     "ink",
            "page_idx": self._page_idx,
            "strokes":  [list(s) for s in self._ink_strokes],
            "color":    (0.0, 0.0, 0.8),
            "width":    2.0,
        })

    def _commit_stamp(self, pos: QPointF):
        x, y = self._to_pdf(pos)
        dlg = _StampDialog(self)
        if dlg.exec():
            self.annotation_committed.emit({
                "type":     "stamp",
                "page_idx": self._page_idx,
                "x":        x,
                "y":        y - 40,
                "name":     dlg.selected_name,
            })

    def _commit_redact(self, p1: QPointF, p2: QPointF):
        x0, y0 = self._to_pdf(QPointF(min(p1.x(), p2.x()), min(p1.y(), p2.y())))
        x1, y1 = self._to_pdf(QPointF(max(p1.x(), p2.x()), max(p1.y(), p2.y())))
        if abs(x1 - x0) < 2 or abs(y1 - y0) < 2:
            return
        self.annotation_committed.emit({
            "type":     "redact",
            "page_idx": self._page_idx,
            "x0": x0, "y0": y0, "x1": x1, "y1": y1,
        })

    # ── Paint ─────────────────────────────────────────────────────────────────

    def paintEvent(self, _event):
        if self._tool == TOOL_POINTER:
            return
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)

        # Live drag rectangle for highlight / redact
        if self._tool in (TOOL_HIGHLIGHT, TOOL_REDACT) and self._drag_start and self._drag_cur:
            color = _HIGHLIGHT_COLOR if self._tool == TOOL_HIGHLIGHT else _REDACT_COLOR
            p.fillRect(QRectF(self._drag_start, self._drag_cur).normalized(), color)
            pen = QPen(color.darker(140), 1, Qt.DashLine)
            p.setPen(pen)
            p.drawRect(QRectF(self._drag_start, self._drag_cur).normalized())

        # Live ink strokes
        if self._tool == TOOL_INK:
            pen = QPen(_INK_COLOR, 2.5, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin)
            p.setPen(pen)
            for stroke in self._ink_strokes + ([self._pdf_to_screen_stroke(self._ink_current)]
                                                if self._ink_current else []):
                self._paint_screen_stroke(p, self._pdf_stroke_to_screen(stroke))
            if self._ink_strokes:
                p.setFont(QFont("sans-serif", 9))
                p.setPen(QPen(QColor(80, 80, 80)))
                p.drawText(self._page_rect.bottomLeft() + QPointF(4, -6),
                           "Double-click to commit ink")

    def _pdf_stroke_to_screen(self, stroke: list[tuple[float, float]]) -> list[QPointF]:
        pr = self._page_rect
        if pr.width() == 0 or pr.height() == 0:
            return []
        pw = self._page_w_pt()
        result = []
        for pdf_x, pdf_y in stroke:
            sx = pr.x() + (pdf_x / pw) * pr.width()
            sy = pr.y() + (1.0 - pdf_y / self._page_h_pt) * pr.height()
            result.append(QPointF(sx, sy))
        return result

    def _pdf_to_screen_stroke(self, pts_screen: list[tuple[float, float]]) -> list[tuple[float, float]]:
        # Already in PDF space — just return as-is for the combined call above
        return pts_screen

    def _paint_screen_stroke(self, p: QPainter, points: list[QPointF]):
        for i in range(len(points) - 1):
            p.drawLine(points[i], points[i + 1])


# ── Stamp picker dialog ───────────────────────────────────────────────────────

STAMP_NAMES = ["Approved", "Draft", "Confidential", "Final", "Void",
               "ForComment", "NotApproved", "Experimental"]


class _StampDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Insert Stamp")
        self.setFixedWidth(260)
        self.selected_name = "Approved"

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Stamp type:"))
        self._combo = QComboBox()
        self._combo.addItems(STAMP_NAMES)
        layout.addWidget(self._combo)

        btns = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        btns.accepted.connect(self._on_ok)
        btns.rejected.connect(self.reject)
        layout.addWidget(btns)

    def _on_ok(self):
        self.selected_name = self._combo.currentText()
        self.accept()
