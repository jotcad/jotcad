#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct BoxOp : P {
    static constexpr const char* path = "jot/Box";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<double>& size) {
        Geometry res;
        double w = size.size() > 0 ? size[0] : 10.0;
        double h = size.size() > 1 ? size[1] : 10.0;
        double d = size.size() > 2 ? size[2] : 0.0;

        res.vertices.push_back({FT(-w/2), FT(-h/2), FT(-d/2)});
        res.vertices.push_back({FT(w/2), FT(-h/2), FT(-d/2)});
        res.vertices.push_back({FT(w/2), FT(h/2), FT(-d/2)});
        res.vertices.push_back({FT(-w/2), FT(h/2), FT(-d/2)});

        if (d == 0) {
            res.faces.push_back({{{0, 1, 2, 3}}});
        } else {
            res.vertices.push_back({FT(-w/2), FT(-h/2), FT(d/2)});
            res.vertices.push_back({FT(w/2), FT(-h/2), FT(d/2)});
            res.vertices.push_back({FT(w/2), FT(h/2), FT(d/2)});
            res.vertices.push_back({FT(-w/2), FT(h/2), FT(d/2)});
            res.faces.push_back({{{0, 1, 2, 3}}});
            res.faces.push_back({{{4, 5, 6, 7}}});
            res.faces.push_back({{{0, 1, 5, 4}}});
            res.faces.push_back({{{1, 2, 6, 5}}});
            res.faces.push_back({{{2, 3, 7, 6}}});
            res.faces.push_back({{{3, 0, 4, 7}}});
        }
        
        Shape out = P::make_shape(vfs, res, {{"type", "box"}});
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"size"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Box"},
            {"description", "Generates a box (rectangle or cuboid)."},
            {"arguments", {{"size", {{"type", "array"}, {"items", {{"type", "number"}}}, {"default", {10, 10}}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void box_init() {
    Processor::register_op<BoxOp<>, std::vector<double>>("jot/Box");
}

} // namespace geo
} // namespace jotcad
