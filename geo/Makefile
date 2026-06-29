# JotCAD Geometry Ops Makefile

.DELETE_ON_ERROR:

CXX = g++
CXXFLAGS = -O1 -std=c++17 -fPIC -MMD -MP
AR = ar
ARFLAGS = rcs

# Paths
BIN_DIR = bin
OBJ_DIR = obj

INCLUDES = -I. -I./ops -I./core -I./data -I./math -I./algorithms -I./render -I./infra \
           -I../fs/cpp -I../fs/cpp/vendor \
           -I./cgal -I./cgal/boost \
           -I./cgal/cgal/include -I./cgal/cgal/include/CGAL/include \
           -I./cgal/freetype/include \
           -I./cgal/zenoh/include \
           -I./pack

DEFS = -DCGAL_DISABLE_GMP=1 -DCPPHTTPLIB_OPENSSL_SUPPORT
LIBS = ./cgal/freetype/build/libfreetype.a \
       ./cgal/zenoh/lib/libzenohc.a \
       -lpthread -lssl -lcrypto -lz -ldl

# Library and Binary
LIB_JOTGEO = $(BIN_DIR)/libjotgeo.a
OPS_BIN = $(BIN_DIR)/ops

# Split Binaries
BIN_PRIMITIVES = $(BIN_DIR)/ops_primitives
BIN_BOOLEANS   = $(BIN_DIR)/ops_booleans
BIN_TRANSFORMS = $(BIN_DIR)/ops_transforms
BIN_FEATURES   = $(BIN_DIR)/ops_features
BIN_MAPPING    = $(BIN_DIR)/ops_mapping
BIN_SWEEP      = $(BIN_DIR)/ops_sweep
BIN_UNFOLD     = $(BIN_DIR)/ops_unfold
BIN_TOOLING    = $(BIN_DIR)/ops_tooling
BIN_IO         = $(BIN_DIR)/ops_io

SPLIT_BINS = $(BIN_PRIMITIVES) $(BIN_BOOLEANS) $(BIN_TRANSFORMS) $(BIN_FEATURES) \
             $(BIN_MAPPING) $(BIN_SWEEP) $(BIN_UNFOLD) $(BIN_TOOLING) $(BIN_IO)

all: $(SPLIT_BINS)

# Object files
OBJS = $(OBJ_DIR)/vfs_core.o $(OBJ_DIR)/vfs_primitives.o $(OBJ_DIR)/vfs_geo_adapter.o \
       $(OBJ_DIR)/cid.o $(OBJ_DIR)/registry.o $(OBJ_DIR)/png_op.o \
       $(OBJ_DIR)/rasterizer.o $(OBJ_DIR)/triangulation.o $(OBJ_DIR)/stb_impl.o

$(OBJ_DIR)/stb_impl.o: infra/stb_impl.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@
DRIVER_OBJ = $(OBJ_DIR)/ops.o

SPLIT_DRIVER_OBJS = $(OBJ_DIR)/ops_primitives_driver.o $(OBJ_DIR)/ops_booleans_driver.o \
                    $(OBJ_DIR)/ops_transforms_driver.o $(OBJ_DIR)/ops_features_driver.o \
                    $(OBJ_DIR)/ops_mapping_driver.o $(OBJ_DIR)/ops_sweep_driver.o \
                    $(OBJ_DIR)/ops_unfold_driver.o $(OBJ_DIR)/ops_tooling_driver.o \
                    $(OBJ_DIR)/ops_io_driver.o

SPLIT_REG_OBJS = $(OBJ_DIR)/ops_primitives_reg.o $(OBJ_DIR)/ops_booleans_reg.o \
                 $(OBJ_DIR)/ops_transforms_reg.o $(OBJ_DIR)/ops_features_reg.o \
                 $(OBJ_DIR)/ops_mapping_reg.o $(OBJ_DIR)/ops_sweep_reg.o \
                 $(OBJ_DIR)/ops_unfold_reg.o $(OBJ_DIR)/ops_tooling_reg.o \
                 $(OBJ_DIR)/ops_io_reg.o

# Include all generated dependency files
DEPS = $(OBJS:.o=.d) $(DRIVER_OBJ:.o=.d) $(SPLIT_DRIVER_OBJS:.o=.d) $(SPLIT_REG_OBJS:.o=.d)
-include $(DEPS)

# Create static library

$(LIB_JOTGEO): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(AR) $(ARFLAGS) $@ $^

TEST_REG_LIBS = $(OBJ_DIR)/ops_library.o $(SPLIT_REG_OBJS)

# Link monolithic binary
$(OPS_BIN): $(DRIVER_OBJ) $(TEST_REG_LIBS) $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $(DRIVER_OBJ) $(TEST_REG_LIBS) $(LIB_JOTGEO) $(LIBS) -o $@

