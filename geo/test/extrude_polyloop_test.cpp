#include "test_base.h"
#include "ops/extrude_op.h"
#include "ops/extrude/polyline.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <iostream>
#include <cassert>

using namespace jotcad::geo;
using namespace jotcad::geo::boolean;

// Extracted draw direction from mold piece where sprue attaches:
static const double DRAW_DIR[3] = {-8.222171494628272e-09, 0.9541447881436418, -0.29934549146149686};

// Extracted 3D polyloop vertices (48 vertices forming a closed loop):
static const double POLYLOOP_PTS_3D[48][3] = {
    {0.0, 0.0, -8.660250000000001},
    {7.071070000000001, -4.08248, -2.88675},
    {7.071070000000001, 4.08248, 2.88675},
    {2.08129, 1.2016300000000002, 6.960889999999999},
    {2.0858, 0.863966, 6.7166},
    {2.0215699999999996, 0.50146, 6.538930000000001},
    {1.88199, 0.0, 6.35529},
    {1.76044, -0.281652, 6.305009999999999},
    {1.5965799999999999, -0.6613249999999999, 6.237220000000001},
    {1.9598399999999998, -0.8117939999999999, 6.538930000000001},
    {2.40031, -0.9942409999999999, 7.16025},
    {2.5980800000000004, 0.0, 7.16025},
    {2.89778, 0.0, 7.883800000000001},
    {3.0, 0.0, 8.660250000000001},
    {4.5000024999958335, 0.0, 23.6603},
    {4.271642119396468, 1.1480519134134775, 23.6603},
    {4.15746230969615, 1.722075956706739, 23.6603},
    {3.5071412258312904, 2.6953475788207015, 23.6603},
    {3.1819817677637205, 3.1819817677637205, 23.6603},
    {2.208710145649757, 3.8323028516285804, 23.6603},
    {1.722075956706739, 4.15746230969615, 23.6603},
    {0.5740240432932613, 4.385822690295516, 23.6603},
    {2.7554565308057816e-16, 4.5000024999958335, 23.6603},
    {-1.1480519134134775, 4.271642119396468, 23.6603},
    {-1.722075956706739, 4.15746230969615, 23.6603},
    {-2.6953475788207015, 3.5071412258312904, 23.6603},
    {-3.0901711013401254, 3.2433278614361774, 23.6603},
    {-3.1819817677637205, 3.1819817677637205, 23.6603},
    {-3.8323028516285804, 2.208710145649757, 23.6603},
    {-4.15746230969615, 1.722075956706739, 23.6603},
    {-4.385822690295516, 0.5740240432932615, 23.6603},
    {-4.5000024999958335, 5.510913061611563e-16, 23.6603},
    {-3.0, 3.67394e-16, 8.660250000000001},
    {-2.89778, 3.5487499999999997e-16, 7.883800000000001},
    {-2.5980800000000004, 3.1817299999999995e-16, 7.16025},
    {-2.40031, -0.9942409999999999, 7.16025},
    {-1.9598399999999998, -0.8117939999999999, 6.538930000000001},
    {-1.5965799999999999, -0.6613249999999999, 6.237220000000001},
    {-1.7168100000000002, -0.38274699999999995, 6.2869600000000005},
    {-1.88199, 2.30478e-16, 6.35529},
    {-1.9247100000000001, 0.153449, 6.411479999999999},
    {-2.0215699999999996, 0.50146, 6.538930000000001},
    {-2.05509, 0.690645, 6.6316500000000005},
    {-2.0858, 0.863966, 6.7166},
    {-2.08167, 1.1732399999999998, 6.9403500000000005},
    {-2.08129, 1.2016300000000002, 6.960889999999999},
    {-7.071070000000001, 4.08248, 2.88675},
    {-7.071070000000001, -4.08248, -2.88675},
};

