#!/bin/bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PREFIX="$DIR/native"

mkdir -p "$PREFIX"
cd "$DIR"

echo "[GMP Native] Downloading and extracting GMP 6.2.1..."
wget -nc https://ftp.gnu.org/gnu/gmp/gmp-6.2.1.tar.xz
rm -rf gmp-6.2.1
tar xf gmp-6.2.1.tar.xz
cd gmp-6.2.1

echo "[GMP Native] Configuring with native assembly, C++ bindings, and -fPIC..."
./configure --enable-cxx --prefix="$PREFIX" CFLAGS="-fPIC -O3 -std=gnu17" CXXFLAGS="-fPIC -O3 -std=gnu++17"

echo "[GMP Native] Building with $(nproc) cores..."
make -j"$(nproc)"
make install

echo "[GMP Native] GMP installed successfully to $PREFIX"
