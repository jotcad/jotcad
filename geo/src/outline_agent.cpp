#include "../include/processor.h"
#include <iostream>
#include <vector>

using namespace jotcad::geo;

/**
 * op/outline Realizer
 * 
 * Simple implementation: converts faces to segments.
 */
void apply_outline(Geometry& geo) {
    geo.segments.clear();
    for (const auto& face : geo.faces) {
        for (const auto& loop : face.loops) {
            if (loop.empty()) continue;
            for (size_t i = 0; i < loop.size(); ++i) {
                int v1 = loop[i];
                int v2 = loop[(i + 1) % loop.size()];
                geo.segments.push_back({v1, v2});
            }
        }
    }
    // Remove faces as we now have segments
    geo.faces.clear();
}

int main(int argc, char** argv) {
    return Processor::run(argc, argv, [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params) {
        std::cout << "[OutlineAgent] Computing outline for " << path << std::endl;

        std::string source_uri = params["$in"];

        // Resolve source path and parameters
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
        apply_outline(geo);

        // Encode and Write
        std::string out = geo.encode_text();
        vfs->write(path, params, std::vector<uint8_t>(out.begin(), out.end()));
    });
}
