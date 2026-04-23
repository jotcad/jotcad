#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PointOp : P {
    static constexpr const char* path = "jot/Point";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double x, double y, double z) {
        Geometry res;
        res.vertices.push_back({FT(x), FT(y), FT(z)});
        Shape out = P::make_shape(vfs, res, {{"type", "point"}});
        vfs->write<Shape>(fulfilling, out, "$out");
    }
    static std::vector<std::string> argument_keys() { return {"x", "y", "z"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Point"},
            {"description", "Generates a single point."},
            {"arguments", {
                {"x", {{"type", "number"}, {"default", 0.0}}},
                {"y", {{"type", "number"}, {"default", 0.0}}},
                {"z", {{"type", "number"}, {"default", 0.0}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct PointsOp : P {
    static constexpr const char* path = "jot/Points";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<std::vector<double>>& points) {
        Geometry res;
        for (const auto& p : points) {
            if (p.size() >= 3) {
                res.vertices.push_back({FT(p[0]), FT(p[1]), FT(p[2])});
            } else if (p.size() == 2) {
                res.vertices.push_back({FT(p[0]), FT(p[1]), FT(0.0)});
            }
        }
        Shape out = P::make_shape(vfs, res, {{"type", "points"}});
        vfs->write<Shape>(fulfilling, out, "$out");
    }
    static std::vector<std::string> argument_keys() { return {"points"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Points"},
            {"description", "Generates a point cloud from a list of coordinates."},
            {"arguments", {
                {"points", {{"type", "array"}, {"items", {{"type", "array"}, {"items", {{"type", "number"}}}}}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void points_init() {
    Processor::register_op<PointOp<>, double, double, double>("jot/Point");
    Processor::register_op<PointsOp<>, std::vector<std::vector<double>>>("jot/Points");
}

} // namespace geo
} // namespace jotcad
