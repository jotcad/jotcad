#include "test_base.h"
#include "render/rasterizer.h"
#include "infra/stl.h"
#include <fstream>
#include <cmath>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>

using namespace jotcad;
using namespace jotcad::geo;

struct Point3D {
    double x, y, z;
};

// Project a point p radially from center c to the boundary of a box
Point3D project_to_box(Point3D p, Point3D c, double xmin, double xmax, double ymin, double ymax, double zmin, double zmax) {
    double dx = p.x - c.x;
    double dy = p.y - c.y;
    double dz = p.z - c.z;

    double t = 999999.0;

    if (std::abs(dx) > 1e-9) {
        double tx = (dx > 0) ? (xmax - c.x) / dx : (xmin - c.x) / dx;
        if (tx > 0 && tx < t) t = tx;
    }
    if (std::abs(dy) > 1e-9) {
        double ty = (dy > 0) ? (ymax - c.y) / dy : (ymin - c.y) / dy;
        if (ty > 0 && ty < t) t = ty;
    }
    if (std::abs(dz) > 1e-9) {
        double tz = (dz > 0) ? (zmax - c.z) / dz : (zmin - c.z) / dz;
        if (tz > 0 && tz < t) t = tz;
    }

    return {c.x + t * dx, c.y + t * dy, c.z + t * dz};
}

