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

class STLReader {
public:
    static bool read_binary(const std::vector<uint8_t>& buffer, Geometry& out_geo) {
        if (buffer.size() < 84) return false;
        
        uint32_t count = 0;
        std::memcpy(&count, buffer.data() + 80, 4);
        
        if (buffer.size() < 84 + count * 50) return false;
        
        out_geo.vertices.clear();
        out_geo.triangles.clear();
        out_geo.faces.clear();
        out_geo.vertices.reserve(count * 3);
        out_geo.triangles.reserve(count);
        
        // Map to deduplicate vertices and reuse indices
        struct ComparePoint {
            bool operator()(const std::array<float, 3>& a, const std::array<float, 3>& b) const {
                if (std::abs(a[0] - b[0]) > 1e-5f) return a[0] < b[0];
                if (std::abs(a[1] - b[1]) > 1e-5f) return a[1] < b[1];
                return a[2] < b[2];
            }
        };
        std::map<std::array<float, 3>, int, ComparePoint> vertex_map;
        
        auto get_or_add_vertex = [&](float x, float y, float z) -> int {
            std::array<float, 3> pt = {x, y, z};
            auto it = vertex_map.find(pt);
            if (it != vertex_map.end()) {
                return it->second;
            }
            int idx = (int)out_geo.vertices.size();
            Vertex v;
            v.x = (FT)x;
            v.y = (FT)y;
            v.z = (FT)z;
            out_geo.vertices.push_back(v);
            vertex_map[pt] = idx;
            return idx;
        };
        
        for (uint32_t i = 0; i < count; ++i) {
            size_t offset = 84 + i * 50;
            float normal[3];
            float v1[3], v2[3], v3[3];
            
            std::memcpy(normal, buffer.data() + offset, 12);
            std::memcpy(v1, buffer.data() + offset + 12, 12);
            std::memcpy(v2, buffer.data() + offset + 24, 12);
            std::memcpy(v3, buffer.data() + offset + 36, 12);
            
            int idx1 = get_or_add_vertex(v1[0], v1[1], v1[2]);
            int idx2 = get_or_add_vertex(v2[0], v2[1], v2[2]);
            int idx3 = get_or_add_vertex(v3[0], v3[1], v3[2]);
            
            out_geo.triangles.push_back({idx1, idx2, idx3});
        }
        
        return true;
    }

    static bool read_file(const std::string& path, Geometry& out_geo) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        file.read((char*)buffer.data(), size);
        return read_binary(buffer, out_geo);
    }
};

} // namespace geo
} // namespace jotcad
