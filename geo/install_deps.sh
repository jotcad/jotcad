#!/bin/bash
set -e
echo "[Geo] Installing System Dependencies (requires sudo)..."
sudo apt-get update
sudo apt-get install -y libboost-all-dev libssl-dev build-essential libxi-dev libglu1-mesa-dev libglew-dev pkg-config xvfb

echo "[Geo] Installing Local Project Dependencies..."
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Run local installer scripts for native C++ dependencies
echo "[Geo] Running CGAL local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_cgal.sh)

echo "[Geo] Running Eigen local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_eigen.sh)

echo "[Geo] Running GLM local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_glm.sh)

echo "[Geo] Running FreeType local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_freetype.sh)

"$SCRIPT_DIR/vendor/install_clipper.sh"
"$SCRIPT_DIR/vendor/install_nlopt.sh"

echo "[Geo] Installation Complete."
