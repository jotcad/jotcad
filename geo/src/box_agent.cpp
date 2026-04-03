#include "../include/processor.h"
#include <iostream>

using namespace jotcad::geo;

int main(int argc, char** argv) {
    return Processor::run(argc, argv, [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params) {
        std::cout << "[BoxAgent] Computing box for " << path << " " << params.dump() << std::endl;

        double w = params.value("width", 1.0);
        double h = params.value("height", 1.0);
        double d = params.value("depth", 1.0);

        // 1. Generate Vertices
        Geometry geo;
        geo.vertices = {
            {0, 0, 0}, {w, 0, 0}, {w, h, 0}, {0, h, 0},
            {0, 0, d}, {w, 0, d}, {w, h, d}, {0, h, d}
        };

        // 2. Generate Faces (6 sides)
        geo.faces = {
            {{{0, 1, 2, 3}}}, // Bottom
            {{{4, 5, 6, 7}}}, // Top
            {{{0, 1, 5, 4}}}, // Front
            {{{1, 2, 6, 5}}}, // Right
            {{{2, 3, 7, 6}}}, // Back
            {{{3, 0, 4, 7}}}  // Left
        };

        std::string geo_text = geo.encode_text();
        std::vector<uint8_t> geo_data(geo_text.begin(), geo_text.end());

        // 3. Write Geometry to a content-addressed coordinate
        // We use the same parameters to ensure uniqueness
        std::string geo_path = "geo/mesh";
        vfs->write(geo_path, params, geo_data);

        // 4. Write Shape JSON to the requested coordinate
        nlohmann::json shape = {
            {"geometry", "vfs:/" + geo_path},
            {"parameters", params},
            {"tags", {{"type", "box"}}}
        };
        std::string shape_text = shape.dump(2);
        std::vector<uint8_t> shape_data(shape_text.begin(), shape_text.end());

        vfs->write(path, params, shape_data);

        std::cout << "[BoxAgent] Successfully fulfilled " << path << std::endl;
    });
}
