# PDFMaestro — Installation Guide

## Requirements

- Python 3.10 or later
- pip 23+
- PySide6 system package **or** installed via pip (see below)

---

## Ubuntu / Debian / Linux Mint

```bash
# Install system Qt6 bindings (recommended — avoids large pip download)
sudo apt-get update
sudo apt-get install -y python3-pip python3-pyside6.qtwidgets python3-pyside6.qtgui \
    python3-pyside6.qtcore

# Install PDFMaestro and its Python dependencies
pip install --user pdfmaestro
```

If `python3-pyside6` is not available in your repos (older Ubuntu), install it via pip instead:

```bash
sudo apt-get install -y python3-pip
pip install --user "pdfmaestro[full]"
```

Launch:

```bash
pdfmaestro
```

---

## Arch Linux / Manjaro

```bash
sudo pacman -S python-pyside6 python-pikepdf python-pillow
pip install --user pdfmaestro
```

---

## Fedora

```bash
sudo dnf install -y python3-pyside6
pip install --user pdfmaestro
```

---

## Any Linux distro — virtual environment (no system changes)

```bash
# Create a venv (system-site-packages lets GTK/Qt system libs through)
python3 -m venv --system-site-packages ~/.venv/pdfmaestro
~/.venv/pdfmaestro/bin/pip install pdfmaestro

# Run
~/.venv/pdfmaestro/bin/pdfmaestro
```

Add a launcher alias in your `~/.bashrc`:

```bash
alias pdfmaestro='~/.venv/pdfmaestro/bin/pdfmaestro'
```

---

## Windows

```powershell
pip install pdfmaestro
pdfmaestro
```

Requires Python 3.10+ from [python.org](https://python.org). PySide6 installs automatically.

---

## macOS

```bash
brew install python@3.12
pip3 install pdfmaestro
pdfmaestro
```

---

## Build from source (for developers)

```bash
git clone https://github.com/RincolTech-Solutions-ltd/PDFMaestro.git
cd PDFMaestro

# Install in editable mode — code changes are instant, no reinstall needed
pip install --user -e ".[dev]"

# Run
python3 -m pdfmaestro
```

Development dependencies (`pytest`, `pytest-qt`, `ruff`) are included with `[dev]`.

### Running tests

```bash
pytest tests/
```

### Linting

```bash
ruff check pdfmaestro/
```

---

## Uninstall

```bash
pip uninstall pdfmaestro
```
