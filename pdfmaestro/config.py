from __future__ import annotations
from PySide6.QtCore import QSettings
from PySide6.QtGui import QPalette, QColor
from PySide6.QtWidgets import QApplication
from pdfmaestro import __app_name__, __org_name__

_MAX_RECENT = 8


def get_settings() -> QSettings:
    return QSettings(__org_name__, __app_name__)


# ── Recent files ──────────────────────────────────────────────────────────────

def _coerce_list(val) -> list[str]:
    # QSettings on Linux returns a bare str (not a list) when only one item
    # was stored. Normalise to list[str] unconditionally.
    if val is None:
        return []
    if isinstance(val, str):
        return [val] if val else []
    return list(val)


def add_recent(path: str) -> None:
    s = get_settings()
    files = _coerce_list(s.value("recent_files", []))
    if path in files:
        files.remove(path)
    files.insert(0, path)
    s.setValue("recent_files", files[:_MAX_RECENT])


def get_recent() -> list[str]:
    return _coerce_list(get_settings().value("recent_files", []))


def clear_recent() -> None:
    get_settings().remove("recent_files")


# ── Theme ─────────────────────────────────────────────────────────────────────

def is_dark() -> bool:
    return get_settings().value("dark_mode", False, type=bool)


def set_dark(enabled: bool) -> None:
    get_settings().setValue("dark_mode", enabled)


def apply_theme(app: QApplication, dark: bool) -> None:
    if dark:
        _apply_dark(app)
    else:
        app.setPalette(app.style().standardPalette())
        app.setStyleSheet("")


def _apply_dark(app: QApplication) -> None:
    p = QPalette()
    # Base colours
    p.setColor(QPalette.Window,          QColor("#1e2530"))
    p.setColor(QPalette.WindowText,      QColor("#dde1ec"))
    p.setColor(QPalette.Base,            QColor("#252a36"))
    p.setColor(QPalette.AlternateBase,   QColor("#2b3142"))
    p.setColor(QPalette.ToolTipBase,     QColor("#2b3142"))
    p.setColor(QPalette.ToolTipText,     QColor("#dde1ec"))
    p.setColor(QPalette.Text,            QColor("#dde1ec"))
    p.setColor(QPalette.Button,          QColor("#2b3142"))
    p.setColor(QPalette.ButtonText,      QColor("#dde1ec"))
    p.setColor(QPalette.BrightText,      QColor("#ffffff"))
    p.setColor(QPalette.Link,            QColor("#5b9fe8"))
    p.setColor(QPalette.Highlight,       QColor("#3d5a8a"))
    p.setColor(QPalette.HighlightedText, QColor("#ffffff"))
    # Disabled
    p.setColor(QPalette.Disabled, QPalette.Text,       QColor("#666e80"))
    p.setColor(QPalette.Disabled, QPalette.ButtonText, QColor("#666e80"))
    app.setPalette(p)
    app.setStyleSheet(
        "QToolBar { border-bottom: 1px solid #3a4055; }"
        " QStatusBar { border-top: 1px solid #3a4055; }"
        " QTreeWidget { border: none; }"
        " QTabWidget::pane { border: 1px solid #3a4055; }"
    )