void run_mold_split_test() {
    MockVFS vfs("mold_split");
    register_all_ops(&vfs);

    std::cout << "Running Mold Splitting Test on Bear..." << std::endl;

    // 1. Load the bear STL
    std::string bear_path = "scratch/bear.stl";
    {
        std::ifstream check(bear_path);
        if (!check.good()) {
            check.open("../scratch/bear.stl");
            if (check.good()) {
                bear_path = "../scratch/bear.stl";
            } else {
                bear_path = "../../scratch/bear.stl";
            }
        }
    }
    Geometry bear_geo;
    if (!STLReader::read_file(bear_path, bear_geo)) {
        std::cerr << "  ❌ FAIL: Could not load bear.stl" << std::endl;
        return;
    }
    std::cout << "  - Loaded bear.stl with " << bear_geo.triangles.size() << " triangles." << std::endl;

    // 2. Define the mold block bounding box centered around the bear
    // Bear bounding box spans:
    // X: -37.4 to 36.7 (span 74)
    // Y: -103.9 to 105.5 (span 209)
    // Z: -0.04 to 116.2 (span 116)
    double xmin = -50.0, xmax = 50.0;
    double ymin = -120.0, ymax = 120.0;
    double zmin = -10.0, zmax = 130.0;

    Point3D center = {
        (-37.42283 + 36.74454) / 2.0,
        (-103.93603 + 105.55215) / 2.0,
        (-0.04631 + 116.20885) / 2.0
    };

    // 3. Compute optimal direction and extract parting line segments
    double dx = 0.932708, dy = -0.26745, dz = 0.241922;
    IK::Vector_3 dir_ik(dx, dy, dz);

    // Compute face normals
    std::vector<IK::Vector_3> face_normals(bear_geo.triangles.size());
    for (size_t f_idx = 0; f_idx < bear_geo.triangles.size(); ++f_idx) {
        const auto& tri = bear_geo.triangles[f_idx];
        const auto& p0 = bear_geo.vertices[tri[0]];
        const auto& p1 = bear_geo.vertices[tri[1]];
        const auto& p2 = bear_geo.vertices[tri[2]];
        
        double ux = CGAL::to_double(p1.x - p0.x);
        double uy = CGAL::to_double(p1.y - p0.y);
        double uz = CGAL::to_double(p1.z - p0.z);

        double vx = CGAL::to_double(p2.x - p0.x);
        double vy = CGAL::to_double(p2.y - p0.y);
        double vz = CGAL::to_double(p2.z - p0.z);

        double nx = uy * vz - uz * vy;
        double ny = uz * vx - ux * vz;
        double nz = ux * vy - uy * vx;

        double flen = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (flen > 1e-12) {
            nx /= flen; ny /= flen; nz /= flen;
        }
        face_normals[f_idx] = IK::Vector_3(nx, ny, nz);
    }

    // Build edge-to-faces map
    struct Edge {
        int u, v;
        bool operator<(const Edge& other) const {
            if (u != other.u) return u < other.u;
            return v < other.v;
        }
    };
    std::map<Edge, std::vector<int>> edge_to_faces;
    for (int f_idx = 0; f_idx < (int)bear_geo.triangles.size(); ++f_idx) {
        const auto& tri = bear_geo.triangles[f_idx];
        for (int i = 0; i < 3; ++i) {
            int u = tri[i];
            int v = tri[(i + 1) % 3];
            Edge edge = {std::min(u, v), std::max(u, v)};
            edge_to_faces[edge].push_back(f_idx);
        }
    }

    // Extract parting line segments and vertices
    std::vector<std::pair<int, int>> parting_segments;
    for (const auto& [edge, faces] : edge_to_faces) {
        bool is_part_line = false;
        if (faces.size() == 1) {
            is_part_line = true;
        } else if (faces.size() >= 2) {
            bool has_pos = false;
            bool has_neg = false;
            for (int f_idx : faces) {
                double dot = CGAL::to_double(face_normals[f_idx] * dir_ik);
                if (dot > 1e-9) has_pos = true;
                else if (dot < -1e-9) has_neg = true;
            }
            if (has_pos && has_neg) {
                is_part_line = true;
            }
        }
        if (is_part_line) {
            parting_segments.push_back({edge.u, edge.v});
        }
    }
    std::cout << "  - Extracted " << parting_segments.size() << " parting line segments." << std::endl;

    // 4. Construct parting surface geometry
    Geometry parting_surf_geo;
    std::map<int, int> v_map; // maps bear vertex index -> parting surface inner vertex index
    std::map<int, int> proj_v_map; // maps bear vertex index -> parting surface outer projected vertex index

    auto get_or_add_inner_vertex = [&](int bear_idx) -> int {
        auto it = v_map.find(bear_idx);
        if (it != v_map.end()) return it->second;
        int new_idx = (int)parting_surf_geo.vertices.size();
        parting_surf_geo.vertices.push_back(bear_geo.vertices[bear_idx]);
        v_map[bear_idx] = new_idx;
        return new_idx;
    };

    auto get_or_add_outer_vertex = [&](int bear_idx) -> int {
        auto it = proj_v_map.find(bear_idx);
        if (it != proj_v_map.end()) return it->second;
        
        Point3D p = {
            CGAL::to_double(bear_geo.vertices[bear_idx].x),
            CGAL::to_double(bear_geo.vertices[bear_idx].y),
            CGAL::to_double(bear_geo.vertices[bear_idx].z)
        };
        Point3D proj_p = project_to_box(p, center, xmin, xmax, ymin, ymax, zmin, zmax);

        int new_idx = (int)parting_surf_geo.vertices.size();
        Vertex v;
        v.x = (FT)proj_p.x;
        v.y = (FT)proj_p.y;
        v.z = (FT)proj_p.z;
        parting_surf_geo.vertices.push_back(v);
        proj_v_map[bear_idx] = new_idx;
        return new_idx;
    };

    for (const auto& seg : parting_segments) {
        int u_inner = get_or_add_inner_vertex(seg.first);
        int v_inner = get_or_add_inner_vertex(seg.second);
        int u_outer = get_or_add_outer_vertex(seg.first);
        int v_outer = get_or_add_outer_vertex(seg.second);

        // Quad consisting of (u_inner, v_inner, v_outer, u_outer)
        // Triangulate the quad
        parting_surf_geo.triangles.push_back({u_inner, v_inner, v_outer});
        parting_surf_geo.triangles.push_back({u_inner, v_outer, u_outer});
    }

    std::cout << "  - Generated parting surface with " << parting_surf_geo.triangles.size() << " triangles." << std::endl;

    // 5. Render composite shape (bear + parting line + parting surface)
    Shape bear_shape = JotVfsProtocol::make_shape(&vfs, bear_geo, {{"color", "#cccccc"}, {"name", "bear"}});
    
    // Create parting line geometry for visualization
    Geometry line_geo;
    std::map<int, int> line_vmap;
    for (const auto& seg : parting_segments) {
        auto get_idx = [&](int old_idx) {
            auto it = line_vmap.find(old_idx);
            if (it != line_vmap.end()) return it->second;
            int n_idx = (int)line_geo.vertices.size();
            line_geo.vertices.push_back(bear_geo.vertices[old_idx]);
            line_vmap[old_idx] = n_idx;
            return n_idx;
        };
        int u = get_idx(seg.first);
        int v = get_idx(seg.second);
        line_geo.segments.push_back({u, v});
    }
    Shape line_shape = JotVfsProtocol::make_shape(&vfs, line_geo, {{"color", "#2bee2b"}, {"name", "parting_line"}, {"type", "outline"}});

    // Parting surface shape
    Shape surf_shape = JotVfsProtocol::make_shape(&vfs, parting_surf_geo, {{"color", "#ee2b2b"}, {"name", "parting_surface"}});

    // Group them
    Shape composite;
    composite.components.push_back(bear_shape);
    composite.components.push_back(surf_shape);
    composite.components.push_back(line_shape);

    composite.tf = Matrix::rotationX(-0.61547) * Matrix::rotationY(0.78539);

    std::cout << "  - Rendering mold split composite..." << std::endl;
    auto png_data = Rasterizer::render_png(&vfs, composite, 1024, 1024, 0.0, 0.0);
    if (!png_data.empty()) {
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/bear_mold_split.png", std::ios::binary);
        out.write((const char*)png_data.data(), png_data.size());
        std::cout << "  📸 Saved actual/bear_mold_split.png" << std::endl;
    } else {
        std::cerr << "  ❌ FAIL: Empty render output." << std::endl;
    }

    std::cout << "  ✅ Mold Splitting Test Completed." << std::endl;
}

int main() {
    run_mold_split_test();
    return 0;
}
