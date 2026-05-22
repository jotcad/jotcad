#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PointOp : P {
    static constexpr const char* path = "jot/Point";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double x, double y, double z) {
        Geometry res;
        res.vertices.push_back({FT(x), FT(y), FT(z)});
        res.points.push_back(0);
        Shape out = P::make_shape(vfs, res, {{"type", "point"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"x", "y", "z"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Point"},
            {"description", "Generates a single point."},
            {"inputs", nlohmann::json::object()},
            {"arguments", json::array({
                {{"name", "x"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "y"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "z"}, {"type", "jot:number"}, {"default", 0.0}}
            })},
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
            int idx = (int)res.vertices.size();
            if (p.size() >= 3) {
                res.vertices.push_back({FT(p[0]), FT(p[1]), FT(p[2])});
                res.points.push_back(idx);
            } else if (p.size() == 2) {
                res.vertices.push_back({FT(p[0]), FT(p[1]), FT(0.0)});
                res.points.push_back(idx);
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
            {"inputs", nlohmann::json::object()},
            {"arguments", json::array({
                {{"name", "points"}, {"type", "array"}, {"items", {{"type", "array"}, {"items", {{"type", "jot:number"}}}}}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct PointsExtractOp : P {
    static constexpr const char* path = "jot/asPoints";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;
        res.vertices = geo.vertices;
        for (int i = 0; i < (int)res.vertices.size(); ++i) res.points.push_back(i);
        Shape out = P::make_shape(vfs, res, {{"type", "points"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/asPoints"},
            {"description", "Extracts vertices from a shape as a single point cloud shape."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct EachPointOp : P {
    static constexpr const char* path = "jot/points";
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
            p.tf = Matrix::translate(v.x, v.y, v.z);
            
            // Give each point a terminal geometry (origin) so it's reifiable as a vertex
            Geometry p_geo;
            p_geo.vertices.push_back({FT(0), FT(0), FT(0)});
            p_geo.points.push_back(0);
            p.geometry = vfs->materialize<Geometry>(p_geo);
            
            out.components.push_back(p);
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/points"},
            {"description", "Extracts vertices from a shape as individual child components."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void points_init(fs::VFSNode* vfs) {
    Processor::register_op<PointOp<>, double, double, double>(vfs, "jot/Point");
    Processor::register_op<PointsOp<>, std::vector<std::vector<double>>>(vfs, "jot/Points");
    Processor::register_op<PointsExtractOp<>, Shape>(vfs, "jot/asPoints");
    Processor::register_op<EachPointOp<>, Shape>(vfs, "jot/points");
}

} // namespace geo
} // namespace jotcad
