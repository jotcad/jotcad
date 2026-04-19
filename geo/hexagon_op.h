#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include <cmath>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct HexagonFullOp : P {
    static constexpr const char* path = "jot/Hexagon/full";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        double r = diameter / 2.0;
        for (int i = 0; i < 6; ++i) {
            double angle = (i * 60.0) * M_PI / 180.0;
            res.vertices.push_back({FT(r * std::cos(angle)), FT(r * std::sin(angle)), FT(0)});
        }
        res.faces.push_back({{{0, 1, 2, 3, 4, 5}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/full"},
            {"description", "Generates a full hexagon."},
            {"arguments", {{"diameter", {{"type", "number"}, {"default", 30.0}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct HexagonCapOp : P {
    static constexpr const char* path = "jot/Hexagon/cap";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        double r = diameter / 2.0;
        res.vertices.push_back({FT(0), FT(0), FT(0)});
        for (int i = 0; i < 2; ++i) {
            double angle = (i * 60.0) * M_PI / 180.0;
            res.vertices.push_back({FT(r * std::cos(angle)), FT(r * std::sin(angle)), FT(0)});
        }
        res.faces.push_back({{{0, 1, 2}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "type"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/cap"},
            {"description", "Generates a triangular sector (cap) of a hexagon."},
            {"arguments", {
                {"diameter", {{"type", "number"}, {"default", 30.0}}},
                {"type", {{"type", "string"}, {"const", "cap"}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct HexagonMiddleOp : P {
    static constexpr const char* path = "jot/Hexagon/middle";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        double r = diameter / 2.0;
        res.vertices.push_back({FT(0), FT(0), FT(0)});
        for (int i = 1; i < 4; ++i) {
            double angle = (i * 60.0) * M_PI / 180.0;
            res.vertices.push_back({FT(r * std::cos(angle)), FT(r * std::sin(angle)), FT(0)});
        }
        res.faces.push_back({{{0, 1, 2, 3}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "type"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/middle"},
            {"arguments", {
                {"diameter", {{"type", "number"}, {"default", 30.0}}},
                {"type", {{"type", "string"}, {"const", "middle"}}}
            }}
        };
    }
};

template <typename P = JotVfsProtocol>
struct HexagonSectorOp : P {
    static constexpr const char* path = "jot/Hexagon/sector";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        double r = diameter / 2.0;
        res.vertices.push_back({FT(0), FT(0), FT(0)});
        for (int i = 0; i < 3; ++i) {
            double angle = (i * 60.0) * M_PI / 180.0;
            res.vertices.push_back({FT(r * std::cos(angle)), FT(r * std::sin(angle)), FT(0)});
        }
        res.faces.push_back({{{0, 1, 2, 3}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "type"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/sector"},
            {"arguments", {
                {"diameter", {{"type", "number"}, {"default", 30.0}}},
                {"type", {{"type", "string"}, {"const", "sector"}}}
            }}
        };
    }
};

template <typename P = JotVfsProtocol>
struct HexagonHalfOp : P {
    static constexpr const char* path = "jot/Hexagon/half";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter) {
        Geometry res;
        double r = diameter / 2.0;
        res.vertices.push_back({FT(0), FT(0), FT(0)});
        for (int i = 0; i < 4; ++i) {
            double angle = (i * 60.0) * M_PI / 180.0;
            res.vertices.push_back({FT(r * std::cos(angle)), FT(r * std::sin(angle)), FT(0)});
        }
        res.faces.push_back({{{0, 1, 2, 3, 4}}});
        Shape out = P::make_shape(vfs, res, {{"type", "hexagon"}});
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "type"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Hexagon/half"},
            {"arguments", {
                {"diameter", {{"type", "number"}, {"default", 30.0}}},
                {"type", {{"type", "string"}, {"const", "half"}}}
            }}
        };
    }
};

static void hexagon_init() {
    Processor::register_op<HexagonFullOp<>, double>("jot/Hexagon/full");
    Processor::register_op<HexagonCapOp<>, double>("jot/Hexagon/cap");
    Processor::register_op<HexagonMiddleOp<>, double>("jot/Hexagon/middle");
    Processor::register_op<HexagonSectorOp<>, double>("jot/Hexagon/sector");
    Processor::register_op<HexagonHalfOp<>, double>("jot/Hexagon/half");
}

} // namespace geo
} // namespace jotcad
