# export EMCFLAGS="$EMCFLAGS -sMEMORY64=1"

(rm -rf glpk-5.0 &&
 . emsdk/emsdk_env.sh &&
 mkdir -p wasm &&
 wget -nc http://ftp.gnu.org/gnu/glpk/glpk-5.0.tar.gz &&
 tar xzvf glpk-5.0.tar.gz &&
 cd glpk-5.0 &&
 emconfigure ./configure --disable-assembly --host none --enable-cxx --prefix=${PWD}/../wasm &&
 make &&
 make install &&
 cd ..)

# emconfigure ./configure CFLAGS="-sMEMORY64=1" --disable-assembly --host none --enable-cxx --prefix=${PWD}/../wasm &&
