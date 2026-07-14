#!/bin/bash
set -e

echo "[Geo] Installing Local Project Dependencies..."

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Run local installer scripts for native C++ dependencies
echo "[Geo] Running Boost local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_boost.sh)

echo "[Geo] Running CGAL local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_cgal.sh)

echo "[Geo] Running Eigen local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_eigen.sh)

echo "[Geo] Running GLM local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_glm.sh)

echo "[Geo] Running FreeType local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_freetype.sh)

echo "[Geo] Running Zenoh local installer..."
(cd "$SCRIPT_DIR/cgal" && ./install_zenoh.sh)

"$SCRIPT_DIR/vendor/install_clipper.sh"

echo "[Geo] Building C++ kernels..."
"$SCRIPT_DIR/compile.sh"

echo "[Geo] Building fs/cpp test server..."
make -C "$SCRIPT_DIR/../fs/cpp" all

echo "[Geo] Installation Complete."

