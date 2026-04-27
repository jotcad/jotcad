#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct RotateOp : P {
    static constexpr const char* path = "jot/rotate";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double angle) {
        Shape out = in;
        double turns = angle / 360.0;
        Matrix r = Matrix::rotationZ(turns);
        out.tf = (r * Matrix::from_vec(in.tf)).to_vec();
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "angle"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/rotate"},
            {"description", "Rotates the input shape around the Z-axis."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"description", "The shape to rotate."}, {"affiliate", "$out"}},
                {{"name", "angle"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "The rotation angle in degrees."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The rotated shape."}}}}}
        };
    }
};

static void rotate_init(fs::VFSNode* vfs) {
    Processor::register_op<RotateOp<>, Shape, double>(vfs, "jot/rotate");
}

} // namespace geo
} // namespace jotcad
