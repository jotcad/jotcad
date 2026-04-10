#include "../include/processor.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_traits_2.h>
#include <CGAL/minkowski_sum_2.h>
#include <iostream>
#include <vector>

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Polygon_2<EK> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;
typedef CGAL::Gps_segment_traits_2<EK> Traits;
typedef CGAL::General_polygon_set_2<Traits> General_polygon_set_2;

using namespace jotcad::geo;

// Simplified offset implementation for the agent
void apply_offset(Geometry& geo, double k) {
    if (k == 0) return;

    for (auto& face : geo.faces) {
        if (face.loops.empty()) continue;

        // 1. Convert outer boundary to CGAL Polygon
        Polygon_2 boundary;
        for (int idx : face.loops[0]) {
            boundary.push_back(EK::Point_2(geo.vertices[idx].x, geo.vertices[idx].y));
        }
        if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();

        // 2. Create tool (a small circle approximation for uniform offset)
        Polygon_2 tool;
        int segments = 16;
        double abs_k = std::abs(k);
        for (int i = 0; i < segments; ++i) {
            double a = 2.0 * M_PI * i / segments;
            tool.push_back(EK::Point_2(abs_k * cos(a), abs_k * sin(a)));
        }

        // 3. Compute Minkowski sum (Offset)
        Polygon_with_holes_2 offset_pwh;
        if (k > 0) {
            offset_pwh = CGAL::minkowski_sum_2(boundary, tool);
        } else {
            // Insetting is more complex (Minkowski sum of complement). 
            // For now, let's focus on positive kerf to verify the modular pattern.
            // Simplified: if k < 0, we'll just skip or do a naive scale for the prototype.
            std::cout << "[OffsetAgent] Insetting (k<0) not implemented in this prototype." << std::endl;
            continue;
        }

        // 4. Update Geometry with new vertices and face
        face.loops.clear();
        std::vector<int> new_loop;
        const auto& ob = offset_pwh.outer_boundary();
        for (auto it = ob.vertices_begin(); it != ob.vertices_end(); ++it) {
            new_loop.push_back(geo.vertices.size());
            geo.vertices.push_back({CGAL::to_double(it->x()), CGAL::to_double(it->y()), 0.0});
        }
        face.loops.push_back(new_loop);
    }
}

int main(int argc, char** argv) {
    return Processor::run(argc, argv, [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params) {
        std::cout << "[OffsetAgent] Computing offset for " << path << std::endl;

        std::string source_uri = params["source"];
        double kerf = params.value("kerf", 0.0);

        // Simple URI parser
        std::string source_path = source_uri;
        nlohmann::json source_params = nlohmann::json::object();
        if (source_path.find("vfs:/") == 0) source_path = source_path.substr(5);
        size_t q = source_path.find("?");
        if (q != std::string::npos) {
            std::string query = source_path.substr(q + 1);
            source_path = source_path.substr(0, q);
            try { source_params = nlohmann::json::parse(query); } catch(...) {}
        }

        // Read and Decode
        std::vector<uint8_t> bytes = vfs->read(source_path, source_params);
        Geometry geo = Geometry::decode_text(std::string(bytes.begin(), bytes.end()));

        // Transform
        apply_offset(geo, kerf);

        // Encode and Write
        std::string out = geo.encode_text();
        vfs->write(path, params, std::vector<uint8_t>(out.begin(), out.end()));
    });
}
