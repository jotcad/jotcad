. emsdk/emsdk_env.sh

ls -la boost
ls -la boost/boost
ls -la boost/boost/config.hpp

em++ -O3 \
     -DBUILDING_NODE_EXTENSION \
     "-DNAPI_EXTERN=__attribute__((__import_module__(\"env\")))" \
     -DGLM_ENABLE_EXPERIMENTAL -DCGAL_USE_GLPK -DCGAL_ALWAYS_ROUND_TO_NEAREST -DCGAL_WITH_GMPXX -DCGAL_USE_GMPXX=1 -DCGAL_DO_NOT_USE_BOOST_MP -DBOOST_ALL_NO_LIB \
     -DIGNORE_NO_ATOMICS=1 -DOCCT_NO_PLUGINS \
     -I../node_modules/emnapi/include/node \
     -I../node_modules/node-addon-api \
     -I. \
     -I./boost \
     -I./glm -I./glm/glm -I./wasm/include \
     -L../node_modules/emnapi/lib/wasm32-emscripten \
     --js-library=../node_modules/emnapi/dist/library_napi.js \
     -sEXPORTED_FUNCTIONS="['_malloc','_free','_napi_register_wasm_v1','_node_api_module_get_api_version_v1']" \
     -sEXPORTED_RUNTIME_METHODS=['emnapiInit'] \
     -sMODULARIZE=1 \
     -sASSERTIONS=1 \
     -o wasm.cjs \
     -fwasm-exceptions \
     -fexceptions \
     wasm.cc \
     ./wasm/lib/libglpk.a ./wasm/lib/libgmpxx.a ./wasm/lib/libmpfr.a ./wasm/lib/libgmp.a \
     -lemnapi

# -DNAPI_DISABLE_CPP_EXCEPTIONS \
