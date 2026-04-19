# JotCAD Independent Operator Tests Makefile

CXX = g++
CXXFLAGS = -O3 -std=c++17 -fPIC
INCLUDES = -I.. -I../impl -I../../fs/cpp/include -I../../fs/cpp/vendor \
           -I../../geometry/native/include -I../../geometry/boost \
           -I../../geometry/native/include/CGAL/include

# Core dependencies (VFS and shared logic)
CORE_SOURCES = ../../fs/cpp/src/vfs_node.cpp
CORE_OBJS = bin/vfs_node.o

DEFS = -DCGAL_DISABLE_GMP=1
LIBS = -lpthread

BIN_DIR = bin

TEST_SOURCES = hexagon_test.cpp box_test.cpp triangle_test.cpp offset_test.cpp outline_test.cpp \
               group_test.cpp rotate_test.cpp pdf_test.cpp corners_test.cpp on_test.cpp \
               cut_test.cpp nested_cut_test.cpp cut_hexagon_test.cpp accumulator_test.cpp \
               schema_test.cpp offset_closure_test.cpp
TEST_BINS = $(addprefix $(BIN_DIR)/, $(TEST_SOURCES:.cpp=))

all: $(TEST_BINS)

# Compile shared VFS core once
$(CORE_OBJS): $(CORE_SOURCES)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

# Build each test independently, including its own operator headers
$(BIN_DIR)/%_test: %_test.cpp test_base.h $(CORE_OBJS)
	@mkdir -p $(BIN_DIR)
	@echo "[Build] $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) $< $(CORE_OBJS) $(LIBS) -o $@

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
