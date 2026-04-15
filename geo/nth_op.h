#pragma once
#include "impl/processor.h"
#include "impl/geometry.h"
#include <vector>

namespace jotcad {
namespace geo {

static void nth_init() {
    Processor::Operation op;
    op.path = "jot/nth";
    op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::cout << "[Nth Op] Filtering sequence..." << std::endl;
        
        auto in_selector = params.at("$in");
        jotcad::fs::VFSNode::VFSRequest in_req;
        in_req.path = in_selector["path"];
        in_req.parameters = in_selector.value("parameters", nlohmann::json::object());
        in_req.stack = stack;
        auto shape_bytes = vfs->read(in_req);
        if (shape_bytes.empty()) return std::vector<uint8_t>();

        // For Nth, if the input is already a sequence (Group/Array), we pick members.
        // If it's a single shape, we pick from its internal components if they exist.
        nlohmann::json in_shape = nlohmann::json::parse(std::string(shape_bytes.begin(), shape_bytes.end()));
        auto indices = params.at("indices").get<std::vector<int>>();

        // TODO: Implement proper sequence filtering. 
        // For now, return the original shape as a pass-through if index 0 is requested.
        for (int idx : indices) if (idx == 0) return shape_bytes;

        return std::vector<uint8_t>();
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "shape"}}},
            {"indices", {{"type", "array"}, {"items", {{"type", "integer"}}}}}
        }},
        {"inputs", {
            {"$in", {{"type", "shape"}}}
        }},
        {"outputs", {
            {"$out", {{"type", "shape"}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
