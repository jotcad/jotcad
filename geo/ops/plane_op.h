#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PlaneOpBase : P {
    static void execute_plane(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Matrix& orientation) {
        Geometry res;
        res.vertices.push_back({FT(0), FT(0), FT(0)});
        
        typename P::json tags = {{"is_plane", true}, {"type", "plane"}};
        Shape out = P::make_shape(vfs, res, tags);
        out.tf = orientation.to_vec();
        
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct XOp : PlaneOpBase<P> {
    static constexpr const char* path = "jot/X";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling) {
        // Rotate local Z (0,0,1) to world X (1,0,0) -> Rotation around Y by 0.25 turns
        PlaneOpBase<P>::execute_plane(vfs, fulfilling, Matrix::rotationY(0.25));
    }
    static std::vector<std::string> argument_keys() { return {}; }
    static typename P::json schema() {
        return { {"path", "jot/X"}, {"description", "Infinite plane on the YZ axis (normal +X)."}, {"outputs", {{"$out", {{"type", "jot:shape"}}}}} };
    }
};

template <typename P = JotVfsProtocol>
struct YOp : PlaneOpBase<P> {
    static constexpr const char* path = "jot/Y";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling) {
        // Rotate local Z (0,0,1) to world Y (0,1,0) -> Rotation around X by -0.25 turns
        PlaneOpBase<P>::execute_plane(vfs, fulfilling, Matrix::rotationX(-0.25));
    }
    static std::vector<std::string> argument_keys() { return {}; }
    static typename P::json schema() {
        return { {"path", "jot/Y"}, {"description", "Infinite plane on the XZ axis (normal +Y)."}, {"outputs", {{"$out", {{"type", "jot:shape"}}}}} };
    }
};

template <typename P = JotVfsProtocol>
struct ZOp : PlaneOpBase<P> {
    static constexpr const char* path = "jot/Z";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling) {
        PlaneOpBase<P>::execute_plane(vfs, fulfilling, Matrix::identity());
    }
    static std::vector<std::string> argument_keys() { return {}; }
    static typename P::json schema() {
        return { {"path", "jot/Z"}, {"description", "Infinite plane on the XY axis (normal +Z)."}, {"outputs", {{"$out", {{"type", "jot:shape"}}}}} };
    }
};

static void plane_init() {
    Processor::register_op<XOp<>>("jot/X");
    Processor::register_op<YOp<>>("jot/Y");
    Processor::register_op<ZOp<>>("jot/Z");
}

} // namespace geo
} // namespace jotcad
