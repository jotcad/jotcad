#pragma once
#include "types.h"
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits_3.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/intersections.h>
#include "fix/assert_mesh.h"
#include <cmath>
#include <variant>

namespace jotcad {
namespace geo {
namespace pour {

inline ExactMesh build_sprue_mesh(
    const EK::Point_3& apex,
    FT radius_bot,
    FT radius_top,
    FT height,
    int hemisphere_rings = 6,
    int segments = 16
) {
    ExactMesh mesh;
    std::vector<std::vector<ExactMesh::Vertex_index>> rings(hemisphere_rings + 1);

    // 1. Bottom pole vertex of the spherical dome (at z = apex.z - radius_bot)
    auto v_bot_pole = mesh.add_vertex(EK::Point_3(
        apex.x(),
        apex.y(),
        apex.z() - radius_bot
    ));

    // 2. Intermediate rings of the lower hemisphere up to equator (z = apex.z)
    for (int r = 1; r <= hemisphere_rings; ++r) {
        double phi = (M_PI / 2.0) * (double(r) / double(hemisphere_rings));
        double sin_phi = std::sin(phi);
        double cos_phi = std::cos(phi);

        FT z_ring = apex.z() - radius_bot * FT(cos_phi);
        FT r_ring = radius_bot * FT(sin_phi);

        for (int s = 0; s < segments; ++s) {
            double theta = 2.0 * M_PI * double(s) / double(segments);
            double cos_theta = std::cos(theta);
            double sin_theta = std::sin(theta);

            EK::Point_3 p(
                apex.x() + r_ring * FT(cos_theta),
                apex.y() + r_ring * FT(sin_theta),
                z_ring
            );
            rings[r].push_back(mesh.add_vertex(p));
        }
    }

    // 3. Top rim ring of the cone (at z = apex.z + height)
    std::vector<ExactMesh::Vertex_index> top_ring;
    for (int s = 0; s < segments; ++s) {
        double theta = 2.0 * M_PI * double(s) / double(segments);
        double cos_theta = std::cos(theta);
        double sin_theta = std::sin(theta);

        EK::Point_3 p(
            apex.x() + radius_top * FT(cos_theta),
            apex.y() + radius_top * FT(sin_theta),
            apex.z() + height
        );
        top_ring.push_back(mesh.add_vertex(p));
    }

    // 4. Top center pole vertex (cap)
    auto v_top_pole = mesh.add_vertex(EK::Point_3(
        apex.x(),
        apex.y(),
        apex.z() + height
    ));

    // 5. Build hemisphere faces
    // Bottom pole triangle fan (outward pointing normal in -Z)
    for (int s = 0; s < segments; ++s) {
        int next_s = (s + 1) % segments;
        mesh.add_face(v_bot_pole, rings[1][next_s], rings[1][s]);
    }

    // Quad strips between hemisphere rings (counterclockwise outward)
    for (int r = 1; r < hemisphere_rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            int next_s = (s + 1) % segments;
            auto v00 = rings[r][s];
            auto v01 = rings[r][next_s];
            auto v10 = rings[r + 1][s];
            auto v11 = rings[r + 1][next_s];

            mesh.add_face(v00, v01, v11);
            mesh.add_face(v00, v11, v10);
        }
    }

    // Quad strip from equator (rings[hemisphere_rings]) to top_ring (conical sidewall)
    for (int s = 0; s < segments; ++s) {
        int next_s = (s + 1) % segments;
        auto v00 = rings[hemisphere_rings][s];
        auto v01 = rings[hemisphere_rings][next_s];
        auto v10 = top_ring[s];
        auto v11 = top_ring[next_s];

        mesh.add_face(v00, v01, v11);
        mesh.add_face(v00, v11, v10);
    }

    // Top cap triangle fan (outward pointing normal in +Z)
    for (int s = 0; s < segments; ++s) {
        int next_s = (s + 1) % segments;
        mesh.add_face(top_ring[s], top_ring[next_s], v_top_pole);
    }

