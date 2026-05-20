#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/catmull_rom.h"
#include "math/zag.h"
#include <iostream>

namespace jotcad {
namespace geo {

namespace detail {
    inline void collect_points_recursive(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<EK::Point_3>& pts) {
        Matrix current_tf = parent_tf * s.tf;
        
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            if (!geo.vertices.empty()) {
                for (const auto& v : geo.vertices) {
                    pts.push_back(current_tf.transform(EK::Point_3(v.x, v.y, v.z)));
                }
            } else {
                pts.push_back(current_tf.transform(EK::Point_3(0, 0, 0)));
            }
        } else if (s.components.empty()) {
            pts.push_back(current_tf.transform(EK::Point_3(0, 0, 0)));
        }

        for (const auto& child : s.components) {
            collect_points_recursive(vfs, child, current_tf, pts);
        }
    }

    inline Geometry generate_path_geometry(const std::vector<EK::Point_3>& points, bool closed, bool smooth, double zag_val) {
        Geometry res;
        if (points.size() < 2) return res;

        std::vector<EK::Point_3> curve_points;

        if (smooth) {
            // Determine steps per segment. 
            // Default to 10 steps if zag is 0 but smooth is true.
            int steps = (zag_val <= 0) ? 10 : (int)std::ceil(1.0 / zag_val);
            if (steps < 1) steps = 1;

            std::vector<EK::Point_3> padded = points;
            if (closed) {
                padded.insert(padded.begin(), points.back());
                padded.push_back(points.front());
                padded.push_back(points[1]);
            } else {
                padded.insert(padded.begin(), points.front());
                padded.push_back(points.back());
            }

            for (size_t i = 0; i < padded.size() - 3; ++i) {
                const auto& p0 = padded[i];
                const auto& p1 = padded[i + 1];
                const auto& p2 = padded[i + 2];
                const auto& p3 = padded[i + 3];

                for (int j = 0; j < steps; ++j) {
                    double t = (double)j / steps;
                    curve_points.push_back(math::catmull_rom<EK>(p0, p1, p2, p3, t));
                }
            }
            if (!closed) {
                curve_points.push_back(points.back());
            }
        } else {
            curve_points = points;
        }

        for (size_t i = 0; i < curve_points.size(); ++i) {
            res.vertices.push_back({curve_points[i].x(), curve_points[i].y(), curve_points[i].z()});
            if (i > 0) {
                res.segments.push_back({(int)i - 1, (int)i});
            }
        }

        if (closed && curve_points.size() > 2) {
            res.segments.push_back({(int)res.vertices.size() - 1, 0});
        }

        return res;
    }
}

template <typename P = JotVfsProtocol>
struct LinkOp : P {
    static constexpr const char* path = "jot/link";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools, bool smooth = false, double zag = 0) {
        std::vector<EK::Point_3> pts;
        detail::collect_points_recursive(vfs, in, Matrix::identity(), pts);
        for (const auto& t : tools) {
            detail::collect_points_recursive(vfs, t, Matrix::identity(), pts);
        }

        Geometry res = detail::generate_path_geometry(pts, false, smooth, zag);

        Shape out = in; 
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "segments");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "tools", "smooth", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/link"},
            {"description", "Connects points into an open path, optionally with Catmull-Rom smoothing."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", {
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}},
                {{"name", "smooth"}, {"type", "jot:boolean"}, {"default", false}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct LoopOp : P {
    static constexpr const char* path = "jot/loop";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools, bool smooth = false, double zag = 0) {
        std::vector<EK::Point_3> pts;
        detail::collect_points_recursive(vfs, in, Matrix::identity(), pts);
        for (const auto& t : tools) {
            detail::collect_points_recursive(vfs, t, Matrix::identity(), pts);
        }

        Geometry res = detail::generate_path_geometry(pts, true, smooth, zag);

        Shape out = in;
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "segments");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "tools", "smooth", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/loop"},
            {"description", "Connects points into a closed loop, optionally with Catmull-Rom smoothing."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", {
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}},
                {{"name", "smooth"}, {"type", "jot:boolean"}, {"default", false}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct LinkConstructorOp : P {
    static constexpr const char* path = "jot/Link";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes, bool smooth = false, double zag = 0) {
        std::vector<EK::Point_3> pts;
        for (const auto& s : shapes) {
            detail::collect_points_recursive(vfs, s, Matrix::identity(), pts);
        }

        Geometry res = detail::generate_path_geometry(pts, false, smooth, zag);

        Shape out;
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "segments");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"shapes", "smooth", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Link"},
            {"description", "Creates an open path from a sequence of shapes or points."},
            {"arguments", {
                {{"name", "shapes"}, {"type", "jot:shapes"}},
                {{"name", "smooth"}, {"type", "jot:boolean"}, {"default", false}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct LoopConstructorOp : P {
    static constexpr const char* path = "jot/Loop";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes, bool smooth = false, double zag = 0) {
        std::vector<EK::Point_3> pts;
        for (const auto& s : shapes) {
            detail::collect_points_recursive(vfs, s, Matrix::identity(), pts);
        }

        Geometry res = detail::generate_path_geometry(pts, true, smooth, zag);

        Shape out;
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "segments");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"shapes", "smooth", "zag"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Loop"},
            {"description", "Creates a closed loop from a sequence of shapes or points."},
            {"arguments", {
                {{"name", "shapes"}, {"type", "jot:shapes"}},
                {{"name", "smooth"}, {"type", "jot:boolean"}, {"default", false}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void path_init(fs::VFSNode* vfs) {
    Processor::register_op<LinkOp<>, Shape, std::vector<Shape>, bool, double>(vfs, "jot/link");
    Processor::register_op<LoopOp<>, Shape, std::vector<Shape>, bool, double>(vfs, "jot/loop");
    Processor::register_op<LinkConstructorOp<>, std::vector<Shape>, bool, double>(vfs, "jot/Link");
    Processor::register_op<LoopConstructorOp<>, std::vector<Shape>, bool, double>(vfs, "jot/Loop");
}

} // namespace geo
} // namespace jotcad
