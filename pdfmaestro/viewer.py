import pypdfium2 as pdfium
from PySide6.QtWidgets import QGraphicsView, QGraphicsScene, QGraphicsPixmapItem
from PySide6.QtCore import Qt, Signal, QTimer
from PySide6.QtGui import QPainter, QPixmap, QImage, QColor, QWheelEvent, QKeyEvent

BASE_DPI = 150   # base render resolution
PAGE_GAP = 16    # vertical gap between pages (px)
ZOOM_MIN = 0.10
ZOOM_MAX = 5.00


def _page_to_pixmap(page: pdfium.PdfPage, dpi: float) -> QPixmap:
    bitmap = page.render(scale=dpi / 72.0)
    pil = bitmap.to_pil().convert("RGBA")
    raw = bytes(pil.tobytes("raw", "RGBA"))
    # .copy() detaches QImage from the Python bytes lifetime
    qimg = QImage(raw, pil.width, pil.height, pil.width * 4, QImage.Format_RGBA8888).copy()
    return QPixmap.fromImage(qimg)


class PageItem(QGraphicsPixmapItem):
    def __init__(self, index: int, pixmap: QPixmap):
        super().__init__(pixmap)
        self._index = index
        self.setTransformationMode(Qt.SmoothTransformation)

    @property
    def page_index(self) -> int:
        return self._index


