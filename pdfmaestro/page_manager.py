import pypdfium2 as pdfium
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QListView, QAbstractItemView, QMenu, QLabel,
)
from PySide6.QtCore import (
    Qt, Signal, QModelIndex, QSize, QPoint, QObject, QRunnable, QThreadPool,
)
from PySide6.QtGui import (
    QStandardItemModel, QStandardItem, QIcon, QPixmap, QPainter, QColor, QFont,
)

THUMB_W = 140      # rendered thumbnail width (px)
RENDER_DPI = 72    # DPI used before downscaling to THUMB_W


# ── Background thumbnail renderer ─────────────────────────────────────────────

class _ThumbSignals(QObject):
    done = Signal(int, QPixmap)   # (row, pixmap)


class _ThumbWorker(QRunnable):
    def __init__(self, doc_path: str, page_index: int, row: int):
        super().__init__()
        self.signals = _ThumbSignals()
        self._path = doc_path
        self._page_index = page_index
        self._row = row

    def run(self):
        try:
            doc = pdfium.PdfDocument(self._path)
            page = doc[self._page_index]
            bitmap = page.render(scale=RENDER_DPI / 72.0)
            from PIL import Image
            pil = bitmap.to_pil().convert("RGBA")
            raw = bytes(pil.tobytes("raw", "RGBA"))
            from PySide6.QtGui import QImage
            qimg = QImage(raw, pil.width, pil.height, pil.width * 4,
                          QImage.Format_RGBA8888).copy()
            pixmap = QPixmap.fromImage(qimg).scaledToWidth(
                THUMB_W, Qt.SmoothTransformation
            )
            doc.close()
            self.signals.done.emit(self._row, pixmap)
        except Exception:
            pass


def _placeholder_pixmap(label: str) -> QPixmap:
    h = int(THUMB_W * 1.414)   # A4 ratio
    pm = QPixmap(THUMB_W, h)
    pm.fill(QColor("#cdd5e0"))
    painter = QPainter(pm)
    painter.setPen(QColor("#8898aa"))
    f = QFont()
    f.setPointSize(8)
    painter.setFont(f)
    painter.drawText(pm.rect(), Qt.AlignCenter, label)
    painter.end()
    return pm


# ── Drag-aware list view ───────────────────────────────────────────────────────

class _PageListView(QListView):
    """QListView that emits a signal after a successful internal drag-drop."""
    reordered = Signal()

    def dropEvent(self, event):
        super().dropEvent(event)
        self.reordered.emit()


# ── Public widget ─────────────────────────────────────────────────────────────

