#include "../include/processor.h"
#include <iostream>
#include <cmath>
#include <vector>

using namespace jotcad::geo;

int main(int argc, char** argv) {
    return Processor::run(argc, argv, [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params) {
        std::cout << "[HexagonAgent] Computing hexagon for " << path << " " << params.dump() << std::endl;

        double radius = params.value("radius", 10.0);
        std::string variant = params.value("variant", "full");
        
        Geometry geo;
        double s3 = std::sqrt(3.0);

        if (variant == "middle") {
            // Rectangular middle section
            geo.vertices.push_back({  radius/2.0,  radius * s3 / 2.0, 0.0 });
            geo.vertices.push_back({ -radius/2.0,  radius * s3 / 2.0, 0.0 });
            geo.vertices.push_back({ -radius/2.0, -radius * s3 / 2.0, 0.0 });
            geo.vertices.push_back({  radius/2.0, -radius * s3 / 2.0, 0.0 });
        } else if (variant == "cap") {
            // Triangular top cap
            geo.vertices.push_back({ radius, 0.0, 0.0 });
            geo.vertices.push_back({ radius/2.0,  radius * s3 / 2.0, 0.0 });
            geo.vertices.push_back({ radius/2.0, -radius * s3 / 2.0, 0.0 });
        } else if (variant == "sector") {
            // One of the six triangular sectors
            geo.vertices.push_back({ 0.0, 0.0, 0.0 });
            geo.vertices.push_back({ radius, 0.0, 0.0 });
            geo.vertices.push_back({ radius / 2.0, radius * s3 / 2.0, 0.0 });
        } else if (variant == "half") {
            // Half Hexagon (Trapezoid)
            geo.vertices.push_back({  radius, 0.0, 0.0 }); 
            geo.vertices.push_back({  radius / 2.0,  radius * s3 / 2.0, 0.0 }); 
            geo.vertices.push_back({ -radius / 2.0,  radius * s3 / 2.0, 0.0 }); 
            geo.vertices.push_back({ -radius, 0.0, 0.0 }); 
        } else {
            // Full Hexagon
            for (int i = 0; i < 6; ++i) {
                double angle_rad = (M_PI / 180.0) * (60.0 * i);
                geo.vertices.push_back({ radius * std::cos(angle_rad), radius * std::sin(angle_rad), 0.0 });
            }
        }

        // 2. Generate Face
        Geometry::Face face;
        std::vector<int> indices;
        for (size_t i = 0; i < geo.vertices.size(); ++i) indices.push_back(i);
        face.loops.push_back(indices);
        geo.faces.push_back(face);

        std::string geo_text = geo.encode_text();
        std::vector<uint8_t> geo_data(geo_text.begin(), geo_text.end());

        // 3. Write Geometry to VFS
        std::string geo_path = "geo/mesh";
        vfs->write(geo_path, params, geo_data);

        // 4. Write Shape JSON
        nlohmann::json shape = {
            {"geometry", "vfs:/" + geo_path},
            {"parameters", params},
            {"tags", {{"type", "hexagon"}}}
        };
        std::string shape_text = shape.dump(2);
        std::vector<uint8_t> shape_data(shape_text.begin(), shape_text.end());

        vfs->write(path, params, shape_data);

        std::cout << "[HexagonAgent] Successfully fulfilled " << path << std::endl;
    });
}
