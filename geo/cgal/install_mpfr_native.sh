#!/bin/bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PREFIX="$DIR/native"

mkdir -p "$PREFIX"
cd "$DIR"

echo "[MPFR Native] Downloading and extracting MPFR 4.2.1..."
wget -nc https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.1.tar.xz
rm -rf mpfr-4.2.1
tar xf mpfr-4.2.1.tar.xz
cd mpfr-4.2.1

echo "[MPFR Native] Configuring with native target, -fPIC, and GMP at $PREFIX..."
./configure --prefix="$PREFIX" --with-gmp="$PREFIX" CFLAGS="-fPIC -O3 -std=gnu17"

echo "[MPFR Native] Building with $(nproc) cores..."
make -j"$(nproc)"
make install

echo "[MPFR Native] MPFR installed successfully to $PREFIX"
