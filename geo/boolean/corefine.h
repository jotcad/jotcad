#pragma once
#include "kernel.h"
#include "fix/assert_mesh.h"
#include "fix/kiss.h"
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <string>

namespace jotcad {
namespace geo {
namespace boolean {

using Mesh = CGAL::Surface_mesh<EK::Point_3>;
using KissMode = fix::KissMode;

inline bool do_meshes_overlap(const Mesh& m1, const Mesh& m2) {
    if (m1.is_empty() || m2.is_empty()) return false;
    return CGAL::do_overlap(
        CGAL::Polygon_mesh_processing::bbox(m1),
        CGAL::Polygon_mesh_processing::bbox(m2)
    );
}

/**
 * Regularizes raw corefinement output to guarantee unambiguous geometry:
 * 1. Duplicates non-manifold vertices (resolves BGL graph traversal singularities).
 * 2. Detects and resolves 0D point, 1D seam, and 2D face kisses via Minkowski PART or WELD.
 */
inline void regularize_and_resolve_kisses(
    Mesh& mesh,
    KissMode kiss_mode = KissMode::WELD,
    EK::FT width = EK::FT(1) / 100
) {
    if (mesh.is_empty()) return;

    // 1. Restore combinatorial 2-manifold validity
    CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices(mesh);

    // 2. Eliminate zero-volume contact singularities (0D points, 1D seams, 2D surfaces)
    fix::resolve_kissing_seams(mesh, kiss_mode, width);
}

/**
 * corefine_difference:
 * Exact boolean subtraction (target \ tool) with guaranteed unambiguous geometry.
 */
inline bool corefine_difference(
    const Mesh& target,
    const Mesh& tool,
    Mesh& out,
    KissMode kiss_mode = KissMode::WELD,
    EK::FT width = EK::FT(1) / 100,
    const std::string& label = "boolean::corefine_difference"
) {
    if (target.is_empty()) {
        out.clear();
        return true;
    }
    if (tool.is_empty() || !do_meshes_overlap(target, tool)) {
        out = target;
        return true;
    }

    Mesh target_copy = target;
    Mesh tool_copy = tool;
    if (target_copy.has_garbage()) target_copy.collect_garbage();
    if (tool_copy.has_garbage()) tool_copy.collect_garbage();

    fix::assert_well_formed_for_corefinement(target_copy, label + " (target)");
    fix::assert_well_formed_for_corefinement(tool_copy, label + " (tool)");

    std::cout << "    [" << label << "] CGAL corefine difference... " << std::flush;
    auto t0 = std::chrono::steady_clock::now();
    bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
        target_copy, tool_copy, out,
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::all_default()
    );
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "Done in " << std::chrono::duration<double, std::milli>(t1 - t0).count() << "ms." << std::endl << std::flush;
    if (!ok) return false;

    if (out.is_empty() || out.number_of_faces() == 0) {
        out.clear();
        return true;
    }

    std::cout << "    [" << label << "] Checking kisses... " << std::flush;
    auto t2 = std::chrono::steady_clock::now();
    regularize_and_resolve_kisses(out, kiss_mode, width);
    auto t3 = std::chrono::steady_clock::now();
    std::cout << "Done in " << std::chrono::duration<double, std::milli>(t3 - t2).count() << "ms." << std::endl << std::flush;
    fix::assert_well_formed_for_corefinement(out, label + " (out)");
    return true;
}

/**
 * corefine_intersection:
 * Exact boolean intersection (target ∩ tool) with guaranteed unambiguous geometry.
 */
inline bool corefine_intersection(
    const Mesh& target,
    const Mesh& tool,
    Mesh& out,
    KissMode kiss_mode = KissMode::WELD,
    EK::FT width = EK::FT(1) / 100,
    const std::string& label = "boolean::corefine_intersection"
) {
    if (target.is_empty() || tool.is_empty() || !do_meshes_overlap(target, tool)) {
        out.clear();
        return true;
    }

    Mesh target_copy = target;
    Mesh tool_copy = tool;
    if (target_copy.has_garbage()) target_copy.collect_garbage();
    if (tool_copy.has_garbage()) tool_copy.collect_garbage();

    fix::assert_well_formed_for_corefinement(target_copy, label + " (target)");
    fix::assert_well_formed_for_corefinement(tool_copy, label + " (tool)");

    bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(
        target_copy, tool_copy, out,
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::all_default()
    );
    if (!ok) return false;

    if (out.is_empty() || out.number_of_faces() == 0) {
        out.clear();
        return true;
    }

    regularize_and_resolve_kisses(out, kiss_mode, width);
    fix::assert_well_formed_for_corefinement(out, label + " (out)");
    return true;
}

/**
 * corefine_union:
 * Exact boolean union (target ∪ tool) with guaranteed unambiguous geometry.
 */
inline bool corefine_union(
    const Mesh& target,
    const Mesh& tool,
    Mesh& out,
    KissMode kiss_mode = KissMode::WELD,
    EK::FT width = EK::FT(1) / 100,
    const std::string& label = "boolean::corefine_union"
) {
    if (tool.is_empty()) {
        out = target;
        return true;
    }
    if (target.is_empty()) {
        out = tool;
        return true;
    }
    if (!do_meshes_overlap(target, tool)) {
        out = target;
        out.join(tool);
        return true;
    }

    Mesh target_copy = target;
    Mesh tool_copy = tool;
    if (target_copy.has_garbage()) target_copy.collect_garbage();
    if (tool_copy.has_garbage()) tool_copy.collect_garbage();

    fix::assert_well_formed_for_corefinement(target_copy, label + " (target)");
    fix::assert_well_formed_for_corefinement(tool_copy, label + " (tool)");

    std::cout << "    [" << label << "] CGAL corefine union... " << std::flush;
    auto t0 = std::chrono::steady_clock::now();
    bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_union(
        target_copy, tool_copy, out,
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::all_default()
    );
    auto t1 = std::chrono::steady_clock::now();
    std::cout << (ok ? "Done" : "FAILED") << " in " << std::chrono::duration<double, std::milli>(t1 - t0).count() << "ms." << std::endl << std::flush;
    if (!ok) return false;

    if (out.is_empty() || out.number_of_faces() == 0) {
        out.clear();
        return true;
    }

    std::cout << "    [" << label << "] Checking kisses... " << std::flush;
    auto t2 = std::chrono::steady_clock::now();
    regularize_and_resolve_kisses(out, kiss_mode, width);
    auto t3 = std::chrono::steady_clock::now();
    std::cout << "Done in " << std::chrono::duration<double, std::milli>(t3 - t2).count() << "ms." << std::endl << std::flush;
    fix::assert_well_formed_for_corefinement(out, label + " (out)");
    return true;
}

} // namespace boolean
} // namespace geo
} // namespace jotcad
