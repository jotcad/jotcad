#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/zag.h"
#include "math/rational_approx.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ArcOp : P {
    static constexpr const char* path = "jot/Arc";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double diameter, double width, double height, double start, double end, double zag_val) {
        Geometry res;
        double w = (width > 0) ? width : diameter;
        double h = (height > 0) ? height : diameter;

        int total_sides = zag(std::max(w, h), zag_val);
        
        double span = end - start;
        int sides = (int)std::max(1.0, std::ceil(std::abs(span) * total_sides));
        
        FT w2 = FT(w) / FT(2);
        FT h2 = FT(h) / FT(2);

        bool is_full = (std::abs(span - 1.0) < 1e-9) || (std::abs(span + 1.0) < 1e-9);

        for (int i = 0; i <= sides; ++i) {
            double turns = start + (span * i / sides);
            if (is_full && i == sides) break; 

            auto [s, c] = get_approx_sincos(turns);
            res.vertices.push_back({c * w2, s * h2, FT(0)});
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
                {{"name", "diameter"}, {"type", "jot:number"}, {"default", 10.0}},
                {{"name", "width"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "height"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "start"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "end"}, {"type", "jot:number"}, {"default", 1.0}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void arc_init(fs::VFSNode* vfs) {
    Processor::register_op<ArcOp<>, double, double, double, double, double, double>(vfs, "jot/Arc");
}

} // namespace geo
} // namespace jotcad
