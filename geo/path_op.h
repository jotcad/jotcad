#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct LinkOp : P {
    static constexpr const char* path = "jot/link";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& a, const Shape& b) {
        Shape out;
        out.geometry = std::nullopt;
        out.components = {a, b};
        out.add_tag("type", "link");
        vfs->write<Shape>(fulfilling, out, "$out");
    }
    static std::vector<std::string> argument_keys() { return {"$a", "$b"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/link"},
            {"description", "Creates a semantic link between two shapes, establishing a topological connection."},
            {"arguments", {
                {"$a", {{"type", "jot:shape"}, {"description", "The first shape to be linked."}}},
                {"$b", {{"type", "jot:shape"}, {"description", "The second shape to be linked."}}}
            }},
            {"inputs", {
                {"$a", {{"type", "shape"}, {"description", "The first shape."}}}, 
                {"$b", {{"type", "shape"}, {"description", "The second shape."}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "A group containing both linked shapes."}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct LoopOp : P {
    static constexpr const char* path = "jot/loop";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        std::vector<Shape> all_components;
        if (in.geometry.has_value()) {
            Shape first = in;
            first.components.clear();
            all_components.push_back(first);
        }
        for (const auto& c : in.components) {
            all_components.push_back(c);
        }

        if (all_components.size() < 2) {
            vfs->write<Shape>(fulfilling, in, "$out");
            return;
        }

        Geometry res;
        for (const auto& c : all_components) {
            Matrix m = Matrix::from_vec(c.tf);
            Point_3 p = m.t.transform(Point_3(0, 0, 0));
            res.vertices.push_back({p.x(), p.y(), p.z()});
        }

        for (size_t i = 0; i < res.vertices.size(); ++i) {
            int v1 = (int)i;
            int v2 = (int)((i + 1) % res.vertices.size());
            res.segments.push_back({v1, v2});
        }

        Shape out;
        out.geometry = vfs->write_anonymous<Geometry>(res);
        out.components = all_components;
        out.add_tag("type", "loop");
        vfs->write<Shape>(fulfilling, out, "$out");
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/loop"},
            {"description", "Closes a sequence of connected segments into a topological loop."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"description", "The shape (usually a group) to form into a loop."}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}, {"description", "The input shape."}}}}},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "The resulting loop shape."}}}}}
        };
    }
};

static void path_init() {
    Processor::register_op<LinkOp<>, Shape, Shape>("jot/link");
    Processor::register_op<LoopOp<>, Shape>("jot/loop");
}

} // namespace geo
} // namespace jotcad
