(. emsdk/emsdk_env.sh &&
 mkdir -p wasm &&
 wget -nc https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.1.tar.xz &&
 rm -rf mpfr-4.2.1 &&
 tar xf mpfr-4.2.1.tar.xz &&
 cd mpfr-4.2.1 &&
 emconfigure ./configure --host none --prefix=${PWD}/../wasm --with-gmp=${PWD}/../wasm &&
 make &&
 make install &&
 cd ..)

# emconfigure ./configure CFLAGS="-sMEMORY64=1" --host none --prefix=${PWD}/../wasm --with-gmp=${PWD}/../wasm &&
