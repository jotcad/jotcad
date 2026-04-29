#pragma once
#include "protocols.h"
#include "processor.h"
#include <cmath>

namespace jotcad {
namespace geo {

// Exact rational approximations for hexagon coordinates to ensure stability in boolean ops.
// cos(60) = 1/2
// sin(60) = sqrt(3)/2 approx 0.86602540378
static const FT HEX_COS_60 = FT(1) / FT(2);
static const FT HEX_SIN_60 = FT(86602540378LL) / FT(100000000000LL);

static std::vector<Vertex> get_hexagon_vertices(FT r) {
    return {
        {r, FT(0), FT(0)},                       // 0 deg
        {r * HEX_COS_60, r * HEX_SIN_60, FT(0)}, // 60 deg
        {-r * HEX_COS_60, r * HEX_SIN_60, FT(0)}, // 120 deg
        {-r, FT(0), FT(0)},                      // 180 deg
        {-r * HEX_COS_60, -r * HEX_SIN_60, FT(0)}, // 240 deg
        {r * HEX_COS_60, -r * HEX_SIN_60, FT(0)}  // 300 deg
    };
}

template <typename P = JotVfsProtocol>
struct HexagonFullOp : P {
    static constexpr const char* path = "jot/Hexagon/full";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        FT r = FT(diameter) / FT(2);
        res.vertices = get_hexagon_vertices(r);
        res.faces.push_back({{{0, 1, 2, 3, 4, 5}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/full"},
            {"description", "Generates a full hexagon."},
            {"arguments", {{{"name", "diameter"}, {"type", "jot:number"}, {"default", 30.0}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct HexagonCapOp : P {
    static constexpr const char* path = "jot/Hexagon/cap";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        FT r = FT(diameter) / FT(2);
        auto hex = get_hexagon_vertices(r);
        res.vertices = {{FT(0), FT(0), FT(0)}, hex[0], hex[1]};
        res.faces.push_back({{{0, 1, 2}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "type"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/cap"},
            {"description", "Generates a triangular sector (cap) of a hexagon."},
            {"arguments", {
                {{"name", "diameter"}, {"type", "jot:number"}, {"default", 30.0}},
                {{"name", "type"}, {"type", "jot:string"}, {"const", "cap"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct HexagonMiddleOp : P {
    static constexpr const char* path = "jot/Hexagon/middle";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        FT r = FT(diameter) / FT(2);
        auto hex = get_hexagon_vertices(r);
        res.vertices = {{FT(0), FT(0), FT(0)}, hex[1], hex[2], hex[3]};
        res.faces.push_back({{{0, 1, 2, 3}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "type"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/middle"},
            {"arguments", {
                {{"name", "diameter"}, {"type", "jot:number"}, {"default", 30.0}},
                {{"name", "type"}, {"type", "jot:string"}, {"const", "middle"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct HexagonSectorOp : P {
    static constexpr const char* path = "jot/Hexagon/sector";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        FT r = FT(diameter) / FT(2);
        auto hex = get_hexagon_vertices(r);
        res.vertices = {{FT(0), FT(0), FT(0)}, hex[0], hex[1], hex[2]};
        res.faces.push_back({{{0, 1, 2, 3}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "type"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/sector"},
            {"arguments", {
                {{"name", "diameter"}, {"type", "jot:number"}, {"default", 30.0}},
                {{"name", "type"}, {"type", "jot:string"}, {"const", "sector"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct HexagonHalfOp : P {
    static constexpr const char* path = "jot/Hexagon/half";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        FT r = FT(diameter) / FT(2);
        auto hex = get_hexagon_vertices(r);
        res.vertices = {{FT(0), FT(0), FT(0)}, hex[0], hex[1], hex[2], hex[3]};
        res.faces.push_back({{{0, 1, 2, 3, 4}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "type"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/half"},
            {"arguments", {
                {{"name", "diameter"}, {"type", "jot:number"}, {"default", 30.0}},
                {{"name", "type"}, {"type", "jot:string"}, {"const", "half"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void hexagon_init(fs::VFSNode* vfs) {
    Processor::register_op<HexagonFullOp<>, double>(vfs, "jot/Hexagon/full");
    Processor::register_op<HexagonCapOp<>, double>(vfs, "jot/Hexagon/cap");
    Processor::register_op<HexagonMiddleOp<>, double>(vfs, "jot/Hexagon/middle");
    Processor::register_op<HexagonSectorOp<>, double>(vfs, "jot/Hexagon/sector");
    Processor::register_op<HexagonHalfOp<>, double>(vfs, "jot/Hexagon/half");
}

} // namespace geo
} // namespace jotcad
