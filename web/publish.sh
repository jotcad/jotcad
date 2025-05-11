cp ./buffer.js jotcad.github.io/
cp ./demo.html jotcad.github.io/
cp ./demo.js jotcad.github.io/
cp dist/jotcad-geometry.js jotcad.github.io/dist/
cp dist/jotcad-op.js jotcad.github.io/dist/
cp dist/jotcad-ops.js jotcad.github.io/dist/
cp dist/jotcad-threejs.js jotcad.github.io/dist/
cp dist/wasm.cjs jotcad.github.io/dist/
cp dist/wasm.wasm jotcad.github.io/dist/

(cd jotcad.github.io &&
 git commit -a -m "publish" &&
 git push)
