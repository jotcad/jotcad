#pragma once
#include "impl/processor.h"
#include "impl/offset.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> offset_op(jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params) {
    if (!params.contains("source")) return std::vector<uint8_t>();
    double kerf = params.value("kerf", 0.0);

    std::string src_path;
    nlohmann::json src_params = nlohmann::json::object();

    if (params["source"].is_object()) {
        src_path = params["source"].value("path", "");
        src_params = params["source"].value("parameters", nlohmann::json::object());
    } else {
        // Fallback for string URIs
        src_path = params["source"].get<std::string>();
        if (src_path.find("vfs:/") == 0) src_path = src_path.substr(5);
        size_t q = src_path.find("?");
        if (q != std::string::npos) {
            std::string query = src_path.substr(q + 1);
            src_path = src_path.substr(0, q);
            try { src_params = nlohmann::json::parse(query); } catch(...) {}
        }
    }

    // 1. Read the Shape artifact
    std::cout << "[Offset Op] Reading Shape from: " << src_path << " with params: " << src_params.dump() << std::endl;
    std::vector<uint8_t> shape_bytes = vfs->read(src_path, src_params);
    nlohmann::json shape = nlohmann::json::parse(shape_bytes.begin(), shape_bytes.end());

    // 2. Resolve and read the underlying Geometry
    std::string geo_uri = shape.value("geometry", "");
    if (geo_uri.find("vfs:/") != 0) throw std::runtime_error("Invalid geometry URI: " + geo_uri);
    std::string geo_path = geo_uri.substr(5);
    nlohmann::json geo_params = shape.value("parameters", nlohmann::json::object());

    std::cout << "[Offset Op] Reading Mesh from: " << geo_path << std::endl;
    std::vector<uint8_t> mesh_bytes = vfs->read(geo_path, geo_params);
    
    Geometry geo;
    geo.decode_text(std::string(mesh_bytes.begin(), mesh_bytes.end()));
    
    applyOffset(geo, kerf);

    // 3. Write new mesh and return new Shape
    std::string mesh_text = geo.encode_text();
    vfs->write("geo/mesh", params, std::vector<uint8_t>(mesh_text.begin(), mesh_text.end()));

    // Give the Hub a tiny moment to process the write
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    nlohmann::json tags = shape.value("tags", nlohmann::json::array());
    if (!tags.is_array()) {
        nlohmann::json arr = nlohmann::json::array();
        if (!tags.is_null()) arr.push_back(tags);
        tags = arr;
    }
    tags.push_back("op:offset");

    nlohmann::json out_shape = {
        {"geometry", "vfs:/geo/mesh"},
        {"parameters", params},
        {"tags", tags}
    };

    std::string out_text = out_shape.dump(2);
    return std::vector<uint8_t>(out_text.begin(), out_text.end());
}

static void offset_init() {
    Processor::Operation op;
    op.path = "op/offset";
    op.logic = offset_op;
    op.schema = {
        {"type", "object"},
        {"properties", {
            {"source", {{"type", "string"}}},
            {"kerf", {{"type", "number"}, {"default", 0.0}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
