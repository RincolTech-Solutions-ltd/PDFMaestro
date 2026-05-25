import sys
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QIcon
from pdfmaestro.main_window import MainWindow
from pdfmaestro import __app_name__, __org_name__

import os

def main():
    app = QApplication(sys.argv)
    app.setApplicationName(__app_name__)
    app.setOrganizationName(__org_name__)
    app.setApplicationVersion("0.1.0")

    icon_path = os.path.join(os.path.dirname(__file__), "..", "data", "icons", "pdfmaestro.svg")
    app.setWindowIcon(QIcon(os.path.abspath(icon_path)))

    window = MainWindow()
    window.show()
    return app.exec()

if __name__ == "__main__":
    sys.exit(main())
