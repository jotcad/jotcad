# JotCAD Geometry Ops Makefile

CXX = g++
CXXFLAGS = -O3 -std=c++17 -fPIC
AR = ar
ARFLAGS = rcs

# Paths
VFS_SRC = ../fs/cpp/src/vfs_node.cpp
OPS_LIB_SRC = ops_library.cc
DRIVER_SRC = ops.cc

BIN_DIR = bin
OBJ_DIR = obj

INCLUDES = -I. -I./impl -I../fs/cpp/include -I../fs/cpp/vendor \
           -I../geometry/native/include -I../geometry/boost \
           -I../geometry/native/include/CGAL/include
DEFS = -DCGAL_DISABLE_GMP=1
LIBS = -lpthread

# Library and Binary
LIB_JOTGEO = $(BIN_DIR)/libjotgeo.a
OPS_BIN = $(BIN_DIR)/ops

# Object files
OBJS = $(OBJ_DIR)/vfs_node.o $(OBJ_DIR)/ops_library.o
DRIVER_OBJ = $(OBJ_DIR)/ops.o

all: $(OPS_BIN) test

# Create static library
$(LIB_JOTGEO): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(AR) $(ARFLAGS) $@ $^

# Build the main ops binary
$(OPS_BIN): $(DRIVER_OBJ) $(LIB_JOTGEO)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

# Run unit tests
test:
	@$(MAKE) -C test test

# Compile vfs_node
$(OBJ_DIR)/vfs_node.o: $(VFS_SRC)
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

# Compile ops_library (and its included headers)
$(OBJ_DIR)/ops_library.o: $(OPS_LIB_SRC) *.h impl/*.h
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

# Compile main driver
$(OBJ_DIR)/ops.o: $(DRIVER_SRC)
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	$(MAKE) -C test clean

.PHONY: all clean test
