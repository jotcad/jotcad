#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct MoveOp : P {
    static constexpr const char* path = "jot/move";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double x, double y, double z) {
        Shape out = in;
        Matrix t = Matrix::translate(x, y, z);
        out.tf = t * in.tf;
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "x", "y", "z"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/move"},
            {"description", "Translates the input shape by the given x, y, z offsets."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"description", "The shape to move."}, {"affiliate", "$out"}},
                {{"name", "x"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Offset along the X-axis."}},
                {{"name", "y"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Offset along the Y-axis."}},
                {{"name", "z"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Offset along the Z-axis."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The moved shape."}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct MoveXOp : P {
    static constexpr const char* path = "jot/moveX";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double x) {
        MoveOp<P>::execute(vfs, fulfilling, in, x, 0, 0);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "x"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/moveX"},
            {"description", "Translates the input shape along the X-axis."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "x"}, {"type", "jot:number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct MoveYOp : P {
    static constexpr const char* path = "jot/moveY";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double y) {
        MoveOp<P>::execute(vfs, fulfilling, in, 0, y, 0);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "y"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/moveY"},
            {"description", "Translates the input shape along the Y-axis."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "y"}, {"type", "jot:number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct MoveZOp : P {
    static constexpr const char* path = "jot/moveZ";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double z) {
        MoveOp<P>::execute(vfs, fulfilling, in, 0, 0, z);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "z"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/moveZ"},
            {"description", "Translates the input shape along the Z-axis."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "z"}, {"type", "jot:number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void move_init(fs::VFSNode* vfs) {
    Processor::register_op<MoveOp<>, Shape, double, double, double>(vfs, "jot/move");
    Processor::register_op<MoveOp<>, Shape, double, double, double>(vfs, "jot/m"); 
    Processor::register_op<MoveXOp<>, Shape, double>(vfs, "jot/moveX");
    Processor::register_op<MoveXOp<>, Shape, double>(vfs, "jot/mx");
    Processor::register_op<MoveYOp<>, Shape, double>(vfs, "jot/moveY");
    Processor::register_op<MoveYOp<>, Shape, double>(vfs, "jot/my");
    Processor::register_op<MoveZOp<>, Shape, double>(vfs, "jot/moveZ");
    Processor::register_op<MoveZOp<>, Shape, double>(vfs, "jot/mz");
}

} // namespace geo
} // namespace jotcad
