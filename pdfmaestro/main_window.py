from __future__ import annotations
import os
import tempfile

import pikepdf
from PySide6.QtWidgets import (
    QMainWindow, QWidget, QSplitter, QToolBar, QStatusBar,
    QTabWidget, QLabel, QFileDialog, QSpinBox, QComboBox,
    QMessageBox, QDialog,
)
from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QKeySequence

from pdfmaestro.viewer import PDFViewer
from pdfmaestro.page_manager import PageManagerWidget
from pdfmaestro import operations
from pdfmaestro.dialogs import MergeDialog, SplitDialog, CropDialog


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PDFMaestro")
        self.setMinimumSize(1280, 800)

        # Document state
        self._pdf: pikepdf.Pdf | None = None
        self._current_path: str | None = None
        self._tmp_path: str | None = None      # reused temp file for viewer reload
        self._modified: bool = False

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
        self._left_panel.addTab(QWidget(), "Bookmarks")
        self._left_panel.addTab(QWidget(), "Annotations")

        splitter.addWidget(self._left_panel)
        splitter.addWidget(self._viewer)
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        self.setCentralWidget(splitter)

    def _build_menu(self):
        mb = self.menuBar()

        fm = mb.addMenu("&File")
        fm.addAction(self._act("&Open…",    "Ctrl+O",       self._on_open))
        fm.addAction(self._act("&Save",     "Ctrl+S",       self._on_save))
        fm.addAction(self._act("Save &As…", "Ctrl+Shift+S", self._on_save_as))
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
        vm.addAction(self._act("&Full Screen",      "F11", self._toggle_fullscreen))
        vm.addAction(self._act("&Presentation Mode","F5",  lambda: None))

        tm = mb.addMenu("&Tools")
        tm.addAction(self._act("&Insert Signature…", "Ctrl+Shift+G", lambda: None))
        tm.addAction(self._act("&Annotate",           None, lambda: None))
        tm.addAction(self._act("&Search…",           "Ctrl+F", lambda: None))
        tm.addAction(self._act("&Crop Pages…",        None, self._on_crop))
        tm.addAction(self._act("&Rotate CW",          None, lambda: self._rotate_current(90)))
        tm.addAction(self._act("Rotate &CCW",         None, lambda: self._rotate_current(-90)))
        tm.addAction(self._act("Re&dact…",            None, lambda: None))
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
            ("Merge",    self._on_merge),
            ("Split",    self._on_split),
            ("Crop",     self._on_crop),
            ("Sign",     lambda: None),
            ("Annotate", lambda: None),
            ("Search",   lambda: None),
        ]:
            tb.addAction(self._act(label, None, slot))

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

    # ── Viewer / status bar sync ──────────────────────────────────────────────

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
        if not path:
            return
        try:
            if self._pdf:
                self._pdf.close()
            self._pdf = pikepdf.Pdf.open(path)
            self._current_path = path
            self._set_modified(False)
            self._viewer.load_document(path)
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

    # ── View ──────────────────────────────────────────────────────────────────

    def _toggle_fullscreen(self):
        if self.isFullScreen():
            self.showNormal()
        else:
            self.showFullScreen()

    # ── Helper ────────────────────────────────────────────────────────────────

    @staticmethod
    def _act(label: str, shortcut: str | None, slot) -> QAction:
        a = QAction(label)
        if shortcut:
            a.setShortcut(QKeySequence(shortcut))
        a.triggered.connect(slot)
        return a
