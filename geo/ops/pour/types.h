#pragma once
#include "protocols.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <vector>
#include <string>

namespace jotcad {
namespace geo {
namespace pour {

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Surface_mesh<EK::Point_3> ExactMesh;
typedef EK::FT FT;

struct PourParams {
    FT sprue_base_dia = FT(16.0);    // Base diameter of main pour funnel in mm
    FT sprue_top_dia = FT(36.0);     // Top opening diameter of main pour funnel in mm
    FT sprue_height = FT(20.0);      // Extra height above model top
    FT vent_dia = FT(2.5);           // Secondary air bleed riser diameter in mm
    bool auto_orient = true;         // Whether to optimize orientation for minimal bubble trapping
};

struct PeakCluster {
    int id = 0;
    EK::Point_3 apex;
    FT max_z = FT(-1e9);
    std::vector<ExactMesh::Vertex_index> vertices;
    FT catchment_area = FT(0);
    bool is_primary = false;
};

struct PourResult {
    ExactMesh oriented_mesh;
    EK::Vector_3 up_vector;
    std::vector<PeakCluster> peaks;
    ExactMesh prepped_mesh_with_vents;
};

} // namespace pour
} // namespace geo
} // namespace jotcad
