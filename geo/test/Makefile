# JotCAD Operator Tests Makefile

CXX = g++
CXXFLAGS = -O1 -std=c++17 -MMD -MP -fPIC
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
OBJ_DIR = obj
LIB_GEO = ../bin/libjotgeo.a

# List of all test source files
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
               material_test.cpp \
               simplify_test.cpp \
               relief_test.cpp \
               emboss_test.cpp \
               trace_quant_test.cpp

# Filter out cid_consistency_test.cpp from combined build
COMBINED_TEST_SOURCES = $(filter-out cid_consistency_test.cpp, $(TEST_SOURCES))

# Targets
all: $(LIB_GEO) $(BIN_DIR)/unit_tests $(BIN_DIR)/cid_consistency_test

# Ensure the library is built first
.PHONY: $(LIB_GEO)
$(LIB_GEO):
	$(MAKE) -C ..

# Rule for building the combined unit_tests binary from a single translation unit
$(BIN_DIR)/unit_tests: $(OBJ_DIR)/unit_tests.o $(LIB_GEO)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -Wl,--start-group $(LIB_GEO) $(LIBS) -Wl,--end-group -o $@

# Rule for building the standalone cid_consistency_test binary
$(BIN_DIR)/cid_consistency_test: $(OBJ_DIR)/cid_consistency_test.o $(LIB_GEO)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -Wl,--start-group $(LIB_GEO) $(LIBS) -Wl,--end-group -o $@

# Compile unit_tests.o, generating unit_tests_run.h dynamically first
$(OBJ_DIR)/unit_tests.o: unit_tests.cpp unit_tests_run.h
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

# Compile cid_consistency_test.o
$(OBJ_DIR)/cid_consistency_test.o: cid_consistency_test.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFS) -c $< -o $@

# Dynamic header generation listing all test functions inside namespaces
unit_tests_run.h: Makefile
	@echo "// Automatically generated. Do not edit." > $@
	@for t in $(COMBINED_TEST_SOURCES:.cpp=); do \
		echo "namespace test_$$t {" >> $@; \
		echo "  #define main run" >> $@; \
		echo "  #include \"$$t.cpp\"" >> $@; \
		echo "  #undef main" >> $@; \
		echo "}" >> $@; \
	done
	@echo "struct TestEntry { const char* name; int (*func)(); };" >> $@
	@echo "static TestEntry ALL_TESTS[] = {" >> $@
	@for t in $(COMBINED_TEST_SOURCES:.cpp=); do \
		echo "    { \"$$t\", test_$$t::run }," >> $@; \
	done
	@echo "    { nullptr, nullptr }" >> $@
	@echo "};" >> $@

# Include generated dependencies
-include $(OBJ_DIR)/unit_tests.d $(OBJ_DIR)/cid_consistency_test.d

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR) unit_tests_run.h

.PHONY: all clean
