#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/zag.h"
#include "math/rational_approx.h"
#include "math/interval.h"
#include <CGAL/Circle_3.h>
#include <CGAL/Plane_3.h>

namespace jotcad {
namespace geo {

// --- Variant 1: Bounds-based Arc (Legacy) ---
template <typename P = JotVfsProtocol>
struct ArcBoundsOp : P {
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

        Shape out = P::make_shape(vfs, res, {{"type", "segments"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"diameter", "width", "height", "start", "end", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Arc"},
            {"description", "Generates an elliptical arc within a bounding box."},
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

// --- Variant 2: 2-Point + Radius Arc ---
template <typename P = JotVfsProtocol>
struct Arc2POp : P {
    static constexpr const char* path = "jot/Arc/2p";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& p1, const Shape& p2, double radius, bool large, bool cw, double zag_val) {
        Geometry res;
        EK::Point_3 pt1 = p1.tf.transform(EK::Point_3(0, 0, 0));
        EK::Point_3 pt2 = p2.tf.transform(EK::Point_3(0, 0, 0));

        EK::Vector_3 v = pt2 - pt1;
        double d2 = CGAL::to_double(v.squared_length());
        double d = std::sqrt(d2);

        if (d > 2.0 * radius || d < 1e-9) {
            // Fallback: Line
            res.vertices.push_back({pt1.x(), pt1.y(), pt1.z()});
            res.vertices.push_back({pt2.x(), pt2.y(), pt2.z()});
            res.segments.push_back({0, 1});
        } else {
            EK::Point_3 mid = pt1 + (v * 0.5);
            double h = std::sqrt(radius * radius - (d2 * 0.25));

            // Default plane: World XY (for now)
            EK::Vector_3 perp(-CGAL::to_double(v.y()), CGAL::to_double(v.x()), 0);
            double len_perp = std::sqrt(CGAL::to_double(perp.squared_length()));
            if (len_perp < 1e-9) perp = EK::Vector_3(0, 1, 0); 
            else perp = perp / len_perp;

            double side = (large == cw) ? -1.0 : 1.0;
            EK::Point_3 center = mid + (perp * (h * side));

            EK::Vector_3 vx = pt1 - center;
            vx = vx / std::sqrt(CGAL::to_double(vx.squared_length()));
            EK::Vector_3 vy = EK::Vector_3(-CGAL::to_double(vx.y()), CGAL::to_double(vx.x()), 0);

            auto get_angle = [&](EK::Point_3 p) {
                EK::Vector_3 vec = p - center;
                double x = CGAL::to_double(vec * vx);
                double y = CGAL::to_double(vec * vy);
                double angle = std::atan2(y, x) / (2.0 * M_PI);
                if (angle < 0) angle += 1.0;
                return angle;
            };

            double a1 = 0;
            double a2 = get_angle(pt2);
            double span = a2;

            if (cw) {
                if (span > 0) span -= 1.0;
            } else {
                if (span < 0) span += 1.0;
            }

            if (large && std::abs(span) < 0.5) {
                span = (span > 0) ? (span - 1.0) : (span + 1.0);
            } else if (!large && std::abs(span) > 0.5) {
                span = (span > 0) ? (span - 1.0) : (span + 1.0);
            }

            int sides = (int)std::max(1.0, std::ceil(std::abs(span) * zag(radius, zag_val)));
            for (int i = 0; i <= sides; ++i) {
                EK::Point_3 pt;
                if (i == 0) pt = pt1;
                else if (i == sides) pt = pt2;
                else {
                    double t = a1 + (span * i / sides);
                    double rad = t * 2.0 * M_PI;
                    double c = std::cos(rad);
                    double s = std::sin(rad);
                    pt = center + (vx * (radius * c)) + (vy * (radius * s));
                }
                res.vertices.push_back({pt.x(), pt.y(), pt.z()});
                if (i > 0) res.segments.push_back({(int)i - 1, (int)i});
            }
        }

        Shape out = P::make_shape(vfs, res, {{"type", "segments"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"p1", "p2", "radius", "large", "cw", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Arc/2p"},
            {"description", "Generates an arc between two points with a given radius (SVG style)."},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "p1"}, {"type", "jot:shape"}},
                {{"name", "p2"}, {"type", "jot:shape"}},
                {{"name", "radius"}, {"type", "jot:number"}},
                {{"name", "large"}, {"type", "jot:boolean"}, {"default", false}},
                {{"name", "cw"}, {"type", "jot:boolean"}, {"default", false}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

// --- Variant 3: 3-Point Arc ---
template <typename P = JotVfsProtocol>
struct Arc3POp : P {
    static constexpr const char* path = "jot/Arc/3p";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& p1, const Shape& p2, const Shape& p3, double zag_val) {
        Geometry res;
        EK::Point_3 pt1 = p1.tf.transform(EK::Point_3(0, 0, 0));
        EK::Point_3 pt2 = p2.tf.transform(EK::Point_3(0, 0, 0));
        EK::Point_3 pt3 = p3.tf.transform(EK::Point_3(0, 0, 0));

        try {
            CGAL::Circle_3<EK> circle(pt1, pt2, pt3);
            EK::Point_3 center = circle.center();
            FT sq_radius = circle.squared_radius();
            double radius = std::sqrt(CGAL::to_double(sq_radius));
            EK::Plane_3 plane = circle.supporting_plane();

            EK::Vector_3 vx = pt1 - center;
            FT len_vx = std::sqrt(CGAL::to_double(vx.squared_length()));
            vx = vx / len_vx;

            EK::Vector_3 vn = plane.orthogonal_vector();
            FT len_vn = std::sqrt(CGAL::to_double(vn.squared_length()));
            vn = vn / len_vn;

            EK::Vector_3 vy = CGAL::cross_product(vn, vx);

            auto get_angle = [&](EK::Point_3 p) {
                EK::Vector_3 v = p - center;
                double x = CGAL::to_double(v * vx);
                double y = CGAL::to_double(v * vy);
                double angle = std::atan2(y, x) / (2.0 * M_PI);
                if (angle < 0) angle += 1.0;
                return angle;
            };

            double a1 = 0;
            double a2 = get_angle(pt2);
            double a3 = get_angle(pt3);

            double span = a3;
            // If the midpoint (pt2) is "ahead" of the endpoint (pt3), we must cross the 0 boundary
            if (a2 > a3) {
                span = a3 - 1.0;
            }

            int sides = (int)std::max(1.0, std::ceil(std::abs(span) * zag(radius, zag_val)));
            for (int i = 0; i <= sides; ++i) {
                EK::Point_3 pt;
                if (i == 0) pt = pt1;
                else if (i == sides) pt = pt3;
                else {
                    double t = a1 + (span * i / sides);
                    double rad = t * 2.0 * M_PI;
                    double c = std::cos(rad);
                    double s = std::sin(rad);
                    pt = center + (vx * (radius * c)) + (vy * (radius * s));
                }
                res.vertices.push_back({pt.x(), pt.y(), pt.z()});
                if (i > 0) res.segments.push_back({(int)i - 1, (int)i});
            }
        } catch (...) {
            res.vertices.push_back({pt1.x(), pt1.y(), pt1.z()});
            res.vertices.push_back({pt3.x(), pt3.y(), pt3.z()});
            res.segments.push_back({0, 1});
        }

        Shape out = P::make_shape(vfs, res, {{"type", "segments"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"p1", "p2", "p3", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Arc/3p"},
            {"description", "Generates an arc passing through three points."},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "p1"}, {"type", "jot:shape"}},
                {{"name", "p2"}, {"type", "jot:shape"}},
                {{"name", "p3"}, {"type", "jot:shape"}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void arc_init(fs::VFSNode* vfs) {
    Processor::register_op<ArcBoundsOp<>, Interval, Interval, Interval, double, double, double>(vfs, "jot/Arc");
    Processor::register_op<Arc2POp<>, Shape, Shape, double, bool, bool, double>(vfs, "jot/Arc/2p");
    Processor::register_op<Arc3POp<>, Shape, Shape, Shape, double>(vfs, "jot/Arc/3p");
}

} // namespace geo
} // namespace jotcad
