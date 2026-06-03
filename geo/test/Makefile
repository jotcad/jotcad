# JotCAD Operator Tests Makefile

CXX = g++
CXXFLAGS = -O1 -std=c++17 -MMD -MP
INCLUDES = -I.. -I../ops -I../core -I../data -I../math -I../algorithms -I../render -I../infra \
           -I../../fs/cpp -I../../fs/cpp/vendor \
           -I../cgal -I../cgal/boost \
           -I../cgal/cgal/include -I../cgal/cgal/include/CGAL/include \
           -I../cgal/freetype/include \
           -I../pack
DEFS = -DCGAL_DISABLE_GMP=1 -DCPPHTTPLIB_OPENSSL_SUPPORT
LIBS = ../cgal/freetype/build/libfreetype.a \
       -lpthread -lssl -lcrypto -lz -ldl

BIN_DIR = bin
LIB_GEO = ../bin/libjotgeo.a

TEST_SOURCES = hexagon_test.cpp box_test.cpp triangle_test.cpp \
               offset_test.cpp outline_test.cpp group_test.cpp rotate_test.cpp \
               pdf_test.cpp corners_test.cpp on_test.cpp cut_test.cpp \
               nested_cut_test.cpp cut_hexagon_test.cpp accumulator_test.cpp \
               offset_closure_test.cpp png_test.cpp corners_visual_test.cpp \
               hexagon_at_cut_test.cpp edges_visual_test.cpp move_test.cpp \
               box_verification.cpp \
               cut_3d_test.cpp join_test.cpp clip_test.cpp fuse_test.cpp \
               disjoint_test.cpp disk_arc_test.cpp arc_variants_test.cpp orb_test.cpp path_test.cpp \
               text_test.cpp \
               cone_test.cpp \
               boolean_plane_test.cpp \
               extrude_interval_test.cpp \
               boolean_visual_test.cpp \
               transform_test.cpp \
               dup_test.cpp \
               gap_test.cpp \
               each_face_test.cpp \
               selection_test.cpp \
               unfold_test.cpp \
               grow_flat_test.cpp \
               cid_consistency_test.cpp \
               pack_test.cpp \
               tag_op_test.cpp \
               pdf_linewidth_test.cpp \
               repro_empty_pdf.cpp \
               cut_surface_solid_test.cpp \
               clip_surface_solid_test.cpp \
               segments_boolean_test.cpp \
               points_boolean_test.cpp \
               lower_dim_boolean_test.cpp \
               ghost_test.cpp \
               stamp_test.cpp \
               simplify_test.cpp
TEST_BINS = $(addprefix $(BIN_DIR)/, $(TEST_SOURCES:.cpp=))
TEST_DEPS = $(TEST_BINS:=.d)

TEST_RUNS = $(addprefix run-, $(TEST_SOURCES:.cpp=))

all: $(LIB_GEO) $(TEST_BINS)

# Ensure the library is built first
.PHONY: $(LIB_GEO)
$(LIB_GEO):
	$(MAKE) -C ..

# Template for building each test
$(BIN_DIR)/%: %.cpp $(LIB_GEO)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) $< -Wl,--start-group $(LIB_GEO) $(LIBS) -Wl,--end-group -o $@

-include $(TEST_DEPS)

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
