"""
Qt dialogs for PDF operations: Merge, Split, Crop.
Each dialog collects user input and exposes it as properties.
Actual pikepdf work is done in operations.py.
"""

from __future__ import annotations
from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QFormLayout,
    QLabel, QPushButton, QListWidget, QListWidgetItem,
    QRadioButton, QButtonGroup, QSpinBox, QDoubleSpinBox,
    QLineEdit, QFileDialog, QDialogButtonBox, QGroupBox,
    QComboBox, QAbstractItemView, QMessageBox,
)
from PySide6.QtCore import Qt


# ── Merge dialog ──────────────────────────────────────────────────────────────

class MergeDialog(QDialog):
    """
    Lets the user pick additional PDFs and choose where to insert them.
    The current document is always the base; extra files are appended/prepended.
    """

    def __init__(self, current_path: str, page_count: int, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Merge PDFs")
        self.setMinimumWidth(520)
        self._current_path = current_path
        self._page_count = page_count

        layout = QVBoxLayout(self)

        # File list
        layout.addWidget(QLabel("Files to merge (in order):"))
        self._file_list = QListWidget()
        self._file_list.setDragDropMode(QAbstractItemView.InternalMove)
        self._file_list.setDefaultDropAction(Qt.MoveAction)
        # Current doc is pinned at the top
        current_item = QListWidgetItem(f"[Current]  {current_path}")
        current_item.setFlags(current_item.flags() & ~Qt.ItemIsDragEnabled)
        self._file_list.addItem(current_item)
        layout.addWidget(self._file_list)

        # Add / remove buttons
        btn_row = QHBoxLayout()
        add_btn = QPushButton("+ Add PDF…")
        add_btn.clicked.connect(self._add_files)
        remove_btn = QPushButton("Remove selected")
        remove_btn.clicked.connect(self._remove_selected)
        btn_row.addWidget(add_btn)
        btn_row.addWidget(remove_btn)
        btn_row.addStretch()
        layout.addLayout(btn_row)

        # Insert position
        pos_group = QGroupBox("Insert added pages")
        pos_layout = QHBoxLayout(pos_group)
        self._pos_combo = QComboBox()
        self._pos_combo.addItem("After current document",  page_count)
        self._pos_combo.addItem("Before current document", 0)
        pos_layout.addWidget(self._pos_combo)
        pos_layout.addStretch()
        layout.addWidget(pos_group)

        # OK / Cancel
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def _add_files(self):
        paths, _ = QFileDialog.getOpenFileNames(
            self, "Select PDFs to add", "", "PDF Files (*.pdf)"
        )
        for p in paths:
            self._file_list.addItem(QListWidgetItem(p))

    def _remove_selected(self):
        for item in self._file_list.selectedItems():
            # Do not allow removing the current-doc item (row 0)
            if self._file_list.row(item) == 0:
                continue
            self._file_list.takeItem(self._file_list.row(item))

    @property
    def other_paths(self) -> list[str]:
        paths = []
        for row in range(1, self._file_list.count()):
            paths.append(self._file_list.item(row).text())
        return paths

    @property
    def insert_at(self) -> int:
        return self._pos_combo.currentData()


# ── Split dialog ──────────────────────────────────────────────────────────────

class SplitDialog(QDialog):
    """Collect split parameters: mode, ranges/N, output folder, prefix."""

    def __init__(self, total_pages: int, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Split PDF")
        self.setMinimumWidth(460)
        self._total = total_pages

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(f"Total pages: <b>{total_pages}</b>", textFormat=Qt.RichText))

        # Mode
        mode_group = QGroupBox("Split mode")
        mode_layout = QVBoxLayout(mode_group)
        self._mode_group = QButtonGroup(self)

        self._rb_ranges = QRadioButton("By page ranges  (e.g. 1-3, 4-7, 8-)")
        self._rb_every_n = QRadioButton("Every N pages")
        self._rb_each = QRadioButton("Each page as a separate file")
        self._rb_ranges.setChecked(True)

        for i, rb in enumerate([self._rb_ranges, self._rb_every_n, self._rb_each]):
            self._mode_group.addButton(rb, i)
            mode_layout.addWidget(rb)

        layout.addWidget(mode_group)

        # Ranges input
        form = QFormLayout()
        self._ranges_edit = QLineEdit()
        self._ranges_edit.setPlaceholderText(f"e.g. 1-3, 4-{total_pages}")
        form.addRow("Page ranges:", self._ranges_edit)

        self._n_spin = QSpinBox()
        self._n_spin.setMinimum(1)
        self._n_spin.setMaximum(total_pages)
        self._n_spin.setValue(1)
        form.addRow("Pages per file (N):", self._n_spin)

        layout.addLayout(form)

        # Output folder
        out_row = QHBoxLayout()
        self._out_edit = QLineEdit()
        self._out_edit.setPlaceholderText("Output folder…")
        browse_btn = QPushButton("Browse…")
        browse_btn.clicked.connect(self._browse_output)
        out_row.addWidget(self._out_edit)
        out_row.addWidget(browse_btn)
        layout.addLayout(out_row)

        # File name prefix
        self._prefix_edit = QLineEdit("output")
        prefix_row = QFormLayout()
        prefix_row.addRow("File name prefix:", self._prefix_edit)
        layout.addLayout(prefix_row)

        # OK / Cancel
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self._on_accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def _browse_output(self):
        folder = QFileDialog.getExistingDirectory(self, "Choose output folder")
        if folder:
            self._out_edit.setText(folder)

    def _on_accept(self):
        if not self._out_edit.text().strip():
            QMessageBox.warning(self, "No folder", "Please choose an output folder.")
            return
        self.accept()

    @property
    def mode(self) -> str:
        return ["ranges", "every_n", "each"][self._mode_group.checkedId()]

    @property
    def ranges_text(self) -> str:
        return self._ranges_edit.text()

    @property
    def every_n(self) -> int:
        return self._n_spin.value()

    @property
    def out_dir(self) -> str:
        return self._out_edit.text().strip()

    @property
    def base_name(self) -> str:
        return self._prefix_edit.text().strip() or "output"


# ── Crop dialog ───────────────────────────────────────────────────────────────

class CropDialog(QDialog):
    """Collect crop margins in PDF points (72 pt = 1 inch)."""

    def __init__(self, page_count: int, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Crop Pages")
        self.setMinimumWidth(360)

        layout = QVBoxLayout(self)
        layout.addWidget(
            QLabel("Margins to remove (PDF points — 72 pt = 1 inch):"),
        )

        form = QFormLayout()

        def _spin() -> QDoubleSpinBox:
            s = QDoubleSpinBox()
            s.setRange(0, 500)
            s.setDecimals(1)
            s.setSingleStep(5)
            s.setSuffix(" pt")
            return s

        self._top    = _spin()
        self._bottom = _spin()
        self._left   = _spin()
        self._right  = _spin()

        form.addRow("Top:",    self._top)
        form.addRow("Bottom:", self._bottom)
        form.addRow("Left:",   self._left)
        form.addRow("Right:",  self._right)
        layout.addLayout(form)

        # Apply to
        apply_group = QGroupBox("Apply to")
        apply_layout = QVBoxLayout(apply_group)
        self._apply_group = QButtonGroup(self)
        self._rb_current = QRadioButton("Current page only")
        self._rb_all     = QRadioButton("All pages")
        self._rb_range   = QRadioButton("Page range:")
        self._rb_current.setChecked(True)

        self._range_edit = QLineEdit()
        self._range_edit.setPlaceholderText(f"e.g. 1-{page_count}")
        self._range_edit.setEnabled(False)
        self._rb_range.toggled.connect(self._range_edit.setEnabled)

        for i, rb in enumerate([self._rb_current, self._rb_all, self._rb_range]):
            self._apply_group.addButton(rb, i)
            apply_layout.addWidget(rb)
        apply_layout.addWidget(self._range_edit)
        layout.addWidget(apply_group)

        layout.addWidget(QLabel(
            "<small>Visual crop overlay will be added in a future update.</small>",
            textFormat=Qt.RichText,
        ))

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    @property
    def margin_top(self)    -> float: return self._top.value()
    @property
    def margin_bottom(self) -> float: return self._bottom.value()
    @property
    def margin_left(self)   -> float: return self._left.value()
    @property
    def margin_right(self)  -> float: return self._right.value()

    @property
    def apply_to(self) -> str:
        return ["current", "all", "range"][self._apply_group.checkedId()]

    @property
    def range_text(self) -> str:
        return self._range_edit.text()
