#pragma once
#include "types.h"
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <cmath>

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
    const std::vector<PeakCluster>& peaks,
    const PourParams& params,
    FT sprue_top_z
) {
    std::vector<ToolComponentMesh> result;
    for (const auto& cluster : peaks) {
        FT h = sprue_top_z - cluster.apex.z();
        if (h <= FT(0)) h = FT(10.0);

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
            fix::assert_well_formed_mesh(vent_geom, "vent_geom in generate_sprue_and_vents");
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
