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
                double rad = t * 2.0 * M_PI;
                double c = std::cos(rad);
                double s = std::sin(rad);
                
                std::vector<double> r = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
                if (axis == 0) { // X
                    r[5] = c; r[6] = -s; r[9] = s; r[10] = c;
                } else if (axis == 1) { // Y
                    r[0] = c; r[2] = s; r[8] = -s; r[10] = c;
                } else { // Z
                    r[0] = c; r[1] = -s; r[4] = s; r[5] = c;
                }

                // Compose: out.tf = src.tf * r
                std::vector<double> next_tf(16, 0.0);
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        for (int k = 0; k < 4; ++k) {
                            next_tf[i * 4 + j] += src.tf[i * 4 + k] * r[k * 4 + j];
                        }
                    }
                }
                
                Shape res = src;
                res.tf = next_tf;
                results.push_back(res);
            }
        }

        if (results.size() == 1) out = results[0];
        else {
            out.geometry.path = "";
            out.components = results;
            out.add_tag("type", "group");
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
