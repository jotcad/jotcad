#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SegmentOpBase : P {
    static void collect_points(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<EK::Point_3>& pts) {
        Matrix current_tf = parent_tf * Matrix::from_vec(s.tf);
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            for (const auto& v : geo.vertices) {
                pts.push_back(current_tf.transform(EK::Point_3(v.x, v.y, v.z)));
            }
        }
        for (const auto& child : s.components) {
            collect_points(vfs, child, current_tf, pts);
        }
    }

    static void execute_impl(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes, bool close_loop) {
        std::vector<EK::Point_3> all_pts;
        for (const auto& s : shapes) {
            collect_points(vfs, s, Matrix::identity(), all_pts);
        }

        if (all_pts.size() < 2) {
            vfs->write(fulfilling.with_output("$out"), P::make_shape(vfs, Geometry(), {{"type", "segments"}}));
            return;
        }

        Geometry res;
        for (const auto& p : all_pts) {
            res.vertices.push_back({p.x(), p.y(), p.z()});
        }

        for (size_t i = 0; i < all_pts.size() - 1; ++i) {
            res.segments.push_back({(int)i, (int)i + 1});
        }

        if (close_loop && all_pts.size() > 2) {
            res.segments.push_back({(int)all_pts.size() - 1, 0});
        }

        Shape out = P::make_shape(vfs, res, {{"type", "segments"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct LinkOp : SegmentOpBase<P> {
    static constexpr const char* path = "jot/link";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes) {
        SegmentOpBase<P>::execute_impl(vfs, fulfilling, shapes, false);
    }
    static std::vector<std::string> argument_keys() { return {"shapes"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/link"},
            {"description", "Creates an open chain of segments connecting points."},
            {"arguments", {{"shapes", {{"type", "jot:shapes"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct LoopOp : SegmentOpBase<P> {
    static constexpr const char* path = "jot/loop";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes) {
        SegmentOpBase<P>::execute_impl(vfs, fulfilling, shapes, true);
    }
    static std::vector<std::string> argument_keys() { return {"shapes"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/loop"},
            {"description", "Creates a closed loop of segments connecting points."},
            {"arguments", {{"shapes", {{"type", "jot:shapes"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void segment_init() {
    Processor::register_op<LinkOp<>, std::vector<Shape>>("jot/link");
    Processor::register_op<LoopOp<>, std::vector<Shape>>("jot/loop");
}

} // namespace geo
} // namespace jotcad
