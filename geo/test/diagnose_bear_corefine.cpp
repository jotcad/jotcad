#include <iostream>
#include "kernel.h"
#include "fix/kiss.h"
#include "fix/assert_mesh.h"
#include "infra/stl.h"
#include "boolean/engine.h"
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

    ExactMesh m_from_reader = boolean::Engine::geometry_to_mesh(bear_geo);
    std::cout << "m_from_reader: vertices=" << m_from_reader.number_of_vertices()
              << " faces=" << m_from_reader.number_of_faces()
              << " is_closed=" << CGAL::is_closed(m_from_reader)
              << " does_self_intersect=" << CGAL::Polygon_mesh_processing::does_self_intersect(m_from_reader)
              << std::endl;

    ExactMesh m_from_cgal;
    bool ok_cgal_io = CGAL::IO::read_polygon_mesh("ugc/models/animals/bear.stl", m_from_cgal);
    std::cout << "CGAL::IO::read_polygon_mesh: " << (ok_cgal_io ? "SUCCESS" : "FAIL")
              << " vertices=" << m_from_cgal.number_of_vertices()
              << " faces=" << m_from_cgal.number_of_faces()
              << " is_closed=" << CGAL::is_closed(m_from_cgal)
              << " does_self_intersect=" << CGAL::Polygon_mesh_processing::does_self_intersect(m_from_cgal)
              << std::endl;

    std::cout << "\n--- 2. Testing Rotation to Gravity ---" << std::endl;
    ExactMesh m_rot = pour::rotate_mesh_to_gravity(m_from_reader, EK::Vector_3(0, 1, 0));
    std::cout << "m_rot: vertices=" << m_rot.number_of_vertices()
              << " faces=" << m_rot.number_of_faces()
              << " is_closed=" << CGAL::is_closed(m_rot)
              << std::endl;

    std::cout << "\n--- 3. Testing Round-Trip (mesh -> Geometry -> mesh) ---" << std::endl;
    Geometry geo_roundtrip = boolean::Engine::mesh_to_geometry(m_rot);
    std::cout << "geo_roundtrip: vertices=" << geo_roundtrip.vertices.size()
              << " triangles=" << geo_roundtrip.triangles.size()
              << std::endl;
    ExactMesh m_roundtrip = boolean::Engine::geometry_to_mesh(geo_roundtrip);
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
