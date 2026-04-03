#include "../include/processor.h"
#include <iostream>
#include <cmath>
#include <algorithm>

using namespace jotcad::geo;

int main(int argc, char** argv) {
    return Processor::run(argc, argv, [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params) {
        std::cout << "[TriangleAgent] Computing triangle for " << path << " " << params.dump() << std::endl;

        double a = 0, b = 0, c = 0;
        std::string form = params.value("form", "SSS");

        if (form == "SSS") {
            a = params["a"]; b = params["b"]; c = params["c"];
        } else if (form == "SAS") {
            a = params["a"]; b = params["b"];
            double angle_rad = (double)params["angle"] * M_PI / 180.0;
            c = std::sqrt(a*a + b*b - 2*a*b*std::cos(angle_rad));
        } else if (form == "equilateral") {
            a = b = c = params["side"];
        }

        std::vector<double> sides = {a, b, c};
        std::sort(sides.begin(), sides.end());
        auto round_near = [](double v) { return std::round(v * 1e10) / 1e10; };
        nlohmann::json canonical_geo_params = {{"a", round_near(sides[0])}, {"b", round_near(sides[1])}, {"c", round_near(sides[2])}, {"type", "triangle"}};
        std::string geo_path = "geo/mesh";

        // 1. Write geometry to canonical location
        if (vfs->status(geo_path, canonical_geo_params) != "AVAILABLE") {
            Geometry geo;
            geo.vertices.push_back({0, 0, 0});
            geo.vertices.push_back({sides[0], 0, 0});
            double cosA = (sides[0]*sides[0] + sides[2]*sides[2] - sides[1]*sides[1]) / (2 * sides[0] * sides[2]);
            double sinA = std::sqrt(1 - cosA*cosA);
            geo.vertices.push_back({sides[2] * cosA, sides[2] * sinA, 0});
            geo.faces.push_back({{{0, 1, 2}}});
            std::string geo_text = geo.encode_text();
            vfs->write(geo_path, canonical_geo_params, std::vector<uint8_t>(geo_text.begin(), geo_text.end()));
        }

        // 2. Link the requested coordinate to the canonical geometry
        // We create a sub-coordinate named 'geometry' that points to the raw mesh
        vfs->link(path + "/geometry", params, geo_path, canonical_geo_params);

        // 3. Write the Shape JSON
        nlohmann::json shape = {
            {"geometry", "vfs:./geometry"}, // Visible relative link
            {"parameters", canonical_geo_params},
            {"tags", {{"type", "triangle"}}}
        };
        std::string shape_text = shape.dump(2);
        vfs->write(path, params, std::vector<uint8_t>(shape_text.begin(), shape_text.end()));
        
        std::cout << "[TriangleAgent] Fulfilled " << path << " with explicit link." << std::endl;
    });
}
