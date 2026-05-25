#!/usr/bin/env bash
# install_pdfmaestro.sh — Install PDFMaestro on Linux (Ubuntu/Debian-based)
set -e

REPO_URL="https://github.com/RincolTech-Solutions-ltd/PDFMaestro.git"
INSTALL_DIR="$HOME/PDFMaestro"

echo "=== PDFMaestro Installer ==="
echo ""

# 1. System dependencies
echo "[1/4] Installing system dependencies..."
sudo apt-get update -qq
sudo apt-get install -y git python3 python3-pip python3-venv libgl1

echo "[✓] System dependencies installed"
echo ""

# 2. Clone or update repo
if [ -d "$INSTALL_DIR/.git" ]; then
    echo "[2/4] Updating existing clone at $INSTALL_DIR..."
    git -C "$INSTALL_DIR" pull origin main
else
    echo "[2/4] Cloning PDFMaestro to $INSTALL_DIR..."
    git clone "$REPO_URL" "$INSTALL_DIR"
fi
echo "[✓] Repository ready"
echo ""

# 3. Install Python package
echo "[3/4] Installing PDFMaestro and Python dependencies..."
pip3 install --user --break-system-packages -e "$INSTALL_DIR"
echo "[✓] Package installed"
echo ""

# 4. Desktop launcher
echo "[4/4] Creating desktop launcher..."
DESKTOP_DIR="$HOME/.local/share/applications"
mkdir -p "$DESKTOP_DIR"
cat > "$DESKTOP_DIR/pdfmaestro.desktop" <<EOF
[Desktop Entry]
Name=PDFMaestro
Comment=Unified PDF viewer and editor
Exec=$HOME/.local/bin/pdfmaestro %f
Icon=$INSTALL_DIR/data/icons/pdfmaestro.svg
Terminal=false
Type=Application
Categories=Office;Viewer;
MimeType=application/pdf;
EOF
update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
echo "[✓] Desktop launcher created"
echo ""

echo "=== Installation complete ==="
echo ""
echo "Run PDFMaestro with:"
echo "  pdfmaestro"
echo ""
echo "Or open any PDF with:"
echo "  pdfmaestro /path/to/file.pdf"
echo ""
echo "GitHub: $REPO_URL"
