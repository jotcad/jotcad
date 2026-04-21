# JotCAD Geometry Ops Makefile

.DELETE_ON_ERROR:

CXX = g++
CXXFLAGS = -O3 -std=c++17 -fPIC -MMD -MP
AR = ar
ARFLAGS = rcs

# Paths
CID_SRC = ../fs/cpp/src/cid.cpp
OPS_LIB_SRC = ops_library.cc
REGISTRY_SRC = impl/registry.cc
DRIVER_SRC = ops.cc

BIN_DIR = bin
OBJ_DIR = obj

INCLUDES = -I. -I./impl -I../fs/cpp/include -I../fs/cpp/vendor \
           -I../geometry/native/include -I../geometry/boost \
           -I../geometry/native/include/CGAL/include
DEFS = -DCGAL_DISABLE_GMP=1
LIBS = -lpthread -lcrypto

# Library and Binary
LIB_JOTGEO = $(BIN_DIR)/libjotgeo.a
OPS_BIN = $(BIN_DIR)/ops

# Object files
OBJS = $(OBJ_DIR)/vfs_core.o $(OBJ_DIR)/vfs_primitives.o $(OBJ_DIR)/vfs_geo_adapter.o \
       $(OBJ_DIR)/cid.o $(OBJ_DIR)/ops_library.o $(OBJ_DIR)/registry.o $(OBJ_DIR)/png_op.o
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

# Compile source files
$(OBJ_DIR)/%.o: %.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/vfs_core.o: ../fs/cpp/src/vfs_core.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/vfs_primitives.o: ../fs/cpp/src/vfs_primitives.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/vfs_geo_adapter.o: impl/vfs_geo_adapter.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/cid.o: ../fs/cpp/src/cid.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/registry.o: impl/registry.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/png_op.o: impl/png_op.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

-include $(DEPS)

.PHONY: all clean
