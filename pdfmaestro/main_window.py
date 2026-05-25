import os

from PySide6.QtWidgets import (
    QMainWindow, QWidget, QSplitter, QToolBar, QStatusBar,
    QTabWidget, QLabel, QFileDialog, QSpinBox, QComboBox,
    QMessageBox,
)
from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QKeySequence

from pdfmaestro.viewer import PDFViewer
from pdfmaestro.page_manager import PageManagerWidget


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PDFMaestro")
        self.setMinimumSize(1280, 800)

        self._viewer = PDFViewer()
        self._page_manager = PageManagerWidget()
        self._build_ui()
        self._build_menu()
        self._build_toolbar()
        self._build_statusbar()
        self._connect_viewer()

    # ── UI scaffold ───────────────────────────────────────────────────────────

    def _build_ui(self):
        splitter = QSplitter(Qt.Horizontal)

        # Left panel: Pages / Bookmarks / Annotations tabs
        self._left_panel = QTabWidget()
        self._left_panel.setMinimumWidth(180)
        self._left_panel.setMaximumWidth(300)
        self._left_panel.addTab(self._page_manager, "Pages")
        self._left_panel.addTab(QWidget(), "Bookmarks")
        self._left_panel.addTab(QWidget(), "Annotations")

        splitter.addWidget(self._left_panel)
        splitter.addWidget(self._viewer)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)

        self.setCentralWidget(splitter)

    def _build_menu(self):
        mb = self.menuBar()

        # File
        fm = mb.addMenu("&File")
        fm.addAction(self._act("&Open...", "Ctrl+O", self._on_open))
        fm.addAction(self._act("&Save", "Ctrl+S", lambda: None))
        fm.addAction(self._act("Save &As...", "Ctrl+Shift+S", lambda: None))
        fm.addSeparator()
        fm.addAction(self._act("&Merge PDFs...", None, lambda: None))
        fm.addAction(self._act("S&plit PDF...", None, lambda: None))
        fm.addSeparator()
        fm.addAction(self._act("&Quit", "Ctrl+Q", self.close))

        # View
        vm = mb.addMenu("&View")
        vm.addAction(self._act("Zoom &In", "Ctrl+=", self._viewer.zoom_in))
        vm.addAction(self._act("Zoom &Out", "Ctrl+-", self._viewer.zoom_out))
        vm.addAction(self._act("&100%", "Ctrl+0", self._viewer.zoom_reset))
        vm.addAction(self._act("Fit &Width", "Ctrl+W", self._viewer.fit_width))
        vm.addAction(self._act("Fit &Page", "Ctrl+Shift+W", self._viewer.fit_page))
        vm.addSeparator()
        vm.addAction(self._act("&First Page", "Home", self._viewer.first_page))
        vm.addAction(self._act("&Previous Page", "Left", self._viewer.prev_page))
        vm.addAction(self._act("&Next Page", "Right", self._viewer.next_page))
        vm.addAction(self._act("&Last Page", "End", self._viewer.last_page))
        vm.addSeparator()
        vm.addAction(self._act("&Full Screen", "F11", self._toggle_fullscreen))
        vm.addAction(self._act("&Presentation Mode", "F5", lambda: None))

        # Tools
        tm = mb.addMenu("&Tools")
        tm.addAction(self._act("&Insert Signature...", "Ctrl+Shift+G", lambda: None))
        tm.addAction(self._act("&Annotate", None, lambda: None))
        tm.addAction(self._act("&Search...", "Ctrl+F", lambda: None))
        tm.addAction(self._act("&Crop Pages...", None, lambda: None))
        tm.addAction(self._act("&Rotate Pages...", None, lambda: None))
        tm.addAction(self._act("Re&dact...", None, lambda: None))
        tm.addSeparator()
        tm.addAction(self._act("Document &Properties...", None, lambda: None))

    def _build_toolbar(self):
        tb = QToolBar("Main")
        tb.setMovable(False)
        tb.setToolButtonStyle(Qt.ToolButtonTextBesideIcon)
        self.addToolBar(tb)

        # File ops
        tb.addAction(self._act("Open", "Ctrl+O", self._on_open))
        tb.addAction(self._act("Save", "Ctrl+S", lambda: None))
        tb.addSeparator()

        # Navigation
        tb.addAction(self._act("◀", "Left", self._viewer.prev_page))
        self._page_spin = QSpinBox()
        self._page_spin.setMinimum(1)
        self._page_spin.setMaximum(1)
        self._page_spin.setFixedWidth(58)
        self._page_spin.setToolTip("Current page")
        tb.addWidget(self._page_spin)
        self._page_total = QLabel(" / 1")
        tb.addWidget(self._page_total)
        tb.addAction(self._act("▶", "Right", self._viewer.next_page))
        tb.addSeparator()

        # Zoom
        self._zoom_combo = QComboBox()
        self._zoom_combo.addItems([
            "50%", "75%", "100%", "125%", "150%", "200%", "Fit Width", "Fit Page",
        ])
        self._zoom_combo.setCurrentText("100%")
        self._zoom_combo.setEditable(True)
        self._zoom_combo.setFixedWidth(110)
        self._zoom_combo.setToolTip("Zoom level")
        tb.addWidget(self._zoom_combo)
        tb.addSeparator()

        # Document tools
        for label, slot in [
            ("Arrange", lambda: None),
            ("Sign",    lambda: None),
            ("Merge",   lambda: None),
            ("Split",   lambda: None),
            ("Crop",    lambda: None),
            ("Annotate",lambda: None),
            ("Search",  lambda: None),
        ]:
            tb.addAction(self._act(label, None, slot))

    def _build_statusbar(self):
        sb = QStatusBar()
        self._status_page = QLabel("No document open")
        self._status_zoom = QLabel("")
        sb.addWidget(self._status_page)
        sb.addPermanentWidget(self._status_zoom)
        self.setStatusBar(sb)

    # ── Viewer signal wiring ──────────────────────────────────────────────────

    def _connect_viewer(self):
        self._viewer.page_changed.connect(self._on_page_changed)
        self._viewer.zoom_changed.connect(self._on_zoom_changed)
        self._viewer.document_loaded.connect(self._on_document_loaded)

        # Page spinbox -> viewer (user types a page number)
        self._page_spin.valueChanged.connect(
            lambda v: self._viewer.go_to_page(v - 1)
        )

        # Zoom combo -> viewer
        self._zoom_combo.currentIndexChanged.connect(self._on_zoom_combo_selected)
        self._zoom_combo.lineEdit().returnPressed.connect(self._on_zoom_text_entered)

        # Page manager <-> viewer bidirectional sync
        # Thumbnail click → jump viewer to that page
        self._page_manager.page_selected.connect(self._viewer.go_to_page)
        # Viewer scrolls to new page → highlight that thumbnail
        self._viewer.page_changed.connect(
            lambda cur, _total: self._page_manager.set_current_page(cur - 1)
        )

    def _on_document_loaded(self, path: str, count: int):
        name = os.path.basename(path)
        self.setWindowTitle(f"PDFMaestro — {name}")
        self._page_spin.blockSignals(True)
        self._page_spin.setMaximum(count)
        self._page_spin.setValue(1)
        self._page_spin.blockSignals(False)
        self._page_total.setText(f" / {count}")
        # Load thumbnails in the page manager panel
        self._page_manager.load_document(path, count)

    def _on_page_changed(self, current: int, total: int):
        self._status_page.setText(f"Page {current} of {total}")
        self._page_spin.blockSignals(True)
        self._page_spin.setValue(current)
        self._page_spin.blockSignals(False)

    def _on_zoom_changed(self, zoom: float):
        text = f"{round(zoom * 100)}%"
        self._status_zoom.setText(text)
        self._zoom_combo.blockSignals(True)
        self._zoom_combo.setCurrentText(text)
        self._zoom_combo.blockSignals(False)

    def _on_zoom_combo_selected(self, _index: int):
        text = self._zoom_combo.currentText()
        self._apply_zoom_text(text)

    def _on_zoom_text_entered(self):
        self._apply_zoom_text(self._zoom_combo.currentText())

    def _apply_zoom_text(self, text: str):
        if text == "Fit Width":
            self._viewer.fit_width()
        elif text == "Fit Page":
            self._viewer.fit_page()
        else:
            try:
                pct = float(text.rstrip("% "))
                self._viewer.set_zoom(pct / 100.0)
            except ValueError:
                pass

    # ── Actions ───────────────────────────────────────────────────────────────

    def _on_open(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Open PDF", "", "PDF Files (*.pdf);;All Files (*)"
        )
        if not path:
            return
        try:
            self._viewer.load_document(path)
        except Exception as exc:
            QMessageBox.critical(self, "Cannot open file", str(exc))

    def _toggle_fullscreen(self):
        if self.isFullScreen():
            self.showNormal()
        else:
            self.showFullScreen()

    # ── Helpers ───────────────────────────────────────────────────────────────

    @staticmethod
    def _act(label: str, shortcut: str | None, slot) -> QAction:
        a = QAction(label)
        if shortcut:
            a.setShortcut(QKeySequence(shortcut))
        a.triggered.connect(slot)
        return a