class PDFViewer(QGraphicsView):
    page_changed = Signal(int, int)     # (current_1based, total)
    zoom_changed = Signal(float)        # zoom factor  (1.0 = 100 %)
    document_loaded = Signal(str, int)  # (path, page_count)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._scene = QGraphicsScene(self)
        self.setScene(self._scene)

        self._doc: pdfium.PdfDocument | None = None
        self._path: str | None = None
        self._page_items: list[PageItem] = []
        self._zoom = 1.0
        self._current_page = 0

        # Re-render after zoom settles (debounce avoids rendering on every step)
        self._rerender_timer = QTimer(self)
        self._rerender_timer.setSingleShot(True)
        self._rerender_timer.setInterval(350)
        self._rerender_timer.timeout.connect(self._rerender_at_zoom)

        self.setDragMode(QGraphicsView.ScrollHandDrag)
        self.setRenderHints(QPainter.Antialiasing | QPainter.SmoothPixmapTransform)
        self.setBackgroundBrush(QColor("#1e2530"))
        self.setAlignment(Qt.AlignHCenter | Qt.AlignTop)
        self.setFocusPolicy(Qt.StrongFocus)

    # ── Document loading ──────────────────────────────────────────────────────

    def load_document(self, path: str):
        if self._doc:
            self._doc.close()
        self._doc = pdfium.PdfDocument(path)
        self._path = path
        self._current_page = 0
        self._zoom = 1.0
        self._build_scene()
        self.document_loaded.emit(path, len(self._doc))
        self.page_changed.emit(1, len(self._doc))
        self.zoom_changed.emit(1.0)

    def _build_scene(self):
        self._scene.clear()
        self._page_items.clear()
        if not self._doc:
            return
        y = PAGE_GAP
        for i in range(len(self._doc)):
            page = self._doc[i]
            pixmap = _page_to_pixmap(page, BASE_DPI * self._zoom)
            item = PageItem(i, pixmap)
            item.setPos(-pixmap.width() / 2, y)
            self._scene.addItem(item)
            self._page_items.append(item)
            y += pixmap.height() + PAGE_GAP
        self._refresh_scene_rect()

    def _rerender_at_zoom(self):
        """Re-render every page at the current zoom level (called after debounce)."""
        if not self._doc:
            return
        y = PAGE_GAP
        for i, item in enumerate(self._page_items):
            page = self._doc[i]
            pixmap = _page_to_pixmap(page, BASE_DPI * self._zoom)
            item.setPixmap(pixmap)
            item.setPos(-pixmap.width() / 2, y)
            y += pixmap.height() + PAGE_GAP
        self._refresh_scene_rect()

    def _refresh_scene_rect(self):
        r = self._scene.itemsBoundingRect()
        self.setSceneRect(r.adjusted(-20, -PAGE_GAP, 20, PAGE_GAP))

    # ── Zoom ──────────────────────────────────────────────────────────────────

    def set_zoom(self, zoom: float):
        self._zoom = max(ZOOM_MIN, min(ZOOM_MAX, zoom))
        self._rerender_timer.start()
        self.zoom_changed.emit(self._zoom)

    def zoom_in(self):
        self.set_zoom(self._zoom * 1.25)

    def zoom_out(self):
        self.set_zoom(self._zoom / 1.25)

    def zoom_reset(self):
        self.set_zoom(1.0)

    def fit_width(self):
        if not self._doc or not self._page_items:
            return
        page = self._doc[self._current_page]
        base_px = page.get_width() * BASE_DPI / 72
        vw = self.viewport().width() - 40
        self.set_zoom(vw / base_px)

    def fit_page(self):
        if not self._doc or not self._page_items:
            return
        page = self._doc[self._current_page]
        base_w = page.get_width() * BASE_DPI / 72
        base_h = page.get_height() * BASE_DPI / 72
        vw = self.viewport().width() - 40
        vh = self.viewport().height() - 40
        self.set_zoom(min(vw / base_w, vh / base_h))

    # ── Navigation ────────────────────────────────────────────────────────────

    def go_to_page(self, index: int):
        if not self._page_items:
            return
        index = max(0, min(index, len(self._page_items) - 1))
        self._current_page = index
        self.centerOn(self._page_items[index])
        self.page_changed.emit(index + 1, len(self._doc))

    def next_page(self):
        self.go_to_page(self._current_page + 1)

    def prev_page(self):
        self.go_to_page(self._current_page - 1)

    def first_page(self):
        self.go_to_page(0)

    def last_page(self):
        self.go_to_page(len(self._page_items) - 1)

    def _update_current_page_from_scroll(self):
        """Update the current page indicator based on scroll position."""
        if not self._page_items:
            return
        cy = self.mapToScene(self.viewport().rect().center()).y()
        closest, min_dist = 0, float("inf")
        for i, item in enumerate(self._page_items):
            page_cy = item.y() + item.pixmap().height() / 2
            d = abs(page_cy - cy)
            if d < min_dist:
                min_dist, closest = d, i
        if closest != self._current_page:
            self._current_page = closest
            self.page_changed.emit(closest + 1, len(self._doc))

    # ── Events ────────────────────────────────────────────────────────────────

    def wheelEvent(self, event: QWheelEvent):
        if event.modifiers() & Qt.ControlModifier:
            if event.angleDelta().y() > 0:
                self.zoom_in()
            else:
                self.zoom_out()
            event.accept()
        else:
            super().wheelEvent(event)
            self._update_current_page_from_scroll()

    def keyPressEvent(self, event: QKeyEvent):
        key, mod = event.key(), event.modifiers()
        if key in (Qt.Key_Right, Qt.Key_PageDown):
            self.next_page()
        elif key in (Qt.Key_Left, Qt.Key_PageUp):
            self.prev_page()
        elif key == Qt.Key_Home:
            self.first_page()
        elif key == Qt.Key_End:
            self.last_page()
        elif key == Qt.Key_Equal and mod & Qt.ControlModifier:
            self.zoom_in()
        elif key == Qt.Key_Minus and mod & Qt.ControlModifier:
            self.zoom_out()
        elif key == Qt.Key_0 and mod & Qt.ControlModifier:
            self.zoom_reset()
        else:
            super().keyPressEvent(event)

    # ── Properties ────────────────────────────────────────────────────────────

    @property
    def page_count(self) -> int:
        return len(self._doc) if self._doc else 0

    @property
    def current_page(self) -> int:
        return self._current_page

    @property
    def current_zoom(self) -> float:
        return self._zoom