// Projected 2D polyloop vertices:
static const double POLYLOOP_PTS_2D[48][2] = {
    {8.263132401520974, -2.1315213231193958e-08},
    {3.97644944915539, -7.071069975077437},
    {-3.97644944915539, -7.071070024922566},
    {-7.001399437246073, -2.0812899922943293},
    {-6.667233210921609, -2.0857999902465707},
    {-6.389195769684386, -2.0215699878399533},
    {-6.063866830641405, -1.881989984357916},
    {-5.931581174332429, -1.7604399822720684},
    {-5.753246298364511, -1.5965799794603295},
    {-5.996079105640609, -1.959839977537289},
    {-6.534293658529341, -2.4003099745767305},
    {-6.831915219305511, -2.5980799823766922},
    {-7.522286680766843, -2.89777998059584},
    {-8.263132401520974, -2.9999999786847873},
    {-22.575351930915005, -4.500002441761449},
    {-22.91901609515908, -4.2716420701687134},
    {-23.090847604509413, -4.157462264971697},
    {-23.38219207655665, -3.507141188742289},
    {-23.527863827007764, -3.1819817344924344},
    {-23.722534511465064, -2.208710117480332},
    {-23.819869529243654, -1.7220759310882356},
    {-23.888228179604504, -0.5740240194662778},
    {-23.922407390854225, 2.2931226555724775e-08},
    {-23.854048740493376, 1.1480519381362244},
    {-23.819869529243654, 1.7220759823252423},
    {-23.625198844786354, 2.6953476095410673},
    {-23.546227503567387, 3.090171134130144},
    {-23.527863827007764, 3.1819818010350076},
    {-23.236519354960528, 3.8323028925353224},
    {-23.090847604509413, 4.1574623544206055},
    {-22.747183440265346, 4.385822744026603},
    {-22.575351930915005, 4.50000255823022},
    {-8.263132401520974, 3.0000000213152136},
    {-7.522286680766843, 2.897780019404161},
    {-6.831915219305511, 2.5980800176233085},
    {-6.534293658529341, 2.4003100254232708},
    {-5.996079105640609, 1.9598400224627115},
    {-5.753246298364511, 1.5965800205396712},
    {-5.884096528447136, 1.7168100184766106},
    {-6.063866830641405, 1.8819900156420846},
    {-6.163414492606471, 1.9247100145765543},
    {-6.389195769684386, 2.0215700121600477},
    {-6.534295751243208, 2.055090010904073},
    {-6.667233210921609, 2.08580000975343},
    {-6.973302884795011, 2.0816700078778414},
    {-7.001399437246073, 2.0812900077056717},
    {-3.97644944915539, 7.071069975077437},
    {3.97644944915539, 7.071070024922566},
};

static void test_polyloop_projection_simplicity() {
    std::cout << "--- Test 1: Verify 2D Projection Simplicity of Boundary Polyloop ---" << std::endl;
    CGAL::Polygon_2<EK> poly;
    for (int i = 0; i < 48; ++i) {
        poly.push_back(EK::Point_2(POLYLOOP_PTS_2D[i][0], POLYLOOP_PTS_2D[i][1]));
    }
    bool is_simple = poly.is_simple();
    std::cout << "  Polyloop vertex count: " << poly.size() << std::endl;
    std::cout << "  poly.is_simple(): " << (is_simple ? "YES" : "NO") << std::endl;
    
    // Invariant under test: The projected polyloop MUST be non-simple (self-intersecting)
    assert(!is_simple && "Expected projected polyloop from sprue boundary to be non-simple!");
    std::cout << "  ✅ Validated: Polyloop is self-intersecting in 2D projection." << std::endl;
}

static void test_naive_polyloop_extrusion_self_intersects() {
    std::cout << "--- Test 2: Direct 3D Extrusion of Polyloop Segments ---" << std::endl;
    
    // Construct 3D segments connecting 48 vertices in a closed loop
    Geometry seg_geo;
    for (int i = 0; i < 48; ++i) {
        seg_geo.vertices.push_back({
            EK::FT(POLYLOOP_PTS_3D[i][0]),
            EK::FT(POLYLOOP_PTS_3D[i][1]),
            EK::FT(POLYLOOP_PTS_3D[i][2])
        });
        seg_geo.segments.push_back({i, (i + 1) % 48});
    }

    // Extrude along DRAW_DIR by height H = 20.0
    Vector_3 d(DRAW_DIR[0], DRAW_DIR[1], DRAW_DIR[2]);
    FT H(20.0);
    Matrix bottom_tf = Matrix::identity();
    Matrix top_tf = Matrix::translate(d.x() * H, d.y() * H, d.z() * H);

    // Build the extruded quad ribbon manually (identical to ExtrudeOpBase::sweep_single_node segment extrusion)
    Geometry extruded_geo;
    for (const auto& seg : seg_geo.segments) {
        Point_3 s(seg_geo.vertices[seg[0]].x, seg_geo.vertices[seg[0]].y, seg_geo.vertices[seg[0]].z);
        Point_3 t(seg_geo.vertices[seg[1]].x, seg_geo.vertices[seg[1]].y, seg_geo.vertices[seg[1]].z);

        Point_3 s_bot = bottom_tf.transform(s);
        Point_3 t_bot = bottom_tf.transform(t);
        Point_3 s_top = top_tf.transform(s);
        Point_3 t_top = top_tf.transform(t);

        int v0 = (int)extruded_geo.vertices.size();
        extruded_geo.vertices.push_back({s_bot.x(), s_bot.y(), s_bot.z()});
        extruded_geo.vertices.push_back({t_bot.x(), t_bot.y(), t_bot.z()});
        extruded_geo.vertices.push_back({t_top.x(), t_top.y(), t_top.z()});
        extruded_geo.vertices.push_back({s_top.x(), s_top.y(), s_top.z()});

        extruded_geo.faces.push_back({{{v0, v0 + 1, v0 + 2, v0 + 3}}});
    }
    extruded_geo.triangulate();

    Surface_mesh extruded_mesh = Engine::geometry_to_mesh(extruded_geo);
    std::cout << "  Extruded ribbon faces: " << extruded_mesh.number_of_faces() 
              << ", vertices: " << extruded_mesh.number_of_vertices() << std::endl;

    bool self_intersects = CGAL::Polygon_mesh_processing::does_self_intersect(extruded_mesh);
    std::cout << "  CGAL::does_self_intersect(extruded_mesh): " 
              << (self_intersects ? "YES (SELF-INTERSECTING)" : "NO (CLEAN)") << std::endl;

    // Invariant under test: Naive extrusion of a non-simple polyloop produces self-intersecting 3D faces
    assert(self_intersects && "Expected naive 3D extrusion of self-intersecting polyloop to self-intersect!");
    std::cout << "  ✅ Validated: Direct extrusion of this polyloop produces a self-intersecting 3D mesh." << std::endl;
}