class PageManagerWidget(QWidget):
    page_selected = Signal(int)        # 0-based page index user clicked
    order_changed = Signal(list)       # new order as list[int] of original indices
    page_deleted  = Signal(int)        # original page index that was deleted
    page_rotated  = Signal(int, int)   # (original_index, degrees)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._doc_path: str | None = None
        self._loading = False
        self._pool = QThreadPool.globalInstance()

        self._model = QStandardItemModel(self)

        self._list = _PageListView()
        self._list.setModel(self._model)
        self._list.setViewMode(QListView.IconMode)
        self._list.setFlow(QListView.TopToBottom)
        self._list.setWrapping(False)
        self._list.setResizeMode(QListView.Adjust)
        self._list.setIconSize(QSize(THUMB_W, int(THUMB_W * 1.414)))
        self._list.setGridSize(QSize(THUMB_W + 20, int(THUMB_W * 1.414) + 28))
        self._list.setSpacing(4)
        self._list.setMovement(QListView.Snap)
        self._list.setUniformItemSizes(True)

        # Drag-and-drop
        self._list.setDragEnabled(True)
        self._list.setAcceptDrops(True)
        self._list.setDropIndicatorShown(True)
        self._list.setDragDropMode(QAbstractItemView.InternalMove)
        self._list.setDefaultDropAction(Qt.MoveAction)
        self._list.setSelectionMode(QAbstractItemView.SingleSelection)

        self._list.setContextMenuPolicy(Qt.CustomContextMenu)
        self._list.customContextMenuRequested.connect(self._on_context_menu)
        self._list.activated.connect(self._on_activated)
        self._list.selectionModel().currentChanged.connect(self._on_current_changed)
        self._list.reordered.connect(self._on_reordered)

        self._empty_label = QLabel("Open a PDF\nto see pages")
        self._empty_label.setAlignment(Qt.AlignCenter)
        self._empty_label.setStyleSheet("color: #6a7a8c; font-size: 13px;")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self._empty_label)
        layout.addWidget(self._list)
        self._list.hide()

    # ── Public API ────────────────────────────────────────────────────────────

    def load_document(self, doc_path: str, page_count: int):
        self._doc_path = doc_path
        self._loading = True
        self._model.clear()

        for i in range(page_count):
            label = f"Page {i + 1}"
            item = QStandardItem()
            item.setIcon(QIcon(_placeholder_pixmap(label)))
            item.setText(label)
            item.setData(i, Qt.UserRole)          # original page index
            item.setData(0, Qt.UserRole + 1)      # rotation (degrees)
            item.setEditable(False)
            item.setDropEnabled(False)
            self._model.appendRow(item)

        self._empty_label.hide()
        self._list.show()
        self._loading = False

        # Kick off background thumbnail renders
        for i in range(page_count):
            worker = _ThumbWorker(doc_path, i, i)
            worker.signals.done.connect(self._on_thumb_ready)
            self._pool.start(worker)

    def set_current_page(self, index: int):
        """Highlight the thumbnail for the given 0-based page index (no signal)."""
        if index < 0 or index >= self._model.rowCount():
            return
        idx = self._model.index(index, 0)
        self._list.selectionModel().blockSignals(True)
        self._list.setCurrentIndex(idx)
        self._list.scrollTo(idx, QAbstractItemView.EnsureVisible)
        self._list.selectionModel().blockSignals(False)

    def get_current_order(self) -> list[int]:
        """Return the current page order as a list of original 0-based indices."""
        return [
            self._model.item(r).data(Qt.UserRole)
            for r in range(self._model.rowCount())
        ]

    # ── Slots ─────────────────────────────────────────────────────────────────

    def _on_thumb_ready(self, row: int, pixmap: QPixmap):
        item = self._model.item(row)
        if item:
            item.setIcon(QIcon(pixmap))

    def _on_current_changed(self, current: QModelIndex, _prev: QModelIndex):
        if current.isValid() and not self._loading:
            self.page_selected.emit(current.row())

    def _on_activated(self, index: QModelIndex):
        if index.isValid():
            self.page_selected.emit(index.row())

    def _on_reordered(self):
        self.order_changed.emit(self.get_current_order())

    # ── Context menu ──────────────────────────────────────────────────────────

    def _on_context_menu(self, pos: QPoint):
        index = self._list.indexAt(pos)
        if not index.isValid():
            return
        row = index.row()
        menu = QMenu(self)
        menu.addAction("Rotate 90° CW",   lambda: self._rotate(row, 90))
        menu.addAction("Rotate 90° CCW",  lambda: self._rotate(row, -90))
        menu.addSeparator()
        menu.addAction("Delete Page",     lambda: self._delete(row))
        menu.addAction("Extract Page...", lambda: self._extract(row))
        menu.addSeparator()
        menu.addAction("Insert Blank Page Before", lambda: None)  # Phase 4
        menu.exec(self._list.viewport().mapToGlobal(pos))

    def reset_indices(self):
        """After any pikepdf mutation, renumber UserRole to match current row order."""
        for r in range(self._model.rowCount()):
            self._model.item(r).setData(r, Qt.UserRole)
            self._model.item(r).setText(f"Page {r + 1}")

    def _rotate(self, row: int, degrees: int):
        item = self._model.item(row)
        if not item:
            return
        orig_idx = item.data(Qt.UserRole)
        rotation = (item.data(Qt.UserRole + 1) or 0) + degrees
        item.setData(rotation % 360, Qt.UserRole + 1)
        self.page_rotated.emit(orig_idx, degrees)

    def _delete(self, row: int):
        if self._model.rowCount() <= 1:
            return
        orig_idx = self._model.item(row).data(Qt.UserRole)
        self._model.removeRow(row)
        self.page_deleted.emit(orig_idx)

    def _extract(self, row: int):
        # Phase 4 — save single page as new PDF
        pass
