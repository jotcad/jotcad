# JotCAD Geometry Ops Makefile

CXX = g++
CXXFLAGS = -O3 -std=c++17 -fPIC -MMD -MP
AR = ar
ARFLAGS = rcs

# Paths
VFS_SRC = ../fs/cpp/src/vfs_node.cpp
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
OBJS = $(OBJ_DIR)/vfs_node.o $(OBJ_DIR)/cid.o $(OBJ_DIR)/ops_library.o $(OBJ_DIR)/registry.o
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

$(OBJ_DIR)/%.o: ../fs/cpp/src/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

$(OBJ_DIR)/registry.o: impl/registry.cc
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

-include $(DEPS)

.PHONY: all clean
