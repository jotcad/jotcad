# JotCAD Operator Tests Makefile

CXX = g++
CXXFLAGS = -O3 -std=c++17
INCLUDES = -I.. -I../impl -I../../fs/cpp/include -I../../fs/cpp/vendor \
           -I../../geometry/native/include -I../../geometry/boost \
           -I../../geometry/native/include/CGAL/include
DEFS = -DCGAL_DISABLE_GMP=1
LIBS = -L../bin -ljotgeo -lpthread

BIN_DIR = bin
LIB_GEO = ../bin/libjotgeo.a

TEST_SOURCES = hexagon_test.cpp box_test.cpp offset_test.cpp outline_test.cpp \
               group_test.cpp rotate_test.cpp pdf_test.cpp
TEST_BINS = $(addprefix $(BIN_DIR)/, $(TEST_SOURCES:.cpp=))

all: $(LIB_GEO) $(TEST_BINS)

# Ensure the library is built first
$(LIB_GEO):
	make -C ..

# Template for building each test
$(BIN_DIR)/%_test: %_test.cpp test_base.h $(LIB_GEO)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) $< $(LIBS) -o $@

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
