# JotCAD Operator Tests Makefile

CXX = g++
CXXFLAGS = -O3 -std=c++17
INCLUDES = -I.. -I../impl -I../../fs/cpp/include -I../../fs/cpp/vendor \
           -I../../geometry/native/include -I../../geometry/boost \
           -I../../geometry/native/include/CGAL/include
DEFS = -DCGAL_DISABLE_GMP=1
LIBS = -L../bin -ljotgeo -lpthread -lcrypto

BIN_DIR = bin
LIB_GEO = ../bin/libjotgeo.a

TEST_SOURCES = cid_consistency_test.cpp hexagon_test.cpp box_test.cpp triangle_test.cpp \
               offset_test.cpp outline_test.cpp group_test.cpp rotate_test.cpp \
               pdf_test.cpp corners_test.cpp on_test.cpp cut_test.cpp \
               nested_cut_test.cpp cut_hexagon_test.cpp accumulator_test.cpp \
               schema_test.cpp offset_closure_test.cpp png_test.cpp
TEST_BINS = $(addprefix $(BIN_DIR)/, $(TEST_SOURCES:_test.cpp=_test))

all: $(LIB_GEO) $(TEST_BINS)

# Ensure the library is built first
$(LIB_GEO):
	$(MAKE) -C ..

# Template for building each test
$(BIN_DIR)/%_test: %_test.cpp $(LIB_GEO)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) $< $(LIBS) -o $@

# Target to build and run an individual test
test-%: $(BIN_DIR)/%_test
	@echo "[Run] $<"
	@./$<

# Run all tests sequentially with fail-fast behavior
unit_test: $(LIB_GEO)
	@for bin in $(TEST_BINS); do \
		echo "--- [$$bin] ---"; \
		$(MAKE) $$bin || exit 1; \
		./$$bin || exit 1; \
	done
	@echo "--------------------------------------"
	@echo "✨ ALL UNIT TESTS PASSED"

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean test-% unit_test
