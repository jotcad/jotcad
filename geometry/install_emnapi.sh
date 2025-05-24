export EMCC_WASM_TARGET=wasm64

(git clone https://github.com/toyobayashi/emnapi.git &&
 cd ./emnapi &&
 npm install -g node-gyp &&
 npm install &&
 npm run build &&
 node ./script/release.js)
