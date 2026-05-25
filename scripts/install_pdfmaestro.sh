#!/usr/bin/env bash
# install_pdfmaestro.sh — Install or update PDFMaestro (run as your normal user, NOT sudo)
set -e

REPO_URL="https://github.com/RincolTech-Solutions-ltd/PDFMaestro.git"
INSTALL_DIR="$HOME/PDFMaestro"

# Guard: refuse to run as root so $HOME and pip --user point to the right place
if [ "$EUID" -eq 0 ]; then
    echo "ERROR: Do not run this script with sudo."
    echo "Run it as your normal user — sudo is invoked internally where needed:"
    echo "  bash \"$0\""
    exit 1
fi

echo "=== PDFMaestro Installer / Updater ==="
echo "Installing for user: $USER  (home: $HOME)"
echo ""

# 1. System dependencies (only this step needs sudo)
echo "[1/4] Installing system dependencies (sudo required here)..."
sudo apt-get update -qq
sudo apt-get install -y git python3 python3-pip python3-venv libgl1 libglib2.0-0
echo "[✓] System dependencies ready"
echo ""

# 2. Clone or update repo into the user's home
if [ -d "$INSTALL_DIR/.git" ]; then
    echo "[2/4] Updating existing install at $INSTALL_DIR..."
    git -C "$INSTALL_DIR" fetch origin
    git -C "$INSTALL_DIR" checkout main
    git -C "$INSTALL_DIR" pull origin main
    echo "[✓] Updated to: $(git -C "$INSTALL_DIR" log -1 --format='%h %s')"
else
    echo "[2/4] Cloning PDFMaestro to $INSTALL_DIR..."
    git clone "$REPO_URL" "$INSTALL_DIR"
    echo "[✓] Cloned: $(git -C "$INSTALL_DIR" log -1 --format='%h %s')"
fi
echo ""

# 3. Install Python package and dependencies into user's site-packages
echo "[3/4] Installing Python package and dependencies..."
pip3 install --user --break-system-packages -e "$INSTALL_DIR"
echo "[✓] Package installed to $HOME/.local"
echo ""

# 4. Ensure ~/.local/bin is on PATH (append to .bashrc if missing)
LOCAL_BIN="$HOME/.local/bin"
if [[ ":$PATH:" != *":$LOCAL_BIN:"* ]]; then
    echo "export PATH=\"$LOCAL_BIN:\$PATH\"" >> "$HOME/.bashrc"
    export PATH="$LOCAL_BIN:$PATH"
    echo "[✓] Added $LOCAL_BIN to PATH (restart terminal or run: source ~/.bashrc)"
else
    echo "[✓] $LOCAL_BIN already on PATH"
fi
echo ""

# 5. Desktop launcher
echo "[5/5] Creating desktop launcher..."
DESKTOP_DIR="$HOME/.local/share/applications"
mkdir -p "$DESKTOP_DIR"
cat > "$DESKTOP_DIR/pdfmaestro.desktop" <<EOF
[Desktop Entry]
Name=PDFMaestro
Comment=Unified PDF viewer and editor
Exec=$LOCAL_BIN/pdfmaestro %f
Icon=$INSTALL_DIR/data/icons/pdfmaestro.svg
Terminal=false
Type=Application
Categories=Office;Viewer;
MimeType=application/pdf;
EOF
update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
echo "[✓] Desktop launcher ready"
echo ""

echo "=== Done ==="
echo ""
echo "Run PDFMaestro:"
echo "  pdfmaestro"
echo "  pdfmaestro /path/to/file.pdf"
echo ""
echo "To update later, just run this script again."
