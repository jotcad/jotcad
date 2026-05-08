#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include "geometry.h"
#include "triangulation.h"

namespace jotcad {
namespace geo {

class STLWriter {
    struct Triangle {
        float normal[3];
        float v1[3];
        float v2[3];
        float v3[3];
        uint16_t attr_byte_count;
    };

    std::vector<Triangle> triangles;

public:
    void add_triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
        Triangle t;
        EK::Point_3 p1(v1.x, v1.y, v1.z);
        EK::Point_3 p2(v2.x, v2.y, v2.z);
        EK::Point_3 p3(v3.x, v3.y, v3.z);

        if (CGAL::collinear(p1, p2, p3)) return;

        // Use unnormalized normal to avoid EK sqrt issues
        EK::Vector_3 n_vec = CGAL::normal(p1, p2, p3);
        double nx = CGAL::to_double(n_vec.x());
        double ny = CGAL::to_double(n_vec.y());
        double nz = CGAL::to_double(n_vec.z());
        double len = std::sqrt(nx*nx + ny*ny + nz*nz);
        
        if (len > 1e-12) {
            t.normal[0] = (float)(nx / len);
            t.normal[1] = (float)(ny / len);
            t.normal[2] = (float)(nz / len);
        } else {
            t.normal[0] = 0; t.normal[1] = 0; t.normal[2] = 0;
        }
        
        t.v1[0] = (float)CGAL::to_double(v1.x);
        t.v1[1] = (float)CGAL::to_double(v1.y);
        t.v1[2] = (float)CGAL::to_double(v1.z);
        
        t.v2[0] = (float)CGAL::to_double(v2.x);
        t.v2[1] = (float)CGAL::to_double(v2.y);
        t.v2[2] = (float)CGAL::to_double(v2.z);
        
        t.v3[0] = (float)CGAL::to_double(v3.x);
        t.v3[1] = (float)CGAL::to_double(v3.y);
        t.v3[2] = (float)CGAL::to_double(v3.z);
        
        t.attr_byte_count = 0;
        triangles.push_back(t);
    }

    void add_geometry(const Geometry& geo) {
        // 1. Process explicit triangles
        for (const auto& t_indices : geo.triangles) {
            add_triangle(geo.vertices[t_indices[0]], 
                         geo.vertices[t_indices[1]], 
                         geo.vertices[t_indices[2]]);
        }
        
        // 2. Process complex faces (triangulate)
        for (const auto& f : geo.faces) {
            std::vector<Vec3> pts;
            for (const auto& v : geo.vertices) {
                pts.push_back(Vec3{CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)});
            }
            
            Triangulation::triangulate_face(f, pts, [&](int i0, int i1, int i2) {
                add_triangle(geo.vertices[i0], geo.vertices[i1], geo.vertices[i2]);
            });
        }
    }

    std::vector<uint8_t> write_binary() const {
        std::vector<uint8_t> buffer;
        buffer.resize(80 + 4 + triangles.size() * 50);
        
        // 80 byte header
        std::string header = "JotCAD Binary STL Export";
        header.resize(80, ' ');
        std::memcpy(buffer.data(), header.data(), 80);
        
        // 4 byte triangle count
        uint32_t count = (uint32_t)triangles.size();
        std::memcpy(buffer.data() + 80, &count, 4);
        
        // Triangles
        for (size_t i = 0; i < triangles.size(); ++i) {
            std::memcpy(buffer.data() + 84 + i * 50, &triangles[i], 50);
        }
        
        return buffer;
    }
};

} // namespace geo
} // namespace jotcad
