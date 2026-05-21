#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include "math/interval.h"

namespace jotcad {
namespace geo {

template <typename P>
struct AreaMeasurementOp : P {
    static constexpr const char* path = "jot/area";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        FT total = 0;
        if (in.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(in.geometry.value());
            boolean::ExactMesh mesh = boolean::Engine::geometry_to_mesh(geo);
            for (auto f : mesh.faces()) {
                auto h = mesh.halfedge(f);
                auto p0 = mesh.point(mesh.source(h)), p1 = mesh.point(mesh.target(h)), p2 = mesh.point(mesh.target(mesh.next(h)));
                auto v1 = p1 - p0, v2 = p2 - p0;
                auto cp = CGAL::cross_product(v1, v2);
                total += std::sqrt(std::max(0.0, CGAL::to_double(cp.squared_length()))) / 2.0;
            }
        }
        vfs->write(fulfilling.with_output("$out"), CGAL::to_double(total));
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {{"path", "jot/area"}, {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to measure."}}}}}, {"outputs", {{"$out", {{"type", "jot:number"}}}}}};
    }
};

template <typename P>
struct LengthMeasurementOp : P {
    static constexpr const char* path = "jot/length";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        FT total = 0;
        if (in.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(in.geometry.value());
            for (const auto& seg : geo.segments) {
                auto v1 = geo.vertices[seg[0]], v2 = geo.vertices[seg[1]];
                FT dx = v1.x - v2.x, dy = v1.y - v2.y, dz = v1.z - v2.z;
                total += std::sqrt(std::max(0.0, CGAL::to_double(dx*dx + dy*dy + dz*dz)));
            }
        }
        vfs->write(fulfilling.with_output("$out"), CGAL::to_double(total));
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {{"path", "jot/length"}, {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to measure."}}}}}, {"outputs", {{"$out", {{"type", "jot:number"}}}}}};
    }
};

template <typename P>
struct VolumeMeasurementOp : P {
    static constexpr const char* path = "jot/volume";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        FT total = 0;
        if (in.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(in.geometry.value());
            if (!geo.triangles.empty()) {
                for (const auto& t : geo.triangles) {
                    auto v0 = geo.vertices[t[0]], v1 = geo.vertices[t[1]], v2 = geo.vertices[t[2]];
                    Vector_3 a(v0.x, v0.y, v0.z), b(v1.x, v1.y, v1.z), c(v2.x, v2.y, v2.z);
                    total += CGAL::to_double(CGAL::scalar_product(CGAL::cross_product(a, b), c)) / 6.0;
                }
            } else {
                for (const auto& f : geo.faces) {
                    if (f.loops.empty() || f.loops[0].size() < 3) continue;
                    auto v0 = geo.vertices[f.loops[0][0]];
                    for (size_t i = 1; i < f.loops[0].size() - 1; ++i) {
                        auto v1 = geo.vertices[f.loops[0][i]];
                        auto v2 = geo.vertices[f.loops[0][i+1]];
                        Vector_3 a(v0.x, v0.y, v0.z), b(v1.x, v1.y, v1.z), c(v2.x, v2.y, v2.z);
                        total += CGAL::to_double(CGAL::scalar_product(CGAL::cross_product(a, b), c)) / 6.0;
                    }
                }
            }
        }
        vfs->write(fulfilling.with_output("$out"), std::abs(CGAL::to_double(total)));
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {{"path", "jot/volume"}, {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to measure."}}}}}, {"outputs", {{"$out", {{"type", "jot:number"}}}}}};
    }
};

// --- COORDINATE MEASUREMENT OPERATORS ---

static Interval calculate_envelope_internal(fs::VFSNode* vfs, const Shape& in, int axis) {
    Interval iv;
    if (in.geometry.has_value()) {
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        for (const auto& v : geo.vertices) {
            Point_3 p = in.tf.t.transform(Point_3(v.x, v.y, v.z));
            iv.expand((axis == 0) ? CGAL::to_double(p.x()) : (axis == 1) ? CGAL::to_double(p.y()) : CGAL::to_double(p.z()));
        }
    } else if (!in.components.empty()) {
        for (const auto& child : in.components) {
            iv.expand(CGAL::to_double(child.tf.t.hm(axis, 3)));
        }
    } else {
        iv.expand(CGAL::to_double(in.tf.t.hm(axis, 3)));
    }
    return iv;
}

template <typename P>
struct XMeasurementOp : P {
    static constexpr const char* path = "jot/x";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Interval iv = calculate_envelope_internal(vfs, in, 0);
        vfs->write(fulfilling.with_output("$out"), nlohmann::json{iv.min, iv.max});
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() { return {{"path", "jot/x"}, {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to measure."}}}}}, {"outputs", {{"$out", {{"type", "jot:interval"}}}}}}; }
};

template <typename P>
struct YMeasurementOp : P {
    static constexpr const char* path = "jot/y";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Interval iv = calculate_envelope_internal(vfs, in, 1);
        vfs->write(fulfilling.with_output("$out"), nlohmann::json{iv.min, iv.max});
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() { return {{"path", "jot/y"}, {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to measure."}}}}}, {"outputs", {{"$out", {{"type", "jot:interval"}}}}}}; }
};

template <typename P>
struct ZMeasurementOp : P {
    static constexpr const char* path = "jot/z";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Interval iv = calculate_envelope_internal(vfs, in, 2);
        vfs->write(fulfilling.with_output("$out"), nlohmann::json{iv.min, iv.max});
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() { return {{"path", "jot/z"}, {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to measure."}}}}}, {"outputs", {{"$out", {{"type", "jot:interval"}}}}}}; }
};

// --- REDUCTION OPERATORS ---

template <typename P>
struct MaxReductionOp : P {
    static constexpr const char* path = "jot/max";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const nlohmann::json& in) {
        vfs->write(fulfilling.with_output("$out"), Interval::from_json(in).max);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() { return {{"path", "jot/max"}, {"inputs", {{"$in", {{"type", "jot:interval"}, {"description", "The interval to reduce."}}}}}, {"outputs", {{"$out", {{"type", "jot:number"}}}}}}; }
};

template <typename P>
struct MinReductionOp : P {
    static constexpr const char* path = "jot/min";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const nlohmann::json& in) {
        vfs->write(fulfilling.with_output("$out"), Interval::from_json(in).min);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() { return {{"path", "jot/min"}, {"inputs", {{"$in", {{"type", "jot:interval"}, {"description", "The interval to reduce."}}}}}, {"outputs", {{"$out", {{"type", "jot:number"}}}}}}; }
};

template <typename P>
struct MidReductionOp : P {
    static constexpr const char* path = "jot/mid";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const nlohmann::json& in) {
        vfs->write(fulfilling.with_output("$out"), Interval::from_json(in).center());
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() { return {{"path", "jot/mid"}, {"inputs", {{"$in", {{"type", "jot:interval"}, {"description", "The interval to reduce."}}}}}, {"outputs", {{"$out", {{"type", "jot:number"}}}}}}; }
};

template <typename P>
struct SpanReductionOp : P {
    static constexpr const char* path = "jot/span";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const nlohmann::json& in) {
        vfs->write(fulfilling.with_output("$out"), Interval::from_json(in).size());
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() { return {{"path", "jot/span"}, {"inputs", {{"$in", {{"type", "jot:interval"}, {"description", "The interval to reduce."}}}}}, {"arguments", nlohmann::json::array()}, {"outputs", {{"$out", {{"type", "jot:number"}}}}}}; }
};

template <typename P>
struct FacingOp : P {
    static constexpr const char* path = "jot/facing";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& vec) {
        if (vec.size() < 3) { vfs->write(fulfilling.with_output("$out"), 0.0); return; }
        double nx = CGAL::to_double(in.tf.t.hm(0, 2));
        double ny = CGAL::to_double(in.tf.t.hm(1, 2));
        double nz = CGAL::to_double(in.tf.t.hm(2, 2));
        double dot = nx * vec[0] + ny * vec[1] + nz * vec[2];
        vfs->write(fulfilling.with_output("$out"), dot);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "vec"}; }
    static typename P::json schema() {
        return {{"path", "jot/facing"}, {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to measure."}}}}}, {"arguments", nlohmann::json::array({ {{"name", "vec"}, {"type", "jot:numbers"}} })}, {"outputs", {{"$out", {{"type", "jot:number"}}}}}};
    }
};

inline void measure_init(fs::VFSNode* vfs) {
    Processor::register_op<AreaMeasurementOp<JotVfsProtocol>, Shape>(vfs, "jot/area");
    Processor::register_op<LengthMeasurementOp<JotVfsProtocol>, Shape>(vfs, "jot/length");
    Processor::register_op<VolumeMeasurementOp<JotVfsProtocol>, Shape>(vfs, "jot/volume");
    Processor::register_op<XMeasurementOp<JotVfsProtocol>, Shape>(vfs, "jot/x");
    Processor::register_op<YMeasurementOp<JotVfsProtocol>, Shape>(vfs, "jot/y");
    Processor::register_op<ZMeasurementOp<JotVfsProtocol>, Shape>(vfs, "jot/z");
    Processor::register_op<MaxReductionOp<JotVfsProtocol>, nlohmann::json>(vfs, "jot/max");
    Processor::register_op<MinReductionOp<JotVfsProtocol>, nlohmann::json>(vfs, "jot/min");
    Processor::register_op<MidReductionOp<JotVfsProtocol>, nlohmann::json>(vfs, "jot/mid");
    Processor::register_op<SpanReductionOp<JotVfsProtocol>, nlohmann::json>(vfs, "jot/span");
    Processor::register_op<FacingOp<JotVfsProtocol>, Shape, std::vector<double>>(vfs, "jot/facing");
}

} // namespace geo
} // namespace jotcad
