# JotCAD Geometry Ops Makefile

.DELETE_ON_ERROR:

CXX = g++
CXXFLAGS = -O3 -std=c++17 -fPIC -MMD -MP
AR = ar
ARFLAGS = rcs

# Paths
BIN_DIR = bin
OBJ_DIR = obj

INCLUDES = -I. -I./ops -I./core -I./data -I./math -I./algorithms -I./render -I./infra \
           -I../fs/cpp -I../fs/cpp/vendor \
           -I./cgal -I./cgal/boost \
           -I./cgal/cgal/include -I./cgal/cgal/include/CGAL/include
DEFS = -DCGAL_DISABLE_GMP=1
LIBS = -lpthread -lcrypto

# Library and Binary
LIB_JOTGEO = $(BIN_DIR)/libjotgeo.a
OPS_BIN = $(BIN_DIR)/ops

# Object files
OBJS = $(OBJ_DIR)/vfs_core.o $(OBJ_DIR)/vfs_primitives.o $(OBJ_DIR)/vfs_geo_adapter.o \
       $(OBJ_DIR)/cid.o $(OBJ_DIR)/ops_library.o $(OBJ_DIR)/registry.o $(OBJ_DIR)/png_op.o \
       $(OBJ_DIR)/rasterizer.o
DRIVER_OBJ = $(OBJ_DIR)/ops.o

# Include all generated dependency files
DEPS = $(OBJS:.o=.d) $(DRIVER_OBJ:.o=.d)

all: $(OPS_BIN)

# Create static library
$(LIB_JOTGEO): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(AR) $(ARFLAGS) $@ $^

# Link binary
$(OPS_BIN): $(DRIVER_OBJ) $(LIB_JOTGEO)
	$(CXX) $(CXXFLAGS) $(DRIVER_OBJ) $(LIB_JOTGEO) $(LIBS) -o $@

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

$(OBJ_DIR)/ops_library.o: infra/ops_library.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/ops.o: infra/ops.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

-include $(DEPS)

.PHONY: all clean
