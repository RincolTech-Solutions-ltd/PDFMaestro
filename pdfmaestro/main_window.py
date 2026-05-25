from __future__ import annotations
import os
import tempfile

import pikepdf
from PySide6.QtWidgets import (
    QMainWindow, QWidget, QSplitter, QToolBar, QStatusBar,
    QTabWidget, QLabel, QFileDialog, QSpinBox, QComboBox,
    QMessageBox, QDialog, QButtonGroup, QLineEdit, QPushButton,
    QHBoxLayout, QVBoxLayout, QCheckBox, QFrame, QSizePolicy,
    QTreeWidget, QTreeWidgetItem, QStackedWidget, QDialogButtonBox,
    QFormLayout, QRadioButton, QApplication,
)
from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtGui import QAction, QKeySequence, QActionGroup, QColor, QPalette

from pdfmaestro.viewer import PDFViewer
from pdfmaestro.page_manager import PageManagerWidget
from pdfmaestro import operations, annotations
from pdfmaestro.dialogs import MergeDialog, SplitDialog, CropDialog
from pdfmaestro.signature import SignatureDialog, rgba_pil_to_pdf
from pdfmaestro.annotation_overlay import (
    TOOL_POINTER, TOOL_HIGHLIGHT, TOOL_NOTE, TOOL_INK, TOOL_STAMP, TOOL_REDACT,
)
from pdfmaestro import search as search_engine
from pdfmaestro.toc import read_toc
from pdfmaestro import config as cfg


# ── Welcome screen ───────────────────────────────────────────────────────────

class WelcomeWidget(QWidget):
    """Shown in place of the viewer when no document is open."""

    open_requested  = Signal()
    recent_selected = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._init_ui()

    def _init_ui(self):
        root = QVBoxLayout(self)
        root.setAlignment(Qt.AlignHCenter | Qt.AlignVCenter)
        root.setSpacing(20)

        title = QLabel("PDFMaestro")
        title.setStyleSheet("font-size: 32px; font-weight: bold; color: #5b9fe8;")
        title.setAlignment(Qt.AlignHCenter)
        root.addWidget(title)

        sub = QLabel("Open a PDF to get started")
        sub.setStyleSheet("font-size: 14px; color: #888;")
        sub.setAlignment(Qt.AlignHCenter)
        root.addWidget(sub)

        open_btn = QPushButton("Open PDF…")
        open_btn.setFixedWidth(180)
        open_btn.setFixedHeight(40)
        open_btn.setStyleSheet(
            "QPushButton { background: #3d5a8a; color: white; border: none;"
            "  border-radius: 6px; font-size: 14px; }"
            " QPushButton:hover { background: #4e6fa3; }"
        )
        open_btn.clicked.connect(self.open_requested)
        root.addWidget(open_btn, alignment=Qt.AlignHCenter)

        self._recent_frame = QFrame()
        recent_layout = QVBoxLayout(self._recent_frame)
        recent_layout.setSpacing(4)
        recent_lbl = QLabel("Recent files")
        recent_lbl.setStyleSheet("color: #aaa; font-size: 12px;")
        recent_layout.addWidget(recent_lbl)
        self._recent_list_layout = QVBoxLayout()
        self._recent_list_layout.setSpacing(2)
        recent_layout.addLayout(self._recent_list_layout)
        root.addWidget(self._recent_frame, alignment=Qt.AlignHCenter)

        self.refresh_recent()

    def refresh_recent(self):
        # Clear old buttons
        while self._recent_list_layout.count():
            item = self._recent_list_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        recent = cfg.get_recent()
        self._recent_frame.setVisible(bool(recent))
        for path in recent:
            import os
            btn = QPushButton(os.path.basename(path))
            btn.setFixedWidth(300)
            btn.setStyleSheet(
                "QPushButton { text-align: left; padding: 4px 8px; color: #5b9fe8;"
                "  background: transparent; border: none; }"
                " QPushButton:hover { color: #7eb8ff; text-decoration: underline; }"
            )
            btn.setToolTip(path)
            btn.clicked.connect(lambda checked=False, p=path: self.recent_selected.emit(p))
            self._recent_list_layout.addWidget(btn)


# ── Settings dialog ───────────────────────────────────────────────────────────

class SettingsDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Settings")
        self.setMinimumWidth(320)

        layout = QVBoxLayout(self)
        form = QFormLayout()
        form.setSpacing(12)

        # Theme
        theme_row = QHBoxLayout()
        self._light_radio = QRadioButton("Light")
        self._dark_radio  = QRadioButton("Dark")
        (self._dark_radio if cfg.is_dark() else self._light_radio).setChecked(True)
        theme_row.addWidget(self._light_radio)
        theme_row.addWidget(self._dark_radio)
        form.addRow("Theme:", theme_row)

        # Default zoom
        self._zoom_combo = QComboBox()
        self._zoom_combo.addItems(["50%", "75%", "100%", "125%", "150%", "Fit Width", "Fit Page"])
        saved_zoom = cfg.get_settings().value("default_zoom", "100%")
        idx = self._zoom_combo.findText(saved_zoom)
        self._zoom_combo.setCurrentIndex(idx if idx >= 0 else 2)
        form.addRow("Default zoom:", self._zoom_combo)

        layout.addLayout(form)
        layout.addSpacing(8)

        btns = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        btns.accepted.connect(self._save)
        btns.rejected.connect(self.reject)
        layout.addWidget(btns)

    def _save(self):
        cfg.set_dark(self._dark_radio.isChecked())
        cfg.get_settings().setValue("default_zoom", self._zoom_combo.currentText())
        self.accept()


# ── Search bar widget ─────────────────────────────────────────────────────────

