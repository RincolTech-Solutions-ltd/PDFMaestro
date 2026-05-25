from PySide6.QtCore import QSettings
from pdfmaestro import __app_name__, __org_name__


def get_settings() -> QSettings:
    return QSettings(__org_name__, __app_name__)
