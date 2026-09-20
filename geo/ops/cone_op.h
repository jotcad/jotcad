#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/zag.h"
#include "math/rational_approx.h"
#include "math/interval.h"
#include "boolean/engine.h"
#include "fix/repair.h"
#include <CGAL/simplest_rational_in_interval.h>
#include <cmath>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ConeOpBase : P {
    static void execute_cone(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                             FT f_xmin, FT f_xmax, Interval y_extent, Interval z_extent, 
                             double zag_val) {
        Geometry res;
        
        double cy = y_extent.center();
        double cz = z_extent.center();
        double w = y_extent.size();
        double h = z_extent.size();
        
        FT f_cy = CGAL::simplest_rational_in_interval<FT>(cy - 1e-5, cy + 1e-5);
        FT f_cz = CGAL::simplest_rational_in_interval<FT>(cz - 1e-5, cz + 1e-5);
        FT w2 = CGAL::simplest_rational_in_interval<FT>(w - 1e-5, w + 1e-5) / FT(2);
        FT h2 = CGAL::simplest_rational_in_interval<FT>(h - 1e-5, h + 1e-5) / FT(2);

        int sides = zag(std::max(w, h), zag_val);
        if (sides < 3) sides = 3;

        // 1. Tip vertex
        int tip_idx = (int)res.vertices.size();
        res.vertices.push_back({f_xmax, f_cy, f_cz});

        // 2. Base perimeter vertices
        int perimeter_start = (int)res.vertices.size();
        for (int i = 0; i < sides; ++i) {
            double turns = (double)i / sides;
            auto [s, c] = get_approx_sincos(turns);
            res.vertices.push_back({f_xmin, f_cy + c * w2, f_cz + s * h2});
        }

        // 3. Side faces (Triangles connecting perimeter to tip)
        for (int i = 0; i < sides; ++i) {
            int v1 = perimeter_start + i;
            int v2 = perimeter_start + (i + 1) % sides;
            // Order (v1, v2, tip) for outward normal
            res.faces.push_back({{{v1, v2, tip_idx}}});
        }

        // 4. Base face (Fan connecting perimeter points)
        std::vector<int> base_loop;
        for (int i = sides - 1; i >= 0; --i) {
            base_loop.push_back(perimeter_start + i);
        }
        res.faces.push_back({{{base_loop}}});

        Shape out = P::make_shape(vfs, res, {{"type", "closed"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void execute_cone(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                             Interval x_extent, Interval y_extent, Interval z_extent, 
                             double zag_val) {
        FT f_xmin = CGAL::simplest_rational_in_interval<FT>(x_extent.min - 1e-5, x_extent.min + 1e-5);
        FT f_xmax = CGAL::simplest_rational_in_interval<FT>(x_extent.max - 1e-5, x_extent.max + 1e-5);
        execute_cone(vfs, fulfilling, f_xmin, f_xmax, y_extent, z_extent, zag_val);
    }
};

template <typename P = JotVfsProtocol>
struct ConeOp : ConeOpBase<P> {
    static constexpr const char* path = "jot/Cone";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Interval diameter, Interval height, double zag_val) {
        ConeOpBase<P>::execute_cone(vfs, fulfilling, height, diameter, diameter, zag_val);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "height", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Cone"},
            {"description", "Generates a sharp cone oriented along the X axis."},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "diameter"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ConeFitOp : ConeOpBase<P> {
    static constexpr const char* path = "jot/Cone/fit";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Interval width, Interval height, Interval depth, double zag_val) {
        ConeOpBase<P>::execute_cone(vfs, fulfilling, width, height, depth, zag_val);
    }
    static std::vector<std::string> argument_keys() { return {"width", "height", "depth", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Cone/fit"},
            {"description", "Generates a cone that fits the given bounding box, base in YZ, tip at X-max."},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "width"}, {"type", "jot:interval"}, {"default", 10.0}, {"description", "X-axis extent (height of cone)"}},
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 10.0}, {"description", "Y-axis base extent"}},
                {{"name", "depth"}, {"type", "jot:interval"}, {"default", 10.0}, {"description", "Z-axis base extent"}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ConeAngleOp : ConeOpBase<P> {
    static constexpr const char* path = "jot/Cone/angle";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Interval diameter, double angle, double zag_val) {
        FT rad = CGAL::simplest_rational_in_interval<FT>(diameter.size() - 1e-5, diameter.size() + 1e-5) / FT(2);
        double angle_rad = angle * 2.0 * M_PI;
        double cot_raw = 1.0 / std::tan(angle_rad);
        FT cot_val = CGAL::simplest_rational_in_interval<FT>(cot_raw - 1e-5, cot_raw + 1e-5);
        FT h = rad * cot_val;
        FT f_xmin = -h / FT(2);
        FT f_xmax = h / FT(2);
        ConeOpBase<P>::execute_cone(vfs, fulfilling, f_xmin, f_xmax, diameter, diameter, zag_val);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "angle", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Cone/angle"},
            {"description", "Generates a cone with base diameter and a specific taper angle."},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "diameter"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "angle"}, {"type", "jot:number"}, {"default", 0.125}, {"description", "Half-angle in turns (0.125 = 45 deg)"}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void cone_init(fs::VFSNode* vfs) {
    Processor::register_op<ConeOp<>, Interval, Interval, double>(vfs, "jot/Cone");
    Processor::register_op<ConeFitOp<>, Interval, Interval, Interval, double>(vfs, "jot/Cone/fit");
    Processor::register_op<ConeAngleOp<>, Interval, double, double>(vfs, "jot/Cone/angle");
}

} // namespace geo
} // namespace jotcad
