#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/zag.h"
#include "math/rational_approx.h"
#include "math/interval.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ArcOp : P {
    static constexpr const char* path = "jot/Arc";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Interval diameter, Interval width, Interval height, double start, double end, double zag_val) {
        Geometry res;
        Interval x_range = (width.size() > 0) ? width : diameter;
        Interval y_range = (height.size() > 0) ? height : diameter;

        double w = x_range.size();
        double h = y_range.size();
        double cx = x_range.center();
        double cy = y_range.center();

        int total_sides = zag(std::max(w, h), zag_val);
        double span = end - start;
        int sides = (int)std::max(1.0, std::ceil(std::abs(span) * total_sides));
        
        FT w2 = FT(w) / FT(2);
        FT h2 = FT(h) / FT(2);
        FT f_cx = FT(cx);
        FT f_cy = FT(cy);

        bool is_full = (std::abs(span - 1.0) < 1e-9) || (std::abs(span + 1.0) < 1e-9);

        for (int i = 0; i <= sides; ++i) {
            double turns = start + (span * i / sides);
            if (is_full && i == sides) break; 

            auto [s, c] = get_approx_sincos(turns);
            res.vertices.push_back({f_cx + c * w2, f_cy + s * h2, FT(0)});
        }

        for (int i = 0; i < (int)res.vertices.size(); ++i) {
            int next = (i + 1) % res.vertices.size();
            if (!is_full && i == (int)res.vertices.size() - 1) break;
            res.segments.push_back({i, next});
        }

        Shape out = P::make_shape(vfs, res, {{"type", "arc"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"diameter", "width", "height", "start", "end", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Arc"},
            {"description", "Generates a circular or elliptical arc/boundary."},
            {"arguments", {
                {{"name", "diameter"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "width"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "start"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "end"}, {"type", "jot:number"}, {"default", 1.0}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void arc_init(fs::VFSNode* vfs) {
    Processor::register_op<ArcOp<>, Interval, Interval, Interval, double, double, double>(vfs, "jot/Arc");
}

} // namespace geo
} // namespace jotcad