# Link split binaries
$(BIN_PRIMITIVES): $(OBJ_DIR)/ops_primitives_driver.o $(OBJ_DIR)/ops_primitives_reg.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(BIN_BOOLEANS): $(OBJ_DIR)/ops_booleans_driver.o $(OBJ_DIR)/ops_booleans_reg.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(BIN_TRANSFORMS): $(OBJ_DIR)/ops_transforms_driver.o $(OBJ_DIR)/ops_transforms_reg.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(BIN_FEATURES): $(OBJ_DIR)/ops_features_driver.o $(OBJ_DIR)/ops_features_reg.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(BIN_MAPPING): $(OBJ_DIR)/ops_mapping_driver.o $(OBJ_DIR)/ops_mapping_reg.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(BIN_SWEEP): $(OBJ_DIR)/ops_sweep_driver.o $(OBJ_DIR)/ops_sweep_reg.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(BIN_UNFOLD): $(OBJ_DIR)/ops_unfold_driver.o $(OBJ_DIR)/ops_unfold_reg.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(BIN_TOOLING): $(OBJ_DIR)/ops_tooling_driver.o $(OBJ_DIR)/ops_tooling_reg.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(BIN_IO): $(OBJ_DIR)/ops_io_driver.o $(OBJ_DIR)/ops_io_reg.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

# Test binaries
TEST_TEXT_BIN = $(BIN_DIR)/text_test
TEST_TEXT_OBJ = $(OBJ_DIR)/text_test.o

$(OBJ_DIR)/text_test.o: test/text_test.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(TEST_TEXT_BIN): $(TEST_TEXT_OBJ) $(TEST_REG_LIBS) $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $< $(TEST_REG_LIBS) -Wl,--start-group $(LIB_JOTGEO) $(LIBS) -Wl,--end-group -o $@

# Generic Test Rule
$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(TEST_REG_LIBS) $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $< $(TEST_REG_LIBS) -Wl,--start-group $(LIB_JOTGEO) $(LIBS) -Wl,--end-group -o $@

$(OBJ_DIR)/test_engine.o: pack/test_engine.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

TEST_ENGINE_BIN = $(BIN_DIR)/test_engine
$(TEST_ENGINE_BIN): $(OBJ_DIR)/test_engine.o $(TEST_REG_LIBS) $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $< $(TEST_REG_LIBS) -Wl,--start-group $(LIB_JOTGEO) $(LIBS) -Wl,--end-group -o $@

TEST_UNIT_BIN = $(BIN_DIR)/unit_tests
TEST_UNIT_OBJ = $(OBJ_DIR)/unit_tests.o

$(OBJ_DIR)/unit_tests.o: test/unit_tests.cpp test/unit_tests_run.h
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(TEST_UNIT_BIN): $(TEST_UNIT_OBJ) $(TEST_REG_LIBS) $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $< $(TEST_REG_LIBS) -Wl,--start-group $(LIB_JOTGEO) $(LIBS) -Wl,--end-group -o $@

test: $(TEST_UNIT_BIN)
	$(TEST_UNIT_BIN)

# Explicit compilation rules
$(OBJ_DIR)/vfs_core.o: ../fs/cpp/vfs_core.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/vfs_primitives.o: ../fs/cpp/vfs_primitives.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/vfs_geo_adapter.o: core/vfs_geo_adapter.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/cid.o: ../fs/cpp/cid.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/registry.o: core/registry.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/png_op.o: render/png_op.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/rasterizer.o: render/rasterizer.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/triangulation.o: render/triangulation.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops_library.o: infra/ops_library.cc $(wildcard ops/*.h)
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

# Compile split drivers
$(OBJ_DIR)/ops_primitives_driver.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -DDEFAULT_NODE_ID=\"geo-primitives-node\" -Dregister_ops=register_primitives_ops -c $< -o $@

$(OBJ_DIR)/ops_booleans_driver.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -DDEFAULT_NODE_ID=\"geo-booleans-node\" -Dregister_ops=register_booleans_ops -c $< -o $@

$(OBJ_DIR)/ops_transforms_driver.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -DDEFAULT_NODE_ID=\"geo-transforms-node\" -Dregister_ops=register_transforms_ops -c $< -o $@

$(OBJ_DIR)/ops_features_driver.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -DDEFAULT_NODE_ID=\"geo-features-node\" -Dregister_ops=register_features_ops -c $< -o $@

$(OBJ_DIR)/ops_mapping_driver.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -DDEFAULT_NODE_ID=\"geo-mapping-node\" -Dregister_ops=register_mapping_ops -c $< -o $@

$(OBJ_DIR)/ops_sweep_driver.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -DDEFAULT_NODE_ID=\"geo-sweep-node\" -Dregister_ops=register_sweep_ops -c $< -o $@

$(OBJ_DIR)/ops_unfold_driver.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -DDEFAULT_NODE_ID=\"geo-unfold-node\" -Dregister_ops=register_unfold_ops -c $< -o $@

$(OBJ_DIR)/ops_tooling_driver.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -DDEFAULT_NODE_ID=\"geo-tooling-node\" -Dregister_ops=register_tooling_ops -c $< -o $@

$(OBJ_DIR)/ops_io_driver.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -DDEFAULT_NODE_ID=\"geo-io-node\" -Dregister_ops=register_io_ops -c $< -o $@

# Compile split registration units
$(OBJ_DIR)/ops_primitives_reg.o: infra/ops_primitives_reg.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops_booleans_reg.o: infra/ops_booleans_reg.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops_transforms_reg.o: infra/ops_transforms_reg.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops_features_reg.o: infra/ops_features_reg.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops_mapping_reg.o: infra/ops_mapping_reg.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops_sweep_reg.o: infra/ops_sweep_reg.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops_unfold_reg.o: infra/ops_unfold_reg.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops_tooling_reg.o: infra/ops_tooling_reg.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops_io_reg.o: infra/ops_io_reg.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

.PHONY: all clean
