from PySide6.QtWidgets import (
    QMainWindow, QWidget, QSplitter, QToolBar, QStatusBar,
    QTabWidget, QLabel, QFileDialog,
)
from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QKeySequence


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PDFMaestro")
        self.setMinimumSize(1280, 800)
        self._current_path: str | None = None

        self._build_ui()
        self._build_menu()
        self._build_toolbar()
        self._build_statusbar()

    # ── UI scaffold ───────────────────────────────────────────────────────────

    def _build_ui(self):
        splitter = QSplitter(Qt.Horizontal)

        # Left panel: Pages / Bookmarks / Annotations tabs
        self._left_panel = QTabWidget()
        self._left_panel.setMinimumWidth(180)
        self._left_panel.setMaximumWidth(300)
        self._left_panel.addTab(QWidget(), "Pages")
        self._left_panel.addTab(QWidget(), "Bookmarks")
        self._left_panel.addTab(QWidget(), "Annotations")

        # Center: PDF viewer (placeholder until Phase 2)
        self._viewer_placeholder = QLabel("Open a PDF to get started")
        self._viewer_placeholder.setAlignment(Qt.AlignCenter)
        self._viewer_placeholder.setStyleSheet(
            "background:#1e2530; color:#6a7a8c; font-size:17px; font-family:sans-serif;"
        )

        splitter.addWidget(self._left_panel)
        splitter.addWidget(self._viewer_placeholder)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)

        self.setCentralWidget(splitter)

    def _build_menu(self):
        mb = self.menuBar()

        # File
        fm = mb.addMenu("&File")
        fm.addAction(self._action("&Open...", "Ctrl+O", self._on_open))
        fm.addAction(self._action("&Save", "Ctrl+S", lambda: None))
        fm.addAction(self._action("Save &As...", "Ctrl+Shift+S", lambda: None))
        fm.addSeparator()
        fm.addAction(self._action("&Merge PDFs...", None, lambda: None))
        fm.addAction(self._action("S&plit PDF...", None, lambda: None))
        fm.addSeparator()
        fm.addAction(self._action("&Quit", "Ctrl+Q", self.close))

        # View
        vm = mb.addMenu("&View")
        vm.addAction(self._action("Zoom &In", "Ctrl++", lambda: None))
        vm.addAction(self._action("Zoom &Out", "Ctrl+-", lambda: None))
        vm.addAction(self._action("Fit &Width", "Ctrl+W", lambda: None))
        vm.addAction(self._action("Fit &Page", "Ctrl+Shift+W", lambda: None))
        vm.addSeparator()
        vm.addAction(self._action("&Full Screen", "F11", lambda: None))
        vm.addAction(self._action("&Presentation Mode", "F5", lambda: None))

        # Tools
        tm = mb.addMenu("&Tools")
        tm.addAction(self._action("&Insert Signature...", "Ctrl+Shift+G", lambda: None))
        tm.addAction(self._action("&Annotate", None, lambda: None))
        tm.addAction(self._action("&Search...", "Ctrl+F", lambda: None))
        tm.addAction(self._action("&Crop Pages...", None, lambda: None))
        tm.addAction(self._action("&Rotate Pages...", None, lambda: None))
        tm.addAction(self._action("Re&dact...", None, lambda: None))
        tm.addSeparator()
        tm.addAction(self._action("Document &Properties...", None, lambda: None))

    def _build_toolbar(self):
        tb = QToolBar("Main")
        tb.setMovable(False)
        tb.setToolButtonStyle(Qt.ToolButtonTextBesideIcon)
        self.addToolBar(tb)

        items = [
            ("Open",     "Ctrl+O",        self._on_open),
            ("Arrange",  None,            lambda: None),
            ("Sign",     "Ctrl+Shift+G",  lambda: None),
            ("Merge",    None,            lambda: None),
            ("Split",    None,            lambda: None),
            ("Crop",     None,            lambda: None),
            ("Annotate", None,            lambda: None),
            ("Search",   "Ctrl+F",        lambda: None),
        ]
        for label, shortcut, slot in items:
            action = self._action(label, shortcut, slot)
            tb.addAction(action)

    def _build_statusbar(self):
        sb = QStatusBar()
        self._page_label = QLabel("No document open")
        self._zoom_label = QLabel("")
        sb.addWidget(self._page_label)
        sb.addPermanentWidget(self._zoom_label)
        self.setStatusBar(sb)

    # ── Helpers ───────────────────────────────────────────────────────────────

    @staticmethod
    def _action(label: str, shortcut: str | None, slot) -> QAction:
        a = QAction(label)
        if shortcut:
            a.setShortcut(QKeySequence(shortcut))
        a.triggered.connect(slot)
        return a

    # ── Slots ─────────────────────────────────────────────────────────────────

    def _on_open(self):
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Open PDF",
            "",
            "PDF Files (*.pdf);;All Files (*)",
        )
        if not path:
            return
        self._current_path = path
        self.setWindowTitle(f"PDFMaestro — {path.split('/')[-1]}")
        self._page_label.setText(f"Opened: {path}")
