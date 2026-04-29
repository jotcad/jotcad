# JotCAD Operator Tests Makefile

CXX = g++
CXXFLAGS = -O3 -std=c++17
INCLUDES = -I.. -I../ops -I../core -I../data -I../math -I../algorithms -I../render -I../infra \
           -I../../fs/cpp -I../../fs/cpp/vendor \
           -I../cgal -I../cgal/boost \
           -I../cgal/cgal/include -I../cgal/cgal/include/CGAL/include
DEFS = -DCGAL_DISABLE_GMP=1
LIBS = -L../bin -ljotgeo -lpthread -lcrypto

BIN_DIR = bin
LIB_GEO = ../bin/libjotgeo.a

TEST_SOURCES = hexagon_test.cpp box_test.cpp triangle_test.cpp \
               offset_test.cpp outline_test.cpp group_test.cpp rotate_test.cpp \
               pdf_test.cpp corners_test.cpp on_test.cpp cut_test.cpp \
               nested_cut_test.cpp cut_hexagon_test.cpp accumulator_test.cpp \
               offset_closure_test.cpp png_test.cpp corners_visual_test.cpp \
               hexagon_at_cut_test.cpp edges_visual_test.cpp move_test.cpp \
               cut_3d_test.cpp join_test.cpp clip_test.cpp fuse_test.cpp
TEST_BINS = $(addprefix $(BIN_DIR)/, $(TEST_SOURCES:.cpp=))

TEST_RUNS = $(addprefix run-, $(TEST_SOURCES:.cpp=))

all: $(LIB_GEO) $(TEST_BINS)

# Ensure the library is built first
$(LIB_GEO):
	$(MAKE) -C ..

# Template for building each test
$(BIN_DIR)/%: %.cpp $(LIB_GEO)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) $< ../obj/rasterizer.o $(LIBS) -o $@

# Run an individual test
run-%: $(BIN_DIR)/%
	@./$<

# Parallel-friendly test target
unit_test: $(TEST_RUNS)
	@echo "--------------------------------------"
	@echo "✨ ALL UNIT TESTS PASSED"

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean test-% unit_test
