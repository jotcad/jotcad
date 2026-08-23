#pragma once
#include "protocols.h"
#include "processor.h"
#include "algorithms/triangle.h"
#include <cmath>

namespace jotcad {
namespace geo {

// Exact rational approximations for triangle coordinates.
// sqrt(3)/2 approx 0.86602540378
static const FT TRI_SQRT3_2 = FT(86602540378LL) / FT(100000000000LL);

template <typename P = JotVfsProtocol>
struct TriangleOp : P {
    static void execute_equilateral(fs::VFSNode* vfs, const fs::Selector& fulfilling, FT s) {
        Geometry res;
        FT h = s * TRI_SQRT3_2;

        res.vertices.push_back({-s/FT(2), FT(0), FT(0)});
        res.vertices.push_back({s/FT(2), FT(0), FT(0)});
        res.vertices.push_back({FT(0), h, FT(0)});
        res.faces.push_back({{{0, 1, 2}}});

        Shape out = P::make_shape(vfs, res, {{"type", "surface"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    struct Standard {
        static constexpr const char* path = "jot/Triangle";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                            std::optional<double> a, 
                            std::optional<double> b, 
                            std::optional<double> c, 
                            std::optional<double> x, 
                            std::optional<double> y, 
                            std::optional<bool> center) {
            Geometry res;
            makeTriangle(res, a, b, c, x, y, center);
            Shape out = P::make_shape(vfs, res, {{"type", "surface"}});
            vfs->write(fulfilling.with_output("$out"), out);
        }
        static std::vector<std::string> argument_keys() { return {"a", "b", "c", "x", "y", "center"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates a 2D triangle from 3 side lengths (a, b, c) or 2 orthogonal legs (x, y)."},
                {"inputs", nlohmann::json::object()},
                {"arguments", json::array({
                    {{"name", "a"}, {"type", "jot:number"}, {"optional", true}},
                    {{"name", "b"}, {"type", "jot:number"}, {"optional", true}},
                    {{"name", "c"}, {"type", "jot:number"}, {"optional", true}},
                    {{"name", "x"}, {"type", "jot:number"}, {"optional", true}},
                    {{"name", "y"}, {"type", "jot:number"}, {"optional", true}},
                    {{"name", "center"}, {"type", "jot:boolean"}, {"optional", true}}
                })},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    struct Equilateral {
        static constexpr const char* path = "jot/Triangle/equilateral";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double size) {
            execute_equilateral(vfs, fulfilling, FT(size));
        }
        static std::vector<std::string> argument_keys() { return {"size"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates an equilateral triangle by side length."},
                {"inputs", nlohmann::json::object()},
                {"arguments", json::array({
                    {{"name", "size"}, {"type", "jot:number"}, {"default", 10.0}}
                })},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    struct ByHeight {
        static constexpr const char* path = "jot/Triangle/height";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double height) {
            // s = h / (sqrt(3)/2)
            execute_equilateral(vfs, fulfilling, FT(height) / TRI_SQRT3_2);
        }
        static std::vector<std::string> argument_keys() { return {"height"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates an equilateral triangle by height."},
                {"inputs", nlohmann::json::object()},
                {"arguments", json::array({
                    {{"name", "height"}, {"type", "jot:number"}, {"default", 8.660254}}
                })},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };
};

static void triangle_init(fs::VFSNode* vfs) {
    Processor::register_op<TriangleOp<>::Standard, 
                           std::optional<double>, 
                           std::optional<double>, 
                           std::optional<double>, 
                           std::optional<double>, 
                           std::optional<double>, 
                           std::optional<bool>>(vfs, "jot/Triangle");
    Processor::register_op<TriangleOp<>::Equilateral, double>(vfs, "jot/Triangle/equilateral");
    Processor::register_op<TriangleOp<>::ByHeight, double>(vfs, "jot/Triangle/height");
}

} // namespace geo
} // namespace jotcad
