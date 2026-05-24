#!/bin/bash
set -e
echo "[Geo] Installing System Dependencies (requires sudo)..."
sudo apt-get update
sudo apt-get install -y libboost-all-dev libssl-dev build-essential libxi-dev libglu1-mesa-dev libglew-dev pkg-config xvfb

echo "[Geo] Installing Local Project Dependencies..."
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Run CGAL local install script if cgal folder is not present
if [ ! -d "$SCRIPT_DIR/cgal/cgal" ]; then
    echo "[Geo] Running CGAL local installer..."
    (cd "$SCRIPT_DIR/cgal" && ./install_cgal.sh)
fi

# Run FreeType local install script if freetype library is not present
if [ ! -f "$SCRIPT_DIR/cgal/freetype/build/libfreetype.a" ]; then
    echo "[Geo] Running FreeType local installer..."
    (cd "$SCRIPT_DIR/cgal" && ./install_freetype.sh)
fi

"$SCRIPT_DIR/vendor/install_clipper.sh"
"$SCRIPT_DIR/vendor/install_nlopt.sh"

echo "[Geo] Installation Complete."
