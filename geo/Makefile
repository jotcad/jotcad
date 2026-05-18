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
           -I./pack \
           -I./vendor/clipper \
           -I./vendor/nlopt/src/api \
           -I./vendor/nlopt/build

DEFS = -DCGAL_DISABLE_GMP=1 -DCPPHTTPLIB_OPENSSL_SUPPORT
LIBS = ./cgal/freetype/build/libfreetype.a \
       ./vendor/clipper/libclipper.a \
       ./vendor/nlopt/build/libnlopt.a \
       -lpthread -lssl -lcrypto -lz -ldl

# Library and Binary
LIB_JOTGEO = $(BIN_DIR)/libjotgeo.a
OPS_BIN = $(BIN_DIR)/ops

all: $(OPS_BIN)

# Object files
OBJS = $(OBJ_DIR)/vfs_core.o $(OBJ_DIR)/vfs_primitives.o $(OBJ_DIR)/vfs_geo_adapter.o \
       $(OBJ_DIR)/cid.o $(OBJ_DIR)/ops_library.o $(OBJ_DIR)/registry.o $(OBJ_DIR)/png_op.o \
       $(OBJ_DIR)/rasterizer.o $(OBJ_DIR)/triangulation.o $(OBJ_DIR)/stb_impl.o

$(OBJ_DIR)/stb_impl.o: infra/stb_impl.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@
DRIVER_OBJ = $(OBJ_DIR)/ops.o

# Include all generated dependency files
DEPS = $(OBJS:.o=.d) $(DRIVER_OBJ:.o=.d)
-include $(DEPS)

# Create static library

$(LIB_JOTGEO): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(AR) $(ARFLAGS) $@ $^

# Link binary
$(OPS_BIN): $(DRIVER_OBJ) $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $(DRIVER_OBJ) $(LIB_JOTGEO) $(LIBS) -o $@

# Test binaries
TEST_LOGIC_BIN = $(BIN_DIR)/logic_verification
TEST_LOGIC_OBJ = $(OBJ_DIR)/logic_verification.o
TEST_TEXT_BIN = $(BIN_DIR)/text_test
TEST_TEXT_OBJ = $(OBJ_DIR)/text_test.o

$(OBJ_DIR)/logic_verification.o: test/logic_verification.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(TEST_LOGIC_BIN): $(TEST_LOGIC_OBJ) $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $< -Wl,--start-group $(LIB_JOTGEO) $(LIBS) -Wl,--end-group -o $@

$(OBJ_DIR)/text_test.o: test/text_test.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(TEST_TEXT_BIN): $(TEST_TEXT_OBJ) $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $< -Wl,--start-group $(LIB_JOTGEO) $(LIBS) -Wl,--end-group -o $@

# Generic Test Rule
$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $< -Wl,--start-group $(LIB_JOTGEO) $(LIBS) -Wl,--end-group -o $@

$(OBJ_DIR)/test_engine.o: pack/test_engine.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

TEST_ENGINE_BIN = $(BIN_DIR)/test_engine
$(TEST_ENGINE_BIN): $(OBJ_DIR)/test_engine.o $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $< -Wl,--start-group $(LIB_JOTGEO) $(LIBS) -Wl,--end-group -o $@

test: $(OPS_BIN) $(TEST_LOGIC_BIN) $(TEST_ENGINE_BIN)
	$(TEST_LOGIC_BIN)
	$(TEST_ENGINE_BIN)

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

$(OBJ_DIR)/ops_library.o: infra/ops_library.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

.PHONY: all clean
