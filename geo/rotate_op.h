#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct RotateOp : P {
    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& sources, const std::vector<double>& turns, int axis, Shape& out) {
        std::vector<Shape> results;
        for (auto src : sources) {
            for (double t : turns) {
                Shape res = src;
                res.geometry.parameters["turns"] = t;
                res.geometry.parameters["axis"] = axis;
                results.push_back(res);
            }
        }

        if (results.size() == 1) out = results[0];
        else {
            out.geometry = {"jot/group", nlohmann::json::object()};
            nlohmann::json items = nlohmann::json::array();
            for (const auto& r : results) items.push_back(r.to_json());
            out.geometry.parameters["items"] = items;
        }
    }

    static std::vector<uint8_t> rotate_logic(jotcad::fs::VFSNode* vfs, const typename P::json& params, const std::vector<std::string>& stack, int axis) {
        auto in = Processor::decode<std::vector<Shape>>(vfs, "$in", params, schema(), stack);
        auto turns = Processor::decode<std::vector<double>>(vfs, "turns", params, schema(), stack);
        
        Shape out;
        execute(vfs, in, turns, axis, out);
        return P::write_shape_obj(out);
    }

    static std::vector<uint8_t> rotateX(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        return rotate_logic(vfs, params, stack, 0);
    }
    static std::vector<uint8_t> rotateY(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        return rotate_logic(vfs, params, stack, 1);
    }
    static std::vector<uint8_t> rotateZ(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        return rotate_logic(vfs, params, stack, 2);
    }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shapes"}}},
                {"turns", {{"type", "jot:numbers"}, {"default", 0.0}}}
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

static void rotate_init() {
    auto reg = [](const std::string& path, auto handler, const std::string& alias) {
        auto schema = RotateOp<>::schema();
        schema["metadata"]["alias"] = alias;
        Processor::register_op(path, handler, schema);
    };

    reg("jot/rotateX", RotateOp<>::rotateX, "jot/rx");
    reg("jot/rotateY", RotateOp<>::rotateY, "jot/ry");
    reg("jot/rotateZ", RotateOp<>::rotateZ, "jot/rz");
}

} // namespace geo
} // namespace jotcad
