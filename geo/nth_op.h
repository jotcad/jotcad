#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct NthOp : P {
    static constexpr const char* path = "jot/nth";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& in, const std::vector<double>& indices, Shape& out) {
        std::vector<Shape> results;
        for (double idx_d : indices) {
            int idx = (int)idx_d;
            if (idx >= 0 && idx < (int)in.size()) {
                results.push_back(in[idx]);
            } else {
                throw jotcad::fs::VFSException("Index out of bounds in nth: " + std::to_string(idx), 400);
            }
        }

        if (results.size() == 1) out = results[0];
        else {
            // Wrap in op/group shape
            out.geometry = {"op/group", nlohmann::json::object()};
            nlohmann::json items = nlohmann::json::array();
            for (const auto& r : results) items.push_back(r.to_json());
            out.geometry.parameters["items"] = items;
        }
    }

    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        auto in = Processor::decode<std::vector<Shape>>(vfs, "$in", params, schema(), stack);
        auto indices = Processor::decode<std::vector<double>>(vfs, "indices", params, schema(), stack);
        
        Shape out;
        execute(vfs, in, indices, out);
        
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shapes"}}},
                {"indices", {{"type", "jot:numbers"}, {"default", 0}}}
            }},
            {"inputs", {
                {"$in", {{"type", "shapes"}}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}}}
            }}
        };
    }
};

static void nth_init() {
    Processor::register_op<NthOp<>>();
}

} // namespace geo
} // namespace jotcad
