[[ -d boost ]] || (
  git clone --recursive https://github.com/boostorg/boost.git &&
  cd boost &&
  ./bootstrap.sh &&
  ./b2 headers)