static void test_regularized_polyloop_extrusion() {
    std::cout << "--- Test 3: Regularized Polyloop Extrusion (Arrangement + Double Quad) ---" << std::endl;

    Geometry seg_geo;
    for (int i = 0; i < 48; ++i) {
        seg_geo.vertices.push_back({
            EK::FT(POLYLOOP_PTS_3D[i][0]),
            EK::FT(POLYLOOP_PTS_3D[i][1]),
            EK::FT(POLYLOOP_PTS_3D[i][2])
        });
        seg_geo.segments.push_back({i, (i + 1) % 48});
    }

    Vector_3 d(DRAW_DIR[0], DRAW_DIR[1], DRAW_DIR[2]);
    FT H(20.0);
    Matrix bottom_tf = Matrix::identity();
    Matrix top_tf = Matrix::translate(d.x() * H, d.y() * H, d.z() * H);

    auto meshes = jotcad::geo::extrude::PolylineExtruder::extrude_segments(seg_geo, bottom_tf, top_tf);
    std::cout << "  Decomposed into " << meshes.size() << " simple polygon mesh components." << std::endl;
    assert(!meshes.empty() && "Expected at least one mesh component from regularized extrusion!");

    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto& m = meshes[i];
        bool self_intersects = CGAL::Polygon_mesh_processing::does_self_intersect(m);
        std::cout << "    Component " << (i + 1) << "/" << meshes.size() 
                  << " | vertices: " << m.number_of_vertices() 
                  << " | faces: " << m.number_of_faces()
                  << " | does_self_intersect: " << (self_intersects ? "YES" : "NO") << std::endl;
        if (self_intersects) {
            std::vector<std::pair<ExactMesh::Face_index, ExactMesh::Face_index>> pairs;
            CGAL::Polygon_mesh_processing::self_intersections(m, std::back_inserter(pairs));
            std::cout << "      Found " << pairs.size() << " self-intersecting face pairs:" << std::endl;
            for (size_t p = 0; p < std::min<size_t>(5, pairs.size()); ++p) {
                auto f1 = pairs[p].first;
                auto f2 = pairs[p].second;
                std::cout << "        Pair " << p << ": f" << f1 << " and f" << f2 << std::endl;
                std::cout << "          f1 points: ";
                for (auto v : m.vertices_around_face(m.halfedge(f1))) {
                    std::cout << "(" << CGAL::to_double(m.point(v).x()) << "," << CGAL::to_double(m.point(v).y()) << "," << CGAL::to_double(m.point(v).z()) << ") ";
                }
                std::cout << std::endl;
                std::cout << "          f2 points: ";
                for (auto v : m.vertices_around_face(m.halfedge(f2))) {
                    std::cout << "(" << CGAL::to_double(m.point(v).x()) << "," << CGAL::to_double(m.point(v).y()) << "," << CGAL::to_double(m.point(v).z()) << ") ";
                }
                std::cout << std::endl;
            }
        }
        assert(!self_intersects && "Regularized mesh component must NOT self-intersect!");
    }

    std::cout << "  ✅ Validated: All regularized mesh components have ZERO self-intersections." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "Running Extrude Polyloop Test Suite" << std::endl;
    std::cout << "==================================================" << std::endl;

    test_polyloop_projection_simplicity();
    test_naive_polyloop_extrusion_self_intersects();
    test_regularized_polyloop_extrusion();

    std::cout << "\n✅ ALL Extrude Polyloop Tests Passed" << std::endl;
    return 0;
}
