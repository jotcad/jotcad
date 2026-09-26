#include "test_base.h"
#include <iostream>
#include <string>
#include <cstring>
#include <exception>
#include <chrono>
#include <iomanip>

// Global backend engine and utility headers included by tests
#include "boolean/engine.h"
#include "render/rasterizer.h"
#include "../math/interval.h"
#include "packaide_engine.h"
#include "../../fs/cpp/cid.h"

// All operator headers included globally to prevent nested namespace parsing
#include "hexagon_op.h"
#include "box_op.h"
#include "disk_op.h"
#include "arc_op.h"
#include "orb_op.h"
#include "cone_op.h"
#include "triangle_op.h"
#include "offset_op.h"
#include "outline_op.h"
#include "points_op.h"
#include "nth_op.h"
#include "rotate_op.h"
#include "color_op.h"
#include "material_op.h"
#include "group_op.h"
#include "path_op.h"
#include "corners_op.h"
#include "on_op.h"
#include "at_op.h"
#include "cut_op.h"
#include "stamp_op.h"
#include "emboss_op.h"
#include "join_op.h"
#include "clip_op.h"
#include "fuse_op.h"
#include "disjoint_op.h"
#include "clean_op.h"
#include "pdf_op.h"
#include "stl_op.h"
#include "png_op.h"
#include "relief_op.h"
#include "rule_op.h"
#include "move_op.h"
#include "fill_op.h"
#include "sew_op.h"
#include "plane_op.h"
#include "extrude_op.h"
#include "faces_op.h"
#include "filter_ops.h"
#include "scale_op.h"
#include "spin_op.h"
#include "simplify_op.h"
#include "approximate_op.h"
#include "hull_op.h"
#include "sweep_op.h"
#include "section_op.h"
#include "place_op.h"
#include "grow_op.h"
#include "wrap_op.h"
#include "smooth_op.h"
#include "separate_op.h"
#include "deform_op.h"
#include "text_op.h"
#include "trace_op.h"
#include "transform_ops.h"
#include "tag_ops.h"
#include "asset_ops.h"
#include "stitch_op.h"
#include "unfold_op.h"
#include "pack_op.h"
#include "item_ops.h"
#include "bb_op.h"
#include "obb_op.h"
#include "measure_ops.h"
#include "sort_ops.h"
#include "decal_pipeline.h"
#include "part_line_op.h"
#include "fix/repair.h"
#include "fix/bridge.h"
#include "fix/kiss.h"
#include "boolean/corefine.h"
#include "mold_op.h"
#include "mold/tide.h"
#include "mold/scoring.h"
#include "mold/candidates.h"
#include "mold/patch.h"
#include "mold/compatibility.h"
#include "mold/beam_search.h"
#include "mold/harmonic.h"

// Global namespace imports to ensure types like Selector and Shape are visible globally to all tests
using namespace fs;
using namespace jotcad::geo;

// Include the generated tests inside namespaces
#include "unit_tests_run.h"

int main(int argc, char* argv[]) {
    // If --list or list is passed, output the space-separated list of test names
    if (argc > 1 && (std::strcmp(argv[1], "--list") == 0 || std::strcmp(argv[1], "list") == 0)) {
        for (int i = 0; ALL_TESTS[i].name != nullptr; ++i) {
            std::cout << ALL_TESTS[i].name << " ";
        }
        std::cout << std::endl;
        return 0;
    }

    std::string target = "all";
    if (argc > 1) {
        target = argv[1];
    }

    if (target == "all" || target == "--all") {
        int passed = 0;
        int failed = 0;
        auto suite_start = std::chrono::steady_clock::now();
        for (int i = 0; ALL_TESTS[i].name != nullptr; ++i) {
            std::cout << "[RUN] (" << (i + 1) << ") " << ALL_TESTS[i].name << "..." << std::endl;
            auto t_start = std::chrono::steady_clock::now();
            try {
                int res = ALL_TESTS[i].func();
                auto t_end = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(t_end - t_start).count();
                if (res == 0) {
                    passed++;
                    std::cout << "✔ " << ALL_TESTS[i].name << " passed (took " << std::fixed << std::setprecision(3) << elapsed << "s)" << std::endl;
                } else {
                    failed++;
                    std::cerr << "✖ " << ALL_TESTS[i].name << " returned " << res << " (took " << std::fixed << std::setprecision(3) << elapsed << "s)" << std::endl;
                }
            } catch (const std::exception& e) {
                auto t_end = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(t_end - t_start).count();
                failed++;
                std::cerr << "✖ " << ALL_TESTS[i].name << " threw exception: " << e.what() << " (took " << std::fixed << std::setprecision(3) << elapsed << "s)" << std::endl;
            } catch (...) {
                auto t_end = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(t_end - t_start).count();
                failed++;
                std::cerr << "✖ " << ALL_TESTS[i].name << " threw unknown exception (took " << std::fixed << std::setprecision(3) << elapsed << "s)" << std::endl;
            }
        }
        auto suite_end = std::chrono::steady_clock::now();
        double total_elapsed = std::chrono::duration<double>(suite_end - suite_start).count();
        std::cout << "\nℹ tests " << (passed + failed) << "\nℹ pass " << passed << "\nℹ fail " << failed << "\nℹ total time " << std::fixed << std::setprecision(3) << total_elapsed << "s" << std::endl;
        return failed > 0 ? 1 : 0;
    } else {
        // Find and run a specific test
        for (int i = 0; ALL_TESTS[i].name != nullptr; ++i) {
            if (target == ALL_TESTS[i].name) {
                auto t_start = std::chrono::steady_clock::now();
                int res = ALL_TESTS[i].func();
                auto t_end = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(t_end - t_start).count();
                std::cout << "\nDuration: " << std::fixed << std::setprecision(3) << elapsed << "s" << std::endl;
                return res;
            }
        }
        std::cerr << "Unknown test: " << target << std::endl;
        return 1;
    }
}
