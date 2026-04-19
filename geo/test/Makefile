# JotCAD Independent Operator Tests Makefile

CXX = g++
CXXFLAGS = -O3 -std=c++17 -fPIC
INCLUDES = -I.. -I../impl -I../../fs/cpp/include -I../../fs/cpp/vendor \
           -I../../geometry/native/include -I../../geometry/boost \
           -I../../geometry/native/include/CGAL/include

# Core dependencies (VFS, shared logic, and operator catalog)
CORE_SOURCES = ../../fs/cpp/src/vfs_node.cpp ../ops_library.cc ../impl/registry.cc
CORE_OBJS = bin/vfs_node.o bin/ops_library.o bin/registry.o

DEFS = -DCGAL_DISABLE_GMP=1
LIBS = -lpthread -lcrypto

BIN_DIR = bin

TEST_SOURCES = hexagon_test.cpp box_test.cpp triangle_test.cpp offset_test.cpp outline_test.cpp \
               group_test.cpp rotate_test.cpp pdf_test.cpp corners_test.cpp on_test.cpp \
               cut_test.cpp nested_cut_test.cpp cut_hexagon_test.cpp accumulator_test.cpp \
               schema_test.cpp offset_closure_test.cpp
TEST_BINS = $(addprefix $(BIN_DIR)/, $(TEST_SOURCES:.cpp=))

TEST_RUNS = $(addprefix test-, $(TEST_SOURCES:_test.cpp=))

all: $(TEST_BINS)

# Compile shared VFS core once
bin/vfs_node.o: ../../fs/cpp/src/vfs_node.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

# Compile shared Op Registry once
bin/ops_library.o: ../ops_library.cc
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

# Compile implementation registry
bin/registry.o: ../impl/registry.cc
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

HEADERS = $(wildcard ../*.h) $(wildcard ../impl/*.h)

# Build each test independently
$(BIN_DIR)/%_test: %_test.cpp test_base.h $(CORE_OBJS) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	@echo "[Build] $@"
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) $< $(CORE_OBJS) $(LIBS) -o $@

# Run all tests
test: $(TEST_RUNS)
	@echo "--------------------------------------"
	@echo "✨ ALL UNIT TESTS PASSED"

# Run a specific test (e.g. make test-hexagon)
test-%: $(BIN_DIR)/%_test
	@echo "[Run] $<"
	@./$<

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean test
