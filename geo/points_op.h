#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PointOp : P {
    static constexpr const char* path = "jot/Point";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double x, double y, double z) {
        Geometry res;
        res.vertices.push_back({FT(x), FT(y), FT(z)});
        Shape out = P::make_shape(vfs, res, {{"type", "point"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"x", "y", "z"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Point"},
            {"description", "Generates a single point."},
            {"arguments", {
                {"x", {{"type", "jot:number"}, {"default", 0.0}}},
                {"y", {{"type", "jot:number"}, {"default", 0.0}}},
                {"z", {{"type", "jot:number"}, {"default", 0.0}}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct PointsOp : P {
    static constexpr const char* path = "jot/Points";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<std::vector<double>>& points) {
        Geometry res;
        for (const auto& p : points) {
            if (p.size() >= 3) {
                res.vertices.push_back({FT(p[0]), FT(p[1]), FT(p[2])});
            } else if (p.size() == 2) {
                res.vertices.push_back({FT(p[0]), FT(p[1]), FT(0.0)});
            }
        }
        Shape out = P::make_shape(vfs, res, {{"type", "points"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"points"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Points"},
            {"description", "Generates a point cloud from a list of coordinates."},
            {"arguments", {
                {"points", {{"type", "array"}, {"items", {{"type", "array"}, {"items", {{"type", "jot:number"}}}}}}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct PointsExtractOp : P {
    static constexpr const char* path = "jot/points";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;
        res.vertices = geo.vertices;
        Shape out = P::make_shape(vfs, res, {{"type", "points"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/points"},
            {"description", "Extracts vertices from a shape as a point cloud."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct EachPointOp : P {
    static constexpr const char* path = "jot/eachPoint";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        
        Shape out;
        out.tags = {{"type", "points"}};
        
        for (const auto& v : geo.vertices) {
            Shape p;
            p.tags = {{"type", "point"}};
            p.tf = Matrix::translate(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)).to_vec();
            
            // Give each point a terminal geometry (origin) so it's reifiable as a vertex
            Geometry p_geo;
            p_geo.vertices.push_back({FT(0), FT(0), FT(0)});
            p.geometry = vfs->materialize<Geometry>(p_geo);
            
            out.components.push_back(p);
        }
        
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/eachPoint"},
            {"description", "Extracts vertices from a shape as individual child components."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void points_init(fs::VFSNode* vfs) {
    Processor::register_op<PointOp<>, double, double, double>(vfs, "jot/Point");
    Processor::register_op<PointsOp<>, std::vector<std::vector<double>>>(vfs, "jot/Points");
    Processor::register_op<PointsExtractOp<>, Shape>(vfs, "jot/points");
    Processor::register_op<EachPointOp<>, Shape>(vfs, "jot/eachPoint");
}

} // namespace geo
} // namespace jotcad
