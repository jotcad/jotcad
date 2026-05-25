[[ -f freetype/build/libfreetype.a ]] || (
  rm -rf freetype &&
  git clone https://github.com/freetype/freetype &&
  cd freetype &&
  cmake -B build . &&
  cmake --build build -- all
)
