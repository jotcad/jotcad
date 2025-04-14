[[ -d boost ]] || (
  git clone --recursive https://github.com/boostorg/boost.git &&
  cd boost &&
  ./bootstrap.sh &&
  emconfigure ./b2 toolset=gcc --prefix=../em_boost --build-dir=build &&
  emconfigure ./b2 toolset=gcc --prefix=../em_boost --build-dir=build install)

