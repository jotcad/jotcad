#include <iostream>
#include "kernel.h"
#include "fix/kiss.h"
#include "fix/assert_mesh.h"
#include "data/surface_mesh_geometry.h"
#include "infra/stl.h"
#include "ops/pour/orientation.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/IO/polygon_mesh_io.h>
#include <CGAL/make_surface_mesh.h>

using namespace jotcad::geo;
typedef CGAL::Surface_mesh<EK::Point_3> ExactMesh;

int main() {
    std::cout << "--- 1. Testing STL Reader & Bear Ingestion ---" << std::endl;
    Geometry bear_geo;
    bool ok_stl = STLReader::read_file("ugc/models/animals/bear.stl", bear_geo);
    std::cout << "STLReader::read_file: " << (ok_stl ? "SUCCESS" : "FAIL")
              << " vertices=" << bear_geo.vertices.size()
              << " triangles=" << bear_geo.triangles.size()
              << std::endl;

    std::cout << "Testing decoupled to_surface_mesh(bear_geo)..." << std::endl;
    ExactMesh m_from_soup = to_surface_mesh(bear_geo);
    std::cout << "to_surface_mesh: vertices=" << m_from_soup.number_of_vertices()
              << " faces=" << m_from_soup.number_of_faces()
              << " is_closed=" << CGAL::is_closed(m_from_soup)
              << " does_self_intersect=" << CGAL::Polygon_mesh_processing::does_self_intersect(m_from_soup)
              << std::endl;

    // --- Boundary Halfedge & Hole Analysis ---
    std::vector<ExactMesh::Halfedge_index> borders;
    for (auto h : m_from_soup.halfedges()) {
        if (m_from_soup.is_border(h)) borders.push_back(h);
    }
    std::cout << "  - Number of border halfedges: " << borders.size() << std::endl;
    std::set<ExactMesh::Halfedge_index> visited_borders;
    int hole_count = 0;
    for (auto h : borders) {
        if (visited_borders.count(h)) continue;
        hole_count++;
        std::cout << "    Hole #" << hole_count << " loop:" << std::endl;
        auto curr = h;
        do {
            visited_borders.insert(curr);
            auto src_p = m_from_soup.point(m_from_soup.source(curr));
            auto tgt_p = m_from_soup.point(m_from_soup.target(curr));
            std::cout << "      Edge: (" << CGAL::to_double(src_p.x()) << ", " << CGAL::to_double(src_p.y()) << ", " << CGAL::to_double(src_p.z()) << ") -> ("
                      << CGAL::to_double(tgt_p.x()) << ", " << CGAL::to_double(tgt_p.y()) << ", " << CGAL::to_double(tgt_p.z()) << ")" << std::endl;
            curr = m_from_soup.next(curr);
        } while (curr != h && visited_borders.size() <= borders.size() + 10);
    }

    // --- Direct Polygon Soup & Edge Manifoldness Trace ---
    std::vector<EK::Point_3> pts;
    std::vector<std::vector<std::size_t>> faces;
    for (const auto& v : bear_geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));
    for (const auto& t : bear_geo.triangles) faces.push_back({(std::size_t)t[0], (std::size_t)t[1], (std::size_t)t[2]});

    std::cout << "\n--- Diagnostic Polygon Soup Analysis ---" << std::endl;
    std::cout << "  - Soup before repair: " << pts.size() << " points, " << faces.size() << " faces." << std::endl;
    auto faces_before = faces;
    auto pts_before = pts;

    // Edge manifoldness audit on original 350 faces
    std::map<std::pair<std::size_t, std::size_t>, std::vector<std::size_t>> edge_to_faces_map;
    for (std::size_t fi = 0; fi < faces_before.size(); ++fi) {
        const auto& f = faces_before[fi];
        for (std::size_t j = 0; j < f.size(); ++j) {
            std::size_t u = f[j];
            std::size_t v = f[(j + 1) % f.size()];
            if (u > v) std::swap(u, v);
            edge_to_faces_map[{u, v}].push_back(fi);
        }
    }
    int open_edges_count = 0;
    int non_manifold_edges_count = 0;
    for (const auto& entry : edge_to_faces_map) {
        if (entry.second.size() == 1) open_edges_count++;
        else if (entry.second.size() > 2) non_manifold_edges_count++;
    }
    std::cout << "  - Edge analysis of original 350 triangles: " << edge_to_faces_map.size() << " unique edges total." << std::endl;
    std::cout << "    * Degree 1 (Open border edges): " << open_edges_count << std::endl;
    std::cout << "    * Degree 2 (Manifold interior edges): " << (edge_to_faces_map.size() - open_edges_count - non_manifold_edges_count) << std::endl;
    std::cout << "    * Degree >2 (Non-manifold edges): " << non_manifold_edges_count << std::endl;

    for (const auto& entry : edge_to_faces_map) {
        if (entry.second.size() > 2) {
            std::cout << "\n  --- Non-Manifold Edge Details (Degree " << entry.second.size() << ") ---" << std::endl;
            std::cout << "  Shared Edge between vertex " << entry.first.first << " and " << entry.first.second << ":" << std::endl;
            auto pA = pts_before[entry.first.first];
            auto pB = pts_before[entry.first.second];
            std::cout << "    Vertex " << entry.first.first << ": (" << CGAL::to_double(pA.x()) << ", " << CGAL::to_double(pA.y()) << ", " << CGAL::to_double(pA.z()) << ")" << std::endl;
            std::cout << "    Vertex " << entry.first.second << ": (" << CGAL::to_double(pB.x()) << ", " << CGAL::to_double(pB.y()) << ", " << CGAL::to_double(pB.z()) << ")" << std::endl;
            std::cout << "  Incident Triangles (" << entry.second.size() << " total):" << std::endl;
            for (auto fi : entry.second) {
                const auto& f = faces_before[fi];
                std::cout << "    * Triangle #" << fi << ": [" << f[0] << ", " << f[1] << ", " << f[2] << "]" << std::endl;
                for (int k = 0; k < 3; ++k) {
                    auto pk = pts_before[f[k]];
                    std::cout << "        v" << k << " (idx " << f[k] << "): ("
                              << CGAL::to_double(pk.x()) << ", " << CGAL::to_double(pk.y()) << ", " << CGAL::to_double(pk.z()) << ")" << std::endl;
                }
                auto v01 = pts_before[f[1]] - pts_before[f[0]];
                auto v02 = pts_before[f[2]] - pts_before[f[0]];
                auto cr = CGAL::cross_product(v01, v02);
                double a = std::sqrt(CGAL::to_double(cr.squared_length())) / 2.0;
                std::cout << "        Area: " << a << std::endl;
            }
        }
    }

    CGAL::Polygon_mesh_processing::repair_polygon_soup(pts, faces);
    std::cout << "  - Soup after repair: " << pts.size() << " points, " << faces.size() << " faces." << std::endl;

    // Identify which triangle was removed by repair_polygon_soup
    if (faces.size() < faces_before.size()) {
        std::map<std::vector<std::size_t>, int> face_counts;
        for (const auto& f : faces) {
            auto sorted_f = f;
            std::sort(sorted_f.begin(), sorted_f.end());
            face_counts[sorted_f]++;
        }
        for (std::size_t i = 0; i < faces_before.size(); ++i) {
            auto sorted_f = faces_before[i];
            std::sort(sorted_f.begin(), sorted_f.end());
            if (face_counts[sorted_f] == 0) {
                std::cout << "  ❌ Triangle #" << i << " was DROPPED by repair_polygon_soup! Vertex indices: ["
                          << faces_before[i][0] << ", " << faces_before[i][1] << ", " << faces_before[i][2] << "]" << std::endl;
                auto p0 = pts_before[faces_before[i][0]];
                auto p1 = pts_before[faces_before[i][1]];
                auto p2 = pts_before[faces_before[i][2]];
                std::cout << "     Coordinates:" << std::endl;
                std::cout << "       v0: (" << CGAL::to_double(p0.x()) << ", " << CGAL::to_double(p0.y()) << ", " << CGAL::to_double(p0.z()) << ")" << std::endl;
                std::cout << "       v1: (" << CGAL::to_double(p1.x()) << ", " << CGAL::to_double(p1.y()) << ", " << CGAL::to_double(p1.z()) << ")" << std::endl;
                std::cout << "       v2: (" << CGAL::to_double(p2.x()) << ", " << CGAL::to_double(p2.y()) << ", " << CGAL::to_double(p2.z()) << ")" << std::endl;

                // Triangle metrics for dropped triangle
                auto v01 = p1 - p0;
                auto v02 = p2 - p0;
                auto v12 = p2 - p1;
                double a = std::sqrt(CGAL::to_double(v12.squared_length()));
                double b = std::sqrt(CGAL::to_double(v02.squared_length()));
                double c = std::sqrt(CGAL::to_double(v01.squared_length()));
                auto cross = CGAL::cross_product(v01, v02);
                double area = std::sqrt(CGAL::to_double(cross.squared_length())) / 2.0;

                // Compute angles in degrees
                double cos_A = (b*b + c*c - a*a) / (2.0 * b * c);
                double cos_B = (a*a + c*c - b*b) / (2.0 * a * c);
                double cos_C = (a*a + b*b - c*c) / (2.0 * a * b);
                cos_A = std::clamp(cos_A, -1.0, 1.0);
                cos_B = std::clamp(cos_B, -1.0, 1.0);
                cos_C = std::clamp(cos_C, -1.0, 1.0);
                double deg_A = std::acos(cos_A) * 180.0 / M_PI;
                double deg_B = std::acos(cos_B) * 180.0 / M_PI;
                double deg_C = std::acos(cos_C) * 180.0 / M_PI;
                double min_deg = std::min({deg_A, deg_B, deg_C});
                double max_deg = std::max({deg_A, deg_B, deg_C});
                double s = (a + b + c) / 2.0;
                double inradius = (area > 1e-14 && s > 1e-14) ? (area / s) : 0.0;
                double circumradius = (area > 1e-14) ? (a * b * c / (4.0 * area)) : 1e9;
                double aspect_ratio = (inradius > 1e-14) ? (circumradius / (2.0 * inradius)) : 1e9;

                std::cout << "     --- Dropped Triangle Geometric Metrics ---" << std::endl;
                std::cout << "       Edge lengths: a=" << a << ", b=" << b << ", c=" << c << std::endl;
                std::cout << "       Area: " << area << std::endl;
                std::cout << "       Angles: A=" << deg_A << "°, B=" << deg_B << "°, C=" << deg_C << "°" << std::endl;
                std::cout << "       Acuteness / Minimum Angle: " << min_deg << "°" << std::endl;
                std::cout << "       Maximum Angle: " << max_deg << "°" << std::endl;
                std::cout << "       Aspect Ratio (R / 2r): " << aspect_ratio << " (ideal equilateral = 1.0)" << std::endl;
            } else {
                face_counts[sorted_f]--;
            }
        }
    }

    // Compute metrics for the 3-sided hole in m_from_soup
    if (borders.size() == 3) {
        auto h0 = borders[0];
        auto h1 = m_from_soup.next(h0);
        auto h2 = m_from_soup.next(h1);
        auto pA = m_from_soup.point(m_from_soup.source(h0));
        auto pB = m_from_soup.point(m_from_soup.source(h1));
        auto pC = m_from_soup.point(m_from_soup.source(h2));

        auto vAB = pB - pA;
        auto vBC = pC - pB;
        auto vCA = pA - pC;
        double l_AB = std::sqrt(CGAL::to_double(vAB.squared_length()));
        double l_BC = std::sqrt(CGAL::to_double(vBC.squared_length()));
        double l_CA = std::sqrt(CGAL::to_double(vCA.squared_length()));
        auto cross_hole = CGAL::cross_product(vAB, pC - pA);
        double hole_area = std::sqrt(CGAL::to_double(cross_hole.squared_length())) / 2.0;

        double cos_A = (l_AB*l_AB + l_CA*l_CA - l_BC*l_BC) / (2.0 * l_AB * l_CA);
        double cos_B = (l_AB*l_AB + l_BC*l_BC - l_CA*l_CA) / (2.0 * l_AB * l_BC);
        double cos_C = (l_BC*l_BC + l_CA*l_CA - l_AB*l_AB) / (2.0 * l_BC * l_CA);
        cos_A = std::clamp(cos_A, -1.0, 1.0);
        cos_B = std::clamp(cos_B, -1.0, 1.0);
        cos_C = std::clamp(cos_C, -1.0, 1.0);
        double deg_A = std::acos(cos_A) * 180.0 / M_PI;
        double deg_B = std::acos(cos_B) * 180.0 / M_PI;
        double deg_C = std::acos(cos_C) * 180.0 / M_PI;
        double min_deg = std::min({deg_A, deg_B, deg_C});
        double max_deg = std::max({deg_A, deg_B, deg_C});
        double s = (l_AB + l_BC + l_CA) / 2.0;
        double inradius = (hole_area > 1e-14 && s > 1e-14) ? (hole_area / s) : 0.0;
        double circumradius = (hole_area > 1e-14) ? (l_AB * l_BC * l_CA / (4.0 * hole_area)) : 1e9;
        double aspect_ratio = (inradius > 1e-14) ? (circumradius / (2.0 * inradius)) : 1e9;

        std::cout << "\n  --- 3-Sided Hole Geometric Metrics ---" << std::endl;
        std::cout << "    Edge lengths: AB=" << l_AB << ", BC=" << l_BC << ", CA=" << l_CA << std::endl;
        std::cout << "    Area: " << hole_area << std::endl;
        std::cout << "    Angles: A=" << deg_A << "°, B=" << deg_B << "°, C=" << deg_C << "°" << std::endl;
        std::cout << "    Acuteness / Minimum Angle: " << min_deg << "°" << std::endl;
        std::cout << "    Maximum Angle: " << max_deg << "°" << std::endl;
        std::cout << "    Aspect Ratio (R / 2r): " << aspect_ratio << " (ideal equilateral = 1.0)" << std::endl;
    }

    ExactMesh m_from_cgal;
    bool ok_cgal_io = CGAL::IO::read_polygon_mesh("ugc/models/animals/bear.stl", m_from_cgal);
    std::cout << "CGAL::IO::read_polygon_mesh: " << (ok_cgal_io ? "SUCCESS" : "FAIL")
              << " vertices=" << m_from_cgal.number_of_vertices()
              << " faces=" << m_from_cgal.number_of_faces()
              << " is_closed=" << CGAL::is_closed(m_from_cgal)
              << " does_self_intersect=" << CGAL::Polygon_mesh_processing::does_self_intersect(m_from_cgal)
              << std::endl;

    std::cout << "\n--- 2. Testing Rotation to Gravity ---" << std::endl;
    ExactMesh m_rot = pour::rotate_mesh_to_gravity(m_from_soup, EK::Vector_3(0, 1, 0));
    std::cout << "m_rot: vertices=" << m_rot.number_of_vertices()
              << " faces=" << m_rot.number_of_faces()
              << " is_closed=" << CGAL::is_closed(m_rot)
              << std::endl;

    std::cout << "\n--- 3. Testing Round-Trip (mesh -> Geometry -> mesh) ---" << std::endl;
    Geometry geo_roundtrip = to_geometry(m_rot);
    std::cout << "geo_roundtrip: vertices=" << geo_roundtrip.vertices.size()
              << " triangles=" << geo_roundtrip.triangles.size()
              << std::endl;
    ExactMesh m_roundtrip = to_surface_mesh(geo_roundtrip);
    std::cout << "m_roundtrip: vertices=" << m_roundtrip.number_of_vertices()
              << " faces=" << m_roundtrip.number_of_faces()
              << " is_closed=" << CGAL::is_closed(m_roundtrip)
              << " does_self_intersect=" << CGAL::Polygon_mesh_processing::does_self_intersect(m_roundtrip)
              << std::endl;

    std::cout << "\n--- 4. Diagnosing Bear Parting Wedge Corefinement ---" << std::endl;
    ExactMesh wedge;
    if (!CGAL::IO::read_polygon_mesh("scratch/self_touch_wedge.off", wedge)) {
        std::cerr << "Failed to load scratch/self_touch_wedge.off" << std::endl;
        return 1;
    }
    std::cout << "Loaded wedge: vertices=" << wedge.number_of_vertices()
              << " faces=" << wedge.number_of_faces()
              << " is_closed=" << CGAL::is_closed(wedge)
              << " does_self_intersect=" << CGAL::Polygon_mesh_processing::does_self_intersect(wedge)
              << std::endl;

    bool rep = fix::separate_kissing_columns(wedge, EK::FT(1) / EK::FT(100));
    std::cout << "separate_kissing_columns returned: " << (rep ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(wedge);

    bool closed = CGAL::is_closed(wedge);
    bool self_inter = CGAL::Polygon_mesh_processing::does_self_intersect(wedge);
    bool oriented = CGAL::Polygon_mesh_processing::is_outward_oriented(wedge);
    std::cout << "Post-repair: is_closed=" << closed
              << " does_self_intersect=" << self_inter
              << " is_outward_oriented=" << oriented
              << std::endl;

    // Create a bounding box / stock mesh enclosing the wedge
    auto bbox = CGAL::Polygon_mesh_processing::bbox(wedge);
    EK::FT xmin(bbox.xmin() - 10), xmax(bbox.xmax() + 10);
    EK::FT ymin(bbox.ymin() - 10), ymax(bbox.ymax() + 10);
    EK::FT zmin(bbox.zmin() - 10), zmax(bbox.zmax() + 10);

    ExactMesh stock;
    CGAL::make_hexahedron(
        EK::Point_3(xmin, ymin, zmin),
        EK::Point_3(xmax, ymin, zmin),
        EK::Point_3(xmax, ymax, zmin),
        EK::Point_3(xmin, ymax, zmin),
        EK::Point_3(xmin, ymin, zmax),
        EK::Point_3(xmax, ymin, zmax),
        EK::Point_3(xmax, ymax, zmax),
        EK::Point_3(xmin, ymax, zmax),
        stock
    );
    CGAL::Polygon_mesh_processing::triangulate_faces(stock);
    std::cout << "Stock box: vertices=" << stock.number_of_vertices()
              << " faces=" << stock.number_of_faces()
              << " is_closed=" << CGAL::is_closed(stock)
              << std::endl;

    std::cout << "Testing corefine_and_compute_intersection..." << std::endl;
    ExactMesh out_inter;
    bool ok_inter = CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(stock, wedge, out_inter);
    std::cout << "Intersection result: " << (ok_inter ? "SUCCESS" : "FAIL")
              << " out vertices=" << out_inter.number_of_vertices()
              << " faces=" << out_inter.number_of_faces()
              << std::endl;

    return 0;
}
