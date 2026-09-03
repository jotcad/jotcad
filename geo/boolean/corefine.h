#pragma once
#include "kernel.h"
#include "fix/assert_mesh.h"
#include "fix/kiss.h"
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <string>

namespace jotcad {
namespace geo {
namespace boolean {

using Mesh = CGAL::Surface_mesh<EK::Point_3>;
using KissMode = fix::KissMode;

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
    fix::assert_well_formed_for_corefinement(target, label + " (target)");
    fix::assert_well_formed_for_corefinement(tool, label + " (tool)");

    Mesh target_copy = target;
    Mesh tool_copy = tool;

    bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
        target_copy, tool_copy, out,
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::all_default()
    );
    if (!ok) return false;

    regularize_and_resolve_kisses(out, kiss_mode, width);
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
    fix::assert_well_formed_for_corefinement(target, label + " (target)");
    fix::assert_well_formed_for_corefinement(tool, label + " (tool)");

    Mesh target_copy = target;
    Mesh tool_copy = tool;

    bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(
        target_copy, tool_copy, out,
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::all_default()
    );
    if (!ok) return false;

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
    fix::assert_well_formed_for_corefinement(target, label + " (target)");
    fix::assert_well_formed_for_corefinement(tool, label + " (tool)");

    Mesh target_copy = target;
    Mesh tool_copy = tool;

    bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_union(
        target_copy, tool_copy, out,
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::throw_on_self_intersection(false),
        CGAL::parameters::all_default()
    );
    if (!ok) return false;

    regularize_and_resolve_kisses(out, kiss_mode, width);
    fix::assert_well_formed_for_corefinement(out, label + " (out)");
    return true;
}

} // namespace boolean
} // namespace geo
} // namespace jotcad