    CGAL::Polygon_mesh_processing::stitch_borders(mesh);
    CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(mesh);
    return mesh;
}

struct ToolComponentMesh {
    ExactMesh mesh;
    bool is_primary = false;
    EK::Point_3 apex;
};

inline std::vector<ToolComponentMesh> generate_sprue_and_vents(
    const ExactMesh& model_mesh,
    const std::vector<PeakCluster>& peaks,
    const PourParams& params,
    FT sprue_top_z
) {
    std::vector<ToolComponentMesh> result;
    if (peaks.empty()) return result;

    typedef CGAL::AABB_face_graph_triangle_primitive<ExactMesh> Primitive;
    typedef CGAL::AABB_traits_3<EK, Primitive> Traits;
    typedef CGAL::AABB_tree<Traits> Tree;
    Tree tree(faces(model_mesh).first, faces(model_mesh).second, model_mesh);

    for (const auto& cluster : peaks) {
        FT h = sprue_top_z - cluster.apex.z();
        if (h <= FT(0)) h = FT(10.0);

        if (!cluster.is_primary) {
            // Check if the vertical riser re-enters the model above apex (delegated venting)
            EK::Ray_3 ray(cluster.apex, EK::Vector_3(0, 0, 1));
            std::vector<typename Tree::Intersection_and_primitive_id<EK::Ray_3>::Type> intersections;
            tree.all_intersections(ray, std::back_inserter(intersections));

            FT min_hit_z = FT(-1);
            FT vent_r = params.vent_dia / FT(2);
            for (const auto& inter : intersections) {
                EK::Point_3 pt;
                if (const EK::Point_3* pi = std::get_if<EK::Point_3>(&inter.first)) {
                    pt = *pi;
                } else if (const EK::Segment_3* ps = std::get_if<EK::Segment_3>(&inter.first)) {
                    pt = ps->source();
                } else {
                    continue;
                }

                if (pt.z() > cluster.apex.z() + FT(0.5)) {
                    if (min_hit_z < FT(0) || pt.z() < min_hit_z) {
                        min_hit_z = pt.z();
                    }
                }
            }

            if (min_hit_z > FT(0)) {
                FT reenter_h = (min_hit_z - cluster.apex.z()) + vent_r;
                if (reenter_h < h) {
                    h = reenter_h;
                    std::cout << "  [Pour Prep] Delegated vent at apex ("
                              << CGAL::to_double(cluster.apex.x()) << ", "
                              << CGAL::to_double(cluster.apex.y()) << ", "
                              << CGAL::to_double(cluster.apex.z()) << ") terminates at z="
                              << CGAL::to_double(cluster.apex.z() + h) << " (re-enters model at z="
                              << CGAL::to_double(min_hit_z) << ")." << std::endl << std::flush;
                }
            }
        }

        ExactMesh vent_geom;
        if (cluster.is_primary) {
            // Primary Pour Funnel / Sprue with spherical vertex dome
            vent_geom = build_sprue_mesh(
                cluster.apex,
                params.sprue_base_dia / FT(2),
                params.sprue_top_dia / FT(2),
                h,
                6,
                16
            );
        } else {
            // Secondary Air Bleed Riser / Vent with spherical vertex dome
            vent_geom = build_sprue_mesh(
                cluster.apex,
                params.vent_dia / FT(2),
                params.vent_dia / FT(2),
                h,
                6,
                12
            );
        }

        if (CGAL::is_closed(vent_geom)) {
            fix::assert_well_formed_closed_mesh(vent_geom, "vent_geom in generate_sprue_and_vents");
            std::cout << "  [Pour Prep] Generated " << (cluster.is_primary ? "primary sprue" : "vent")
                      << " at apex (" << CGAL::to_double(cluster.apex.x()) << ", "
                      << CGAL::to_double(cluster.apex.y()) << ", "
                      << CGAL::to_double(cluster.apex.z()) << ") height=" << CGAL::to_double(h) << "." << std::endl << std::flush;
            result.push_back({std::move(vent_geom), cluster.is_primary, cluster.apex});
        } else {
            std::cerr << "  [Pour Prep Warning] vent_geom is not closed! faces=" << vent_geom.number_of_faces() << std::endl << std::flush;
        }
    }
    return result;
}

} // namespace pour
} // namespace geo
} // namespace jotcad