class SearchBar(QFrame):
    """Slim find-bar that docks at the bottom of the viewer area."""

    closed = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFrameShape(QFrame.StyledPanel)
        self.setStyleSheet(
            "QFrame { background: #2b3142; border-top: 1px solid #444; }"
            " QLabel { color: #ccc; }"
            " QLineEdit { background: #1e2530; color: #eee; border: 1px solid #555;"
            "   border-radius: 3px; padding: 2px 6px; }"
            " QPushButton { color: #ccc; background: #3a4055; border: 1px solid #555;"
            "   border-radius: 3px; padding: 2px 10px; }"
            " QPushButton:hover { background: #4a5070; }"
        )

        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)
        layout.setSpacing(6)

        self.input = QLineEdit()
        self.input.setPlaceholderText("Find in document…")
        self.input.setFixedWidth(260)
        self.input.setClearButtonEnabled(True)
        layout.addWidget(self.input)

        self.btn_prev = QPushButton("◀")
        self.btn_prev.setFixedWidth(32)
        self.btn_next = QPushButton("▶")
        self.btn_next.setFixedWidth(32)
        layout.addWidget(self.btn_prev)
        layout.addWidget(self.btn_next)

        self.match_label = QLabel("No results")
        layout.addWidget(self.match_label)

        layout.addSpacing(12)
        self.case_check = QCheckBox("Match case")
        self.case_check.setStyleSheet("color: #ccc;")
        layout.addWidget(self.case_check)

        self.word_check = QCheckBox("Whole word")
        self.word_check.setStyleSheet("color: #ccc;")
        layout.addWidget(self.word_check)

        layout.addStretch()

        close_btn = QPushButton("✕")
        close_btn.setFixedWidth(28)
        close_btn.clicked.connect(self.hide)
        layout.addWidget(close_btn)

    def hideEvent(self, event):
        super().hideEvent(event)
        self.closed.emit()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Escape:
            self.hide()
        elif event.key() in (Qt.Key_Return, Qt.Key_Enter):
            self.btn_next.click()
        else:
            super().keyPressEvent(event)

    def set_result(self, current: int, total: int):
        if total == 0:
            self.match_label.setText("No results")
            self.match_label.setStyleSheet("color: #e06060;")
        else:
            self.match_label.setText(f"{current} / {total}")
            self.match_label.setStyleSheet("color: #88cc88;")

    def clear_result(self):
        self.match_label.setText("")
        self.match_label.setStyleSheet("color: #ccc;")


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PDFMaestro")
        self.setMinimumSize(1280, 800)

        # Document state
        self._pdf: pikepdf.Pdf | None = None
        self._current_path: str | None = None
        self._tmp_path: str | None = None
        self._modified: bool = False

        # Search state
        self._search_matches: list[search_engine.SearchMatch] = []
        self._search_idx: int = 0

        # Presentation mode
        self._in_presentation: bool = False

        self._viewer = PDFViewer()
        self._page_manager = PageManagerWidget()
        self._build_ui()
        self._build_menu()
        self._build_toolbar()
        self._build_statusbar()
        self._connect_signals()

    # ── UI scaffold ───────────────────────────────────────────────────────────

    def _build_ui(self):
        splitter = QSplitter(Qt.Horizontal)

        self._left_panel = QTabWidget()
        self._left_panel.setMinimumWidth(180)
        self._left_panel.setMaximumWidth(300)
        self._left_panel.addTab(self._page_manager, "Pages")
        self._left_panel.addTab(self._build_bookmarks_panel(), "Bookmarks")
        self._left_panel.addTab(QWidget(), "Annotations")

        # Right side: stacked (welcome | viewer+searchbar)
        self._view_stack = QStackedWidget()

        self._welcome = WelcomeWidget()
        self._welcome.open_requested.connect(self._on_open)
        self._welcome.recent_selected.connect(self._open_path)
        self._view_stack.addWidget(self._welcome)      # index 0

        view_container = QWidget()
        vc_layout = QVBoxLayout(view_container)
        vc_layout.setContentsMargins(0, 0, 0, 0)
        vc_layout.setSpacing(0)
        vc_layout.addWidget(self._viewer)

        self._search_bar = SearchBar()
        self._search_bar.hide()
        vc_layout.addWidget(self._search_bar)
        self._view_stack.addWidget(view_container)     # index 1

        splitter.addWidget(self._left_panel)
        splitter.addWidget(self._view_stack)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        self.setCentralWidget(splitter)

    def _build_bookmarks_panel(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self._toc_tree = QTreeWidget()
        self._toc_tree.setHeaderLabels(["Title", "Page"])
        self._toc_tree.setColumnWidth(0, 180)
        self._toc_tree.setColumnWidth(1, 40)
        self._toc_tree.setAlternatingRowColors(True)
        self._toc_tree.setAnimated(True)
        self._toc_tree.itemClicked.connect(self._on_toc_item_clicked)

        self._toc_empty_label = QLabel("No bookmarks in this document.")
        self._toc_empty_label.setAlignment(Qt.AlignCenter)
        self._toc_empty_label.setWordWrap(True)
        self._toc_empty_label.setStyleSheet("color: #888; padding: 12px;")

        layout.addWidget(self._toc_tree)
        layout.addWidget(self._toc_empty_label)
        self._toc_tree.hide()
        return w

    def _build_menu(self):
        mb = self.menuBar()

        fm = mb.addMenu("&File")
        fm.addAction(self._act("&Open…",    "Ctrl+O",       self._on_open))
        fm.addAction(self._act("&Save",     "Ctrl+S",       self._on_save))
        fm.addAction(self._act("Save &As…", "Ctrl+Shift+S", self._on_save_as))
        fm.addSeparator()
        self._recent_menu = fm.addMenu("Open &Recent")
        self._rebuild_recent_menu()
        fm.addSeparator()
        fm.addAction(self._act("&Merge PDFs…", None, self._on_merge))
        fm.addAction(self._act("S&plit PDF…",  None, self._on_split))
        fm.addSeparator()
        fm.addAction(self._act("&Quit", "Ctrl+Q", self.close))

        vm = mb.addMenu("&View")
        vm.addAction(self._act("Zoom &In",   "Ctrl+=",       self._viewer.zoom_in))
        vm.addAction(self._act("Zoom &Out",  "Ctrl+-",       self._viewer.zoom_out))
        vm.addAction(self._act("&100%",      "Ctrl+0",       self._viewer.zoom_reset))
        vm.addAction(self._act("Fit &Width", "Ctrl+W",       self._viewer.fit_width))
        vm.addAction(self._act("Fit &Page",  "Ctrl+Shift+W", self._viewer.fit_page))
        vm.addSeparator()
        vm.addAction(self._act("&First Page",    "Home",  self._viewer.first_page))
        vm.addAction(self._act("&Previous Page", "Left",  self._viewer.prev_page))
        vm.addAction(self._act("&Next Page",     "Right", self._viewer.next_page))
        vm.addAction(self._act("&Last Page",     "End",   self._viewer.last_page))
        vm.addSeparator()
        vm.addAction(self._act("&Full Screen",       "F11", self._toggle_fullscreen))
        vm.addAction(self._act("&Presentation Mode", "F5",  self._toggle_presentation))
        vm.addSeparator()
        self._dark_action = self._act("&Dark Mode", "Ctrl+Shift+D", self._toggle_dark)
        self._dark_action.setCheckable(True)
        self._dark_action.setChecked(cfg.is_dark())
        vm.addAction(self._dark_action)
        vm.addSeparator()
        vm.addAction(self._act("&Settings…", None, self._on_settings))

        tm = mb.addMenu("&Tools")
        tm.addAction(self._act("&Insert Signature…", "Ctrl+Shift+G", self._on_sign))
        tm.addAction(self._act("&Highlight",          "Ctrl+Shift+H", lambda: self._viewer.set_annotation_tool(TOOL_HIGHLIGHT)))
        tm.addAction(self._act("Sticky &Note",        "Ctrl+Shift+N", lambda: self._viewer.set_annotation_tool(TOOL_NOTE)))
        tm.addAction(self._act("&Ink / Freehand",     "Ctrl+Shift+I", lambda: self._viewer.set_annotation_tool(TOOL_INK)))
        tm.addAction(self._act("&Stamp",              "Ctrl+Shift+T", lambda: self._viewer.set_annotation_tool(TOOL_STAMP)))
        tm.addAction(self._act("&Search…",           "Ctrl+F", self._open_search))
        tm.addAction(self._act("&Crop Pages…",        None, self._on_crop))
        tm.addAction(self._act("&Rotate CW",          None, lambda: self._rotate_current(90)))
        tm.addAction(self._act("Rotate &CCW",         None, lambda: self._rotate_current(-90)))
        tm.addAction(self._act("Re&dact…",            None, lambda: self._viewer.set_annotation_tool(TOOL_REDACT)))
        tm.addAction(self._act("&OCR…",               None, self._on_ocr))
        tm.addSeparator()
        tm.addAction(self._act("Document &Properties…", None, lambda: None))

    def _build_toolbar(self):
        tb = QToolBar("Main")
        tb.setMovable(False)
        tb.setToolButtonStyle(Qt.ToolButtonTextBesideIcon)
        self.addToolBar(tb)

        tb.addAction(self._act("Open", "Ctrl+O", self._on_open))
        tb.addAction(self._act("Save", "Ctrl+S", self._on_save))
        tb.addSeparator()

        tb.addAction(self._act("◀", None, self._viewer.prev_page))
        self._page_spin = QSpinBox()
        self._page_spin.setMinimum(1)
        self._page_spin.setMaximum(1)
        self._page_spin.setFixedWidth(58)
        self._page_spin.setToolTip("Current page")
        tb.addWidget(self._page_spin)
        self._page_total = QLabel(" / 1")
        tb.addWidget(self._page_total)
        tb.addAction(self._act("▶", None, self._viewer.next_page))
        tb.addSeparator()

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

        for label, slot in [
            ("Merge",  self._on_merge),
            ("Split",  self._on_split),
            ("Crop",   self._on_crop),
            ("Sign",   self._on_sign),
            ("Search", self._open_search),
        ]:
            tb.addAction(self._act(label, None, slot))

        tb.addSeparator()

        # Annotation tool mode group (mutually exclusive, checkable)
        self._tool_group = QActionGroup(self)
        self._tool_group.setExclusive(True)
        for label, tool in [
            ("Pointer",    TOOL_POINTER),
            ("Highlight",  TOOL_HIGHLIGHT),
            ("Note",       TOOL_NOTE),
            ("Ink",        TOOL_INK),
            ("Stamp",      TOOL_STAMP),
            ("Redact",     TOOL_REDACT),
        ]:
            act = QAction(label, self)
            act.setCheckable(True)
            act.setData(tool)
            act.triggered.connect(lambda checked, t=tool: self._viewer.set_annotation_tool(t))
            self._tool_group.addAction(act)
            tb.addAction(act)
        # Default to pointer
        self._tool_group.actions()[0].setChecked(True)

    def _build_statusbar(self):
        sb = QStatusBar()
        self._status_page = QLabel("No document open")
        self._status_zoom = QLabel("")
        self._status_modified = QLabel("")
        sb.addWidget(self._status_page)
        sb.addWidget(self._status_modified)
        sb.addPermanentWidget(self._status_zoom)
        self.setStatusBar(sb)

    # ── Signal wiring ─────────────────────────────────────────────────────────

    def _connect_signals(self):
        self._viewer.page_changed.connect(self._on_page_changed)
        self._viewer.zoom_changed.connect(self._on_zoom_changed)
        self._viewer.document_loaded.connect(self._on_document_loaded)

        self._page_spin.valueChanged.connect(
            lambda v: self._viewer.go_to_page(v - 1)
        )
        self._zoom_combo.currentIndexChanged.connect(self._on_zoom_combo_selected)
        self._zoom_combo.lineEdit().returnPressed.connect(self._on_zoom_text_entered)

        # Page manager ↔ viewer sync
        self._page_manager.page_selected.connect(self._viewer.go_to_page)
        self._viewer.page_changed.connect(
            lambda cur, _total: self._page_manager.set_current_page(cur - 1)
        )

        # Page manager operations → pikepdf mutations
        self._page_manager.order_changed.connect(self._on_order_changed)
        self._page_manager.page_deleted.connect(self._on_page_deleted)
        self._page_manager.page_rotated.connect(self._on_page_rotated)

        # Annotation overlay → pikepdf writers
        self._viewer.annotation_committed.connect(self._on_annotation_committed)

        # Search bar
        self._search_bar.input.textChanged.connect(self._on_search_text_changed)
        self._search_bar.btn_next.clicked.connect(self._search_next)
        self._search_bar.btn_prev.clicked.connect(self._search_prev)
        self._search_bar.case_check.toggled.connect(self._on_search_text_changed)
        self._search_bar.word_check.toggled.connect(self._on_search_text_changed)
        self._search_bar.closed.connect(lambda: self._on_search_bar_visibility(False))

    # ── Document loading ──────────────────────────────────────────────────────

    def _on_document_loaded(self, path: str, count: int):
        name = os.path.basename(path)
        self.setWindowTitle(f"PDFMaestro — {name}")
        self._page_spin.blockSignals(True)
        self._page_spin.setMaximum(count)
        self._page_spin.setValue(1)
        self._page_spin.blockSignals(False)
        self._page_total.setText(f" / {count}")
        self._page_manager.load_document(path, count)
        self._load_toc(path)

    def _load_toc(self, path: str):
        self._toc_tree.clear()
        try:
            import pypdfium2 as pdfium
            doc = pdfium.PdfDocument(path)
            entries = read_toc(doc)
            doc.close()
        except Exception:
            entries = []

        if not entries:
            self._toc_tree.hide()
            self._toc_empty_label.show()
            return

        self._toc_empty_label.hide()
        self._toc_tree.show()

        # Build tree: maintain a stack of (level, QTreeWidgetItem)
        stack: list[tuple[int, QTreeWidgetItem]] = []
        for entry in entries:
            item = QTreeWidgetItem()
            item.setText(0, entry.title)
            item.setText(1, str(entry.page_idx + 1) if entry.page_idx >= 0 else "—")
            item.setData(0, Qt.UserRole, entry.page_idx)
            item.setToolTip(0, entry.title)

            # Pop stack until we find a parent at level < entry.level
            while stack and stack[-1][0] >= entry.level:
                stack.pop()

            if stack:
                stack[-1][1].addChild(item)
            else:
                self._toc_tree.addTopLevelItem(item)

            stack.append((entry.level, item))

        self._toc_tree.expandToDepth(1)

    def _on_toc_item_clicked(self, item: QTreeWidgetItem, _column: int):
        page_idx = item.data(0, Qt.UserRole)
        if page_idx is not None and page_idx >= 0:
            self._viewer.go_to_page(page_idx)

    # ── Viewer / status bar sync ──────────────────────────────────────────────

    def _on_page_changed(self, current: int, total: int):
        self._status_page.setText(f"Page {current} of {total}")
        self._page_spin.blockSignals(True)
        self._page_spin.setValue(current)
        self._page_spin.blockSignals(False)
        self._sync_toc_selection(current - 1)

    def _sync_toc_selection(self, page_idx: int):
        """Select the last TOC item whose page is <= current page."""
        if not self._toc_tree.isVisible():
            return
        best: QTreeWidgetItem | None = None

        def _walk(item: QTreeWidgetItem):
            nonlocal best
            p = item.data(0, Qt.UserRole)
            if p is not None and 0 <= p <= page_idx:
                best = item
            for i in range(item.childCount()):
                _walk(item.child(i))

        for i in range(self._toc_tree.topLevelItemCount()):
            _walk(self._toc_tree.topLevelItem(i))

        if best:
            self._toc_tree.blockSignals(True)
            self._toc_tree.setCurrentItem(best)
            self._toc_tree.blockSignals(False)

    def _on_zoom_changed(self, zoom: float):
        text = f"{round(zoom * 100)}%"
        self._status_zoom.setText(text)
        self._zoom_combo.blockSignals(True)
        self._zoom_combo.setCurrentText(text)
        self._zoom_combo.blockSignals(False)

    def _on_zoom_combo_selected(self, _index: int):
        self._apply_zoom_text(self._zoom_combo.currentText())

    def _on_zoom_text_entered(self):
        self._apply_zoom_text(self._zoom_combo.currentText())

    def _apply_zoom_text(self, text: str):
        if text == "Fit Width":
            self._viewer.fit_width()
        elif text == "Fit Page":
            self._viewer.fit_page()
        else:
            try:
                self._viewer.set_zoom(float(text.rstrip("% ")) / 100.0)
            except ValueError:
                pass

    # ── pikepdf state helpers ─────────────────────────────────────────────────

    def _require_pdf(self) -> bool:
        if self._pdf is None:
            QMessageBox.information(self, "No document", "Open a PDF first.")
            return False
        return True

    def _get_tmp_path(self) -> str:
        if self._tmp_path is None:
            tmp = tempfile.NamedTemporaryFile(suffix=".pdf", delete=False)
            tmp.close()
            self._tmp_path = tmp.name
        return self._tmp_path

    def _reload_viewer(self):
        """Save current pikepdf doc to temp file and reload just the viewer."""
        tmp = self._get_tmp_path()
        self._pdf.save(tmp)
        cur = self._viewer.current_page
        self._viewer.load_document(tmp)
        self._viewer.go_to_page(cur)
        self._set_modified(True)

    def _set_modified(self, value: bool):
        self._modified = value
        self._status_modified.setText("● Modified" if value else "")
        name = os.path.basename(self._current_path) if self._current_path else "PDFMaestro"
        prefix = "● " if value else ""
        self.setWindowTitle(f"{prefix}PDFMaestro — {name}")

    # ── File actions ──────────────────────────────────────────────────────────

    def _on_open(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Open PDF", "", "PDF Files (*.pdf);;All Files (*)"
        )
        if path:
            self._open_path(path)

    def _open_path(self, path: str):
        try:
            if self._pdf:
                self._pdf.close()
            self._pdf = pikepdf.Pdf.open(path)
            self._current_path = path
            self._set_modified(False)
            self._viewer.load_document(path)
            self._view_stack.setCurrentIndex(1)
            cfg.add_recent(path)
            self._rebuild_recent_menu()
            self._welcome.refresh_recent()
        except Exception as exc:
            QMessageBox.critical(self, "Cannot open file", str(exc))

    def _on_save(self):
        if not self._require_pdf():
            return
        try:
            self._pdf.save(self._current_path)
            self._set_modified(False)
        except Exception as exc:
            QMessageBox.critical(self, "Save failed", str(exc))

    def _on_save_as(self):
        if not self._require_pdf():
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Save As", self._current_path or "", "PDF Files (*.pdf)"
        )
        if not path:
            return
        try:
            self._pdf.save(path)
            self._current_path = path
            self._set_modified(False)
            self.setWindowTitle(f"PDFMaestro — {os.path.basename(path)}")
        except Exception as exc:
            QMessageBox.critical(self, "Save failed", str(exc))

    # ── Page manager → pikepdf ────────────────────────────────────────────────

    def _on_order_changed(self, order: list[int]):
        if not self._require_pdf():
            return
        operations.apply_page_order(self._pdf, order)
        self._page_manager.reset_indices()
        self._reload_viewer()

    def _on_page_deleted(self, original_index: int):
        if not self._require_pdf():
            return
        operations.delete_pages(self._pdf, {original_index})
        self._reload_viewer()

    def _on_page_rotated(self, original_index: int, degrees: int):
        if not self._require_pdf():
            return
        operations.rotate_page(self._pdf, original_index, degrees)
        self._reload_viewer()

    def _rotate_current(self, degrees: int):
        if not self._require_pdf():
            return
        idx = self._viewer.current_page
        operations.rotate_page(self._pdf, idx, degrees)
        self._reload_viewer()

    # ── Merge ─────────────────────────────────────────────────────────────────

    def _on_merge(self):
        if not self._require_pdf():
            return
        dlg = MergeDialog(self._current_path, len(self._pdf.pages), self)
        if not dlg.exec():
            return
        if not dlg.other_paths:
            QMessageBox.information(self, "Nothing to merge", "Add at least one PDF.")
            return
        try:
            operations.merge_into(self._pdf, dlg.other_paths, dlg.insert_at)
            # Update page manager with new page count
            tmp = self._get_tmp_path()
            self._pdf.save(tmp)
            count = len(self._pdf.pages)
            self._page_manager.load_document(tmp, count)
            self._page_spin.setMaximum(count)
            self._page_total.setText(f" / {count}")
            self._reload_viewer()
        except Exception as exc:
            QMessageBox.critical(self, "Merge failed", str(exc))

    # ── Split ─────────────────────────────────────────────────────────────────

    def _on_split(self):
        if not self._require_pdf():
            return
        total = len(self._pdf.pages)
        dlg = SplitDialog(total, self)
        if not dlg.exec():
            return
        try:
            if dlg.mode == "ranges":
                ranges = operations.parse_ranges(dlg.ranges_text, total)
                if not ranges:
                    QMessageBox.warning(self, "Invalid ranges",
                                        "Could not parse any valid page ranges.")
                    return
                out = operations.split_by_ranges(self._pdf, ranges, dlg.out_dir, dlg.base_name)
            elif dlg.mode == "every_n":
                out = operations.split_every_n(self._pdf, dlg.every_n, dlg.out_dir, dlg.base_name)
            else:
                out = operations.split_each_page(self._pdf, dlg.out_dir, dlg.base_name)

            QMessageBox.information(
                self, "Split complete",
                f"Created {len(out)} file(s) in:\n{dlg.out_dir}"
            )
        except Exception as exc:
            QMessageBox.critical(self, "Split failed", str(exc))

    # ── Crop ──────────────────────────────────────────────────────────────────

    def _on_crop(self):
        if not self._require_pdf():
            return
        total = len(self._pdf.pages)
        dlg = CropDialog(total, self)
        if not dlg.exec():
            return
        try:
            kw = dict(
                margin_left=dlg.margin_left,
                margin_bottom=dlg.margin_bottom,
                margin_right=dlg.margin_right,
                margin_top=dlg.margin_top,
            )
            if dlg.apply_to == "all":
                operations.crop_all_pages(self._pdf, **kw)
            elif dlg.apply_to == "current":
                operations.crop_page(self._pdf, self._viewer.current_page, **kw)
            else:
                ranges = operations.parse_ranges(dlg.range_text, total)
                for start, end in ranges:
                    for i in range(start, end + 1):
                        operations.crop_page(self._pdf, i, **kw)
            self._reload_viewer()
        except Exception as exc:
            QMessageBox.critical(self, "Crop failed", str(exc))

    # ── Search ────────────────────────────────────────────────────────────────

    def _open_search(self):
        self._search_bar.show()
        self._search_bar.input.setFocus()
        self._search_bar.input.selectAll()

    def _on_search_bar_visibility(self, visible: bool):
        if not visible:
            self._clear_search_highlights()
            self._search_matches = []
            self._search_idx = 0
            self._search_bar.clear_result()

    def _on_search_text_changed(self, *_args):
        query = self._search_bar.input.text().strip()
        self._clear_search_highlights()
        self._search_matches = []
        self._search_idx = 0

        if not query or not self._pdf or not self._current_path:
            self._search_bar.set_result(0, 0)
            return

        # Search runs on the saved/temp file so pypdfium2 can open it
        src = self._current_path
        if self._modified and self._tmp_path:
            self._pdf.save(self._tmp_path)
            src = self._tmp_path

        try:
            import pypdfium2 as pdfium
            doc = pdfium.PdfDocument(src)
            self._search_matches = search_engine.find_all(
                doc, query,
                match_case=self._search_bar.case_check.isChecked(),
                whole_word=self._search_bar.word_check.isChecked(),
            )
            doc.close()
        except Exception:
            self._search_matches = []

        total = len(self._search_matches)
        self._search_bar.set_result(1 if total else 0, total)
        if total:
            self._search_idx = 0
            self._apply_search_highlights()
            self._viewer.go_to_page(self._search_matches[0].page_idx)

    def _search_next(self):
        if not self._search_matches:
            return
        self._search_idx = (self._search_idx + 1) % len(self._search_matches)
        self._search_bar.set_result(self._search_idx + 1, len(self._search_matches))
        self._viewer.go_to_page(self._search_matches[self._search_idx].page_idx)

    def _search_prev(self):
        if not self._search_matches:
            return
        self._search_idx = (self._search_idx - 1) % len(self._search_matches)
        self._search_bar.set_result(self._search_idx + 1, len(self._search_matches))
        self._viewer.go_to_page(self._search_matches[self._search_idx].page_idx)

    def _apply_search_highlights(self):
        """Write temporary highlight annotations for all search matches."""
        if not self._pdf:
            return
        for match in self._search_matches:
            for rect in match.rects:
                x0, y0, x1, y1 = rect
                quad = (x0, y0, x1, y0, x0, y1, x1, y1)
                annotations.add_highlight(
                    self._pdf, match.page_idx, [quad],
                    color=(1.0, 0.5, 0.0), opacity=0.35,
                )
        self._reload_viewer()

    def _clear_search_highlights(self):
        """Remove all search highlight annotations (orange ones) from every page."""
        if not self._pdf:
            return
        changed = False
        orange = (1.0, 0.5, 0.0)
        for page in self._pdf.pages:
            annots_raw = page.get("/Annots")
            if annots_raw is None:
                continue
            surviving = []
            for annot in annots_raw:
                keep = True
                if str(annot.get("/Subtype", "")) == "/Highlight":
                    c = annot.get("/C")
                    if c and len(c) == 3:
                        color = tuple(round(float(v), 1) for v in c)
                        if color == tuple(round(v, 1) for v in orange):
                            keep = False
                if keep:
                    surviving.append(annot)
            if len(surviving) != len(list(annots_raw)):
                page["/Annots"] = pikepdf.Array(surviving)
                changed = True
        if changed:
            self._reload_viewer()

    # ── Annotations ───────────────────────────────────────────────────────────

    def _on_annotation_committed(self, payload: dict):
        if not self._require_pdf():
            return
        try:
            kind     = payload["type"]
            page_idx = payload["page_idx"]

            if kind == "highlight":
                annotations.add_highlight(
                    self._pdf, page_idx,
                    quads   = payload["quads"],
                    color   = payload.get("color",   (1.0, 0.9, 0.0)),
                    opacity = payload.get("opacity", 0.4),
                )
            elif kind == "note":
                annotations.add_text_note(
                    self._pdf, page_idx,
                    x        = payload["x"],
                    y        = payload["y"],
                    contents = payload["contents"],
                )
            elif kind == "ink":
                annotations.add_ink(
                    self._pdf, page_idx,
                    strokes = payload["strokes"],
                    color   = payload.get("color", (0.0, 0.0, 0.8)),
                    width   = payload.get("width", 2.0),
                )
            elif kind == "stamp":
                annotations.add_stamp(
                    self._pdf, page_idx,
                    x    = payload["x"],
                    y    = payload["y"],
                    name = payload.get("name", "Approved"),
                )
            elif kind == "redact":
                annotations.add_redact(
                    self._pdf, page_idx,
                    x0 = payload["x0"], y0 = payload["y0"],
                    x1 = payload["x1"], y1 = payload["y1"],
                )
            else:
                return

            self._reload_viewer()
        except Exception as exc:
            QMessageBox.critical(self, "Annotation failed", str(exc))

    # ── Sign ──────────────────────────────────────────────────────────────────

    def _on_sign(self):
        if not self._require_pdf():
            return
        dlg = SignatureDialog(self)
        if not dlg.exec():
            return
        img, _ = dlg.get_result()
        if img is None:
            QMessageBox.warning(
                self, "No signature",
                "Please enter a name, draw a signature, or upload an image first.",
            )
            return
        import tempfile, os
        tmp_dir = tempfile.gettempdir()
        sig_path = None
        try:
            sig_path = rgba_pil_to_pdf(img, tmp_dir)
            operations.overlay_signature_on_page(
                self._pdf, self._viewer.current_page, sig_path,
                position="bottom-right",
            )
            self._reload_viewer()
        except Exception as exc:
            QMessageBox.critical(self, "Signature failed", str(exc))
        finally:
            if sig_path:
                try:
                    os.unlink(sig_path)
                except OSError:
                    pass

    # ── View / theme / settings ───────────────────────────────────────────────

    def _toggle_fullscreen(self):
        if self.isFullScreen():
            self.showNormal()
        else:
            self.showFullScreen()

    def _toggle_presentation(self):
        if self._in_presentation:
            self._exit_presentation()
        else:
            self._enter_presentation()

    def _enter_presentation(self):
        self._in_presentation = True
        self.menuBar().hide()
        for tb in self.findChildren(QToolBar):
            tb.hide()
        self._left_panel.hide()
        self.showFullScreen()
        self._viewer.setFocus()

    def _exit_presentation(self):
        self._in_presentation = False
        self.menuBar().show()
        for tb in self.findChildren(QToolBar):
            tb.show()
        self._left_panel.show()
        self.showNormal()

    def keyPressEvent(self, event):
        if self._in_presentation:
            k = event.key()
            if k == Qt.Key_Escape:
                self._exit_presentation()
                return
            if k in (Qt.Key_Right, Qt.Key_Down, Qt.Key_Space, Qt.Key_PageDown):
                self._viewer.next_page()
                return
            if k in (Qt.Key_Left, Qt.Key_Up, Qt.Key_PageUp):
                self._viewer.prev_page()
                return
        super().keyPressEvent(event)

    def _toggle_dark(self):
        dark = self._dark_action.isChecked()
        cfg.set_dark(dark)
        cfg.apply_theme(QApplication.instance(), dark)

    def _on_settings(self):
        dlg = SettingsDialog(self)
        if dlg.exec():
            dark = cfg.is_dark()
            self._dark_action.setChecked(dark)
            cfg.apply_theme(QApplication.instance(), dark)

    def _on_ocr(self):
        QMessageBox.information(
            self, "OCR",
            "OCR support requires Tesseract.\n\n"
            "Install with:\n  sudo apt install tesseract-ocr\n\n"
            "Full OCR integration is planned for a future release.",
        )

    def _rebuild_recent_menu(self):
        self._recent_menu.clear()
        recent = cfg.get_recent()
        if not recent:
            self._recent_menu.addAction("(empty)").setEnabled(False)
            return
        for path in recent:
            act = self._recent_menu.addAction(os.path.basename(path))
            act.setToolTip(path)
            act.triggered.connect(lambda checked=False, p=path: self._open_path(p))
        self._recent_menu.addSeparator()
        self._recent_menu.addAction("Clear Recent", cfg.clear_recent)

    # ── Helper ────────────────────────────────────────────────────────────────

    @staticmethod
    def _act(label: str, shortcut: str | None, slot) -> QAction:
        a = QAction(label)
        if shortcut:
            a.setShortcut(QKeySequence(shortcut))
        a.triggered.connect(slot)
        return a
