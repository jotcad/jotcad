#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/zag.h"
#include "math/rational_approx.h"
#include "math/interval.h"
#include <vector>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OrbOp : P {
    static constexpr const char* path = "jot/Orb";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Interval diameter, Interval width, Interval height, Interval depth, double zag_val) {
        Geometry res;
        Interval x_range = (width.size() > 0) ? width : diameter;
        Interval y_range = (height.size() > 0) ? height : diameter;
        Interval z_range = (depth.size() > 0) ? depth : diameter;

        double w = x_range.size();
        double h = y_range.size();
        double d = z_range.size();
        double cx = x_range.center();
        double cy = y_range.center();
        double cz = z_range.center();

        double max_dim = std::max({w, h, d});
        int lon_sides = zag(max_dim, zag_val);
        int lat_sides = std::max(3, lon_sides / 2);

        FT w2 = FT(w) / FT(2);
        FT h2 = FT(h) / FT(2);
        FT d2 = FT(d) / FT(2);
        FT f_cx = FT(cx);
        FT f_cy = FT(cy);
        FT f_cz = FT(cz);

        // 1. Generate Vertices (UV Grid)
        std::vector<std::vector<int>> grid(lat_sides + 1, std::vector<int>(lon_sides));

        for (int i = 0; i <= lat_sides; ++i) {
            double phi_turns = (double)i / lat_sides * 0.5 - 0.25; // -0.25 to 0.25 turns
            auto [s_phi, c_phi] = get_approx_sincos(phi_turns);

            for (int j = 0; j < lon_sides; ++j) {
                double theta_turns = (double)j / lon_sides; // 0 to 1 turns
                auto [s_theta, c_theta] = get_approx_sincos(theta_turns);

                grid[i][j] = (int)res.vertices.size();
                res.vertices.push_back({
                    f_cx + c_phi * c_theta * w2,
                    f_cy + c_phi * s_theta * h2,
                    f_cz + s_phi * d2
                });
            }
        }

        // 2. Generate Faces
        for (int i = 0; i < lat_sides; ++i) {
            for (int j = 0; j < lon_sides; ++j) {
                int next_j = (j + 1) % lon_sides;
                
                int v1 = grid[i][j];
                int v2 = grid[i][next_j];
                int v3 = grid[i+1][next_j];
                int v4 = grid[i+1][j];

                if (i == 0) {
                    res.faces.push_back({{{v1, v3, v4}}}); 
                } else if (i == lat_sides - 1) {
                    res.faces.push_back({{{v1, v2, v3}}});
                } else {
                    res.faces.push_back({{{v1, v2, v3, v4}}});
                }
            }
        }

        Shape out = P::make_shape(vfs, res, {{"type", "orb"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"diameter", "width", "height", "depth", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Orb"},
            {"description", "Generates a 3D sphere or ellipsoid solid."},
            {"arguments", {
                {{"name", "diameter"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "width"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "depth"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void orb_init(fs::VFSNode* vfs) {
    Processor::register_op<OrbOp<>, Interval, Interval, Interval, Interval, double>(vfs, "jot/Orb");
}

} // namespace geo
} // namespace jotcad
