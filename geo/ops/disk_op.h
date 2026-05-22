#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/zag.h"
#include "math/rational_approx.h"
#include "math/interval.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct DiskOp : P {
    static constexpr const char* path = "jot/Disk";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Interval diameter, Interval width, Interval height, double start, double end, double zag_val) {
        Geometry res;
        
        // Interval logic: width/height override diameter if they are non-zero size
        Interval x_range = (width.size() > 0) ? width : diameter;
        Interval y_range = (height.size() > 0) ? height : diameter;

        double w = x_range.size();
        double h = y_range.size();
        double cx = x_range.center();
        double cy = y_range.center();

        int total_sides = zag(std::max(w, h), zag_val);
        double span = end - start;
        int sides = (int)std::max(3.0, std::ceil(std::abs(span) * total_sides));
        
        FT w2 = FT(w) / FT(2);
        FT h2 = FT(h) / FT(2);
        FT f_cx = FT(cx);
        FT f_cy = FT(cy);

        bool is_full = (std::abs(span - 1.0) < 1e-9) || (std::abs(span + 1.0) < 1e-9);

        if (!is_full) {
            res.vertices.push_back({f_cx, f_cy, FT(0)});
        }

        for (int i = 0; i <= sides; ++i) {
            double turns = start + (span * i / sides);
            if (is_full && i == sides) break; 

            auto [s, c] = get_approx_sincos(turns);
            res.vertices.push_back({f_cx + c * w2, f_cy + s * h2, FT(0)});
        }

        Geometry::Face face;
        std::vector<int> loop;
        for (int i = 0; i < (int)res.vertices.size(); ++i) loop.push_back(i);
        face.loops.push_back(loop);
        res.faces.push_back(face);

        Shape out = P::make_shape(vfs, res, {{"type", "surface"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"diameter", "width", "height", "start", "end", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Disk"},
            {"description", "Generates a circular or elliptical disk/sector."},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "diameter"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "width"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "start"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "end"}, {"type", "jot:number"}, {"default", 1.0}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void disk_init(fs::VFSNode* vfs) {
    Processor::register_op<DiskOp<>, Interval, Interval, Interval, double, double, double>(vfs, "jot/Disk");
}

} // namespace geo
} // namespace jotcad
