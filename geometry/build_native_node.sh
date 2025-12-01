clang-format --style=google -i *.h *.cc
clang-format --style=google -i rs/*.h rs/*.cc

g++ -O3 \
    -DCGAL_DISABLE_GMP=1 \
    -DBUILDING_NODE_EXTENSION \
    -DIGNORE_NO_ATOMICS=1 \
    -DOCCT_NO_PLUGINS \
    -I./native/include/CGAL/include/ \
    -I../node_modules/emnapi/include/node \
    -I../node_modules/node-addon-api \
    -I. \
    -I./boost \
    -I./native/include \
    -I./rs \
    -fexceptions \
    -fPIC \
    wasm.cc \
    -o native.node \
    -shared

    # Or -bundle for macOS, or appropriate flag for shared library
    # -DBOOST_ALL_NO_LIB \
    # -DGLM_ENABLE_EXPERIMENTAL \
    # -DCGAL_USE_GLPK \
    # -DCGAL_ALWAYS_ROUND_TO_NEAREST \
    # -DCGAL_WITH_GMPXX \
    # -DCGAL_USE_GMPXX=1 \
    # -DCGAL_DO_NOT_USE_BOOST_MP \
    # ./native/lib/libgmpxx.a \
    # ./native/lib/libmpfr.a \
    # ./native/lib/libgmp.a \
    # -I./glm \
    # -I./glm/glm \
