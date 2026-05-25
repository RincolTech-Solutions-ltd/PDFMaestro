import sys
import traceback
from PySide6.QtWidgets import QApplication, QMessageBox
from PySide6.QtGui import QIcon
from pdfmaestro.main_window import MainWindow
from pdfmaestro import __app_name__, __org_name__
from pdfmaestro import config as cfg

import os


def _excepthook(exc_type, exc_value, exc_tb):
    """Show unhandled Python exceptions in a dialog instead of dying silently."""
    detail = "".join(traceback.format_exception(exc_type, exc_value, exc_tb))
    box = QMessageBox()
    box.setWindowTitle("Unexpected Error")
    box.setIcon(QMessageBox.Critical)
    box.setText(f"{exc_type.__name__}: {exc_value}")
    box.setDetailedText(detail)
    box.exec()
    sys.__excepthook__(exc_type, exc_value, exc_tb)


def main():
    app = QApplication(sys.argv)
    app.setApplicationName(__app_name__)
    app.setOrganizationName(__org_name__)
    app.setApplicationVersion("0.1.0")

    sys.excepthook = _excepthook

    icon_path = os.path.join(os.path.dirname(__file__), "..", "data", "icons", "pdfmaestro.svg")
    app.setWindowIcon(QIcon(os.path.abspath(icon_path)))

    cfg.apply_theme(app, cfg.is_dark())

    window = MainWindow()
    window.show()
    return app.exec()

if __name__ == "__main__":
    sys.exit(main())
