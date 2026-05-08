#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/rational_approx.h"
#include <cmath>

namespace jotcad {
namespace geo {

// Exact rational approximations for hexagon coordinates.
// sin(60) = sqrt(3)/2 approx 0.86602540378
static const FT HEX_SIN_60 = FT(86602540378LL) / FT(100000000000LL);

static std::vector<Vertex> get_hexagon_vertices(FT r, double turns) {
    std::vector<Vertex> res;
    for (int i = 0; i < 6; ++i) {
        auto [s, c] = get_approx_sincos(turns + (double)i / 6.0);
        res.push_back({c * r, s * r, FT(0)});
    }
    return res;
}

template <typename P = JotVfsProtocol>
struct HexagonOp : P {
    static void execute_impl(fs::VFSNode* vfs, const fs::Selector& fulfilling, FT r, double turns) {
        Geometry res;
        res.vertices = get_hexagon_vertices(r, turns);
        res.faces.push_back({{{0, 1, 2, 3, 4, 5}}});
        Shape out = P::make_shape(vfs, res, {{"type", "surface"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    // Variant: Radius (Center-to-Corner)
    struct ByRadius {
        static constexpr const char* path = "jot/Hexagon/radius";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double radius, double turns) {
            execute_impl(vfs, fulfilling, FT(radius), turns);
        }
        static std::vector<std::string> argument_keys() { return {"radius", "turns"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates a hexagon by radius (center-to-corner)."},
                {"arguments", {
                    {{"name", "radius"}, {"type", "jot:number"}, {"default", 5.0}},
                    {{"name", "turns"}, {"type", "jot:number"}, {"default", 0.0}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    // Variant: Diameter (Corner-to-Corner)
    struct ByDiameter {
        static constexpr const char* path = "jot/Hexagon/diameter";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter, double turns) {
            execute_impl(vfs, fulfilling, FT(diameter) / FT(2), turns);
        }
        static std::vector<std::string> argument_keys() { return {"diameter", "turns"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates a hexagon by diameter (corner-to-corner)."},
                {"arguments", {
                    {{"name", "diameter"}, {"type", "jot:number"}, {"default", 10.0}},
                    {{"name", "turns"}, {"type", "jot:number"}, {"default", 0.0}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    // Variant: Apothem (Center-to-Flat)
    struct ByApothem {
        static constexpr const char* path = "jot/Hexagon/apothem";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double apothem, double turns) {
            // R = a / (sqrt(3)/2)
            execute_impl(vfs, fulfilling, FT(apothem) / HEX_SIN_60, turns);
        }
        static std::vector<std::string> argument_keys() { return {"apothem", "turns"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates a hexagon by apothem (center-to-flat distance)."},
                {"arguments", {
                    {{"name", "apothem"}, {"type", "jot:number"}, {"default", 4.330127}},
                    {{"name", "turns"}, {"type", "jot:number"}, {"default", 0.0}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    // Variant: Edge-to-Edge (Flat-to-Flat)
    struct ByEdgeToEdge {
        static constexpr const char* path = "jot/Hexagon/edgeToEdge";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double edgeToEdge, double turns) {
            // R = e2e / sqrt(3)
            execute_impl(vfs, fulfilling, FT(edgeToEdge) / (FT(2) * HEX_SIN_60), turns);
        }
        static std::vector<std::string> argument_keys() { return {"edgeToEdge", "turns"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates a hexagon by edge-to-edge distance."},
                {"arguments", {
                    {{"name", "edgeToEdge"}, {"type", "jot:number"}, {"default", 8.660254}},
                    {{"name", "turns"}, {"type", "jot:number"}, {"default", 0.0}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    // Variant: Corner-to-Corner (Diagonal)
    struct ByCornerToCorner {
        static constexpr const char* path = "jot/Hexagon/cornerToCorner";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double corner_to_corner, double turns) {
            execute_impl(vfs, fulfilling, FT(corner_to_corner) / FT(2), turns);
        }
        static std::vector<std::string> argument_keys() { return {"corner_to_corner", "turns"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates a hexagon by corner-to-corner distance."},
                {"arguments", {
                    {{"name", "corner_to_corner"}, {"type", "jot:number"}, {"default", 10.0}},
                    {{"name", "turns"}, {"type", "jot:number"}, {"default", 0.0}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    // Variant: Edge-Length (Side)
    struct ByEdgeLength {
        static constexpr const char* path = "jot/Hexagon/edgeLength";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double edgeLength, double turns) {
            // R = Side
            execute_impl(vfs, fulfilling, FT(edgeLength), turns);
        }
        static std::vector<std::string> argument_keys() { return {"edgeLength", "turns"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates a hexagon by edge length (side)."},
                {"arguments", {
                    {{"name", "edgeLength"}, {"type", "jot:number"}, {"default", 5.0}},
                    {{"name", "turns"}, {"type", "jot:number"}, {"default", 0.0}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };
};

static void hexagon_init(fs::VFSNode* vfs) {
    Processor::register_op<HexagonOp<>::ByRadius, double, double>(vfs, "jot/Hexagon/radius");
    Processor::register_op<HexagonOp<>::ByDiameter, double, double>(vfs, "jot/Hexagon/diameter");
    Processor::register_op<HexagonOp<>::ByApothem, double, double>(vfs, "jot/Hexagon/apothem");
    Processor::register_op<HexagonOp<>::ByEdgeToEdge, double, double>(vfs, "jot/Hexagon/edgeToEdge");
    Processor::register_op<HexagonOp<>::ByCornerToCorner, double, double>(vfs, "jot/Hexagon/cornerToCorner");
    Processor::register_op<HexagonOp<>::ByEdgeLength, double, double>(vfs, "jot/Hexagon/edgeLength");
}

} // namespace geo
} // namespace jotcad
