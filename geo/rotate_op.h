#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct RotateOp : P {
    static constexpr const char* path = "jot/rotate";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& in, const std::vector<double>& turns, int axis, Shape& out) {
        std::vector<Shape> results;
        for (const auto& src : in) {
            for (double t : turns) {
                Matrix r;
                if (axis == 0) r = Matrix::rotationX(t);
                else if (axis == 1) r = Matrix::rotationY(t);
                else r = Matrix::rotationZ(t);
                
                Matrix current = Matrix::from_vec(src.tf);
                Shape res = src;
                res.tf = (r * current).to_vec();
                results.push_back(res);
            }
        }

        if (results.size() == 1) out = results[0];
        else {
            out.geometry = std::nullopt;
            out.components = results;
            out.add_tag("type", "group");
        }
    }

    static std::vector<uint8_t> rotate_logic(jotcad::fs::VFSNode* vfs, const nlohmann::json& params, const std::vector<std::string>& stack, int axis) {
        auto in = Processor::decode<std::vector<Shape>>(vfs, "$in", params, {}, stack);
        auto turns = Processor::decode<std::vector<double>>(vfs, "turns", params, {}, stack);
        
        Shape out;
        execute(vfs, in, turns, axis, out);
        return P::write_shape_obj(out);
    }

    static std::vector<uint8_t> rotateX(jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        return rotate_logic(vfs, params, stack, 0);
    }
    static std::vector<uint8_t> rotateY(jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        return rotate_logic(vfs, params, stack, 1);
    }
    static std::vector<uint8_t> rotateZ(jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        return rotate_logic(vfs, params, stack, 2);
    }
};

static void rotate_init() {
    auto get_schema = [](const std::string& path, const std::string& alias) {
        return nlohmann::json{
            {"path", path},
            {"aliases", {alias}},
            {"arguments", {
                {"$in", {{"type", "jot:shapes"}}},
                {"turns", {{"type", "jot:numbers"}, {"default", 0.0}}}
            }},
            {"inputs", {{"$in", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    };

    Processor::register_op("jot/rotateX", RotateOp<>::rotateX, get_schema("jot/rotateX", "jot/rx"));
    Processor::register_op("jot/rotateY", RotateOp<>::rotateY, get_schema("jot/rotateY", "jot/ry"));
    Processor::register_op("jot/rotateZ", RotateOp<>::rotateZ, get_schema("jot/rotateZ", "jot/rz"));
}

} // namespace geo
} // namespace jotcad
