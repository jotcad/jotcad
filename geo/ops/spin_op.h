#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include <map>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SpinOpBase : P {
    static void execute_spin(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double start_tau, double end_tau, int resolution, const Matrix& axis_tf = Matrix::identity()) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        if (resolution < 3) resolution = 3;
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;

        double total_tau = end_tau - start_tau;
        bool is_full = std::abs(std::abs(total_tau) - 1.0) < 1e-9;
        int num_slices = is_full ? resolution : resolution + 1;

        // 1. Generate Rotated Vertices
        std::vector<std::vector<int>> vertex_map(geo.vertices.size(), std::vector<int>(num_slices));
        
        Matrix to_axis = axis_tf;
        Matrix from_axis = axis_tf.inverse();

        for (int j = 0; j < num_slices; ++j) {
            double turns = start_tau + (total_tau * j / resolution);
            // Rotate around the local Z of the axis frame
            Matrix rot = to_axis * Matrix::rotationZ(turns) * from_axis;
            for (size_t i = 0; i < geo.vertices.size(); ++i) {
                auto v = geo.vertices[i];
                Point_3 p(v.x, v.y, v.z);
                Point_3 tp = rot.transform(p);
                vertex_map[i][j] = (int)res.vertices.size();
                res.vertices.push_back({tp.x(), tp.y(), tp.z()});
            }
        }

        // 2. Process Points -> Arcs (Segments)
        for (int p_idx : geo.points) {
            for (int j = 0; j < num_slices - 1; ++j) {
                res.segments.push_back({vertex_map[p_idx][j], vertex_map[p_idx][j+1]});
            }
            if (is_full) {
                res.segments.push_back({vertex_map[p_idx][num_slices-1], vertex_map[p_idx][0]});
            }
        }

        // 3. Process Segments -> Ribbons (Faces)
        for (const auto& seg : geo.segments) {
            for (int j = 0; j < num_slices - 1; ++j) {
                int v0 = vertex_map[seg[0]][j];
                int v1 = vertex_map[seg[1]][j];
                int v2 = vertex_map[seg[1]][j+1];
                int v3 = vertex_map[seg[0]][j+1];
                res.faces.push_back({{{v0, v1, v2, v3}}});
            }
            if (is_full) {
                int v0 = vertex_map[seg[0]][num_slices-1];
                int v1 = vertex_map[seg[1]][num_slices-1];
                int v2 = vertex_map[seg[1]][0];
                int v3 = vertex_map[seg[0]][0];
                res.faces.push_back({{{v0, v1, v2, v3}}});
            }
        }

        // 4. Process Faces -> Volumes
        for (const auto& face : geo.faces) {
            for (int j = 0; j < num_slices - 1; ++j) {
                for (const auto& loop : face.loops) {
                    for (size_t k = 0; k < loop.size(); ++k) {
                        int i0 = loop[k];
                        int i1 = loop[(k + 1) % loop.size()];
                        int v0 = vertex_map[i0][j];
                        int v1 = vertex_map[i1][j];
                        int v2 = vertex_map[i1][j+1];
                        int v3 = vertex_map[i0][j+1];
                        
                        // Degenerate check (on axis)
                        auto p0 = res.vertices[v0];
                        auto p2 = res.vertices[v2];
                        if (p0.x == p2.x && p0.y == p2.y && p0.z == p2.z) continue;

                        res.faces.push_back({{{v0, v1, v2, v3}}});
                    }
                }
            }
            if (is_full) {
                int j = num_slices - 1;
                for (const auto& loop : face.loops) {
                    for (size_t k = 0; k < loop.size(); ++k) {
                        int i0 = loop[k];
                        int i1 = loop[(k + 1) % loop.size()];
                        int v0 = vertex_map[i0][j];
                        int v1 = vertex_map[i1][j];
                        int v2 = vertex_map[i1][0];
                        int v3 = vertex_map[i0][0];
                        res.faces.push_back({{{v0, v1, v2, v3}}});
                    }
                }
            } else {
                // Caps
                Geometry::Face bottom;
                for (const auto& loop : face.loops) {
                    std::vector<int> l;
                    for (int idx : loop) l.push_back(vertex_map[idx][0]);
                    bottom.loops.push_back(l);
                }
                res.faces.push_back(bottom);

                Geometry::Face top;
                for (const auto& loop : face.loops) {
                    std::vector<int> l;
                    for (int idx : loop) l.push_back(vertex_map[idx][num_slices-1]);
                    std::reverse(l.begin(), l.end());
                    top.loops.push_back(l);
                }
                res.faces.push_back(top);
            }
        }

        Shape out = in;
        std::string in_type = in.tags.value("type", "");
        if (in_type == "points") out.add_tag("type", "segments");
        else if (in_type == "segments") out.add_tag("type", "open");
        else if (in_type == "surface") out.add_tag("type", "closed");
        else if (in_type == "open") out.add_tag("type", "closed");
        
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct SpinOp : SpinOpBase<P> {
    static constexpr const char* path = "jot/spin";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double start, double end, int resolution) {
        SpinOpBase<P>::execute_spin(vfs, fulfilling, in, start, end, resolution);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "start", "end", "resolution"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/spin"},
            {"description", "Spins geometry around the local Z axis."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "start"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "end"}, {"type", "jot:number"}, {"default", 1.0}},
                {{"name", "resolution"}, {"type", "jot:number"}, {"default", 32}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct SpinXOp : SpinOpBase<P> {
    static constexpr const char* path = "jot/spinX";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double start, double end, int resolution) {
        SpinOpBase<P>::execute_spin(vfs, fulfilling, in, start, end, resolution, Matrix::rotationY(0.25));
    }
    static std::vector<std::string> argument_keys() { return {"$in", "start", "end", "resolution"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/spinX"}, 
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "start"}, {"type", "jot:number"}, {"default", 0.0}}, 
                {{"name", "end"}, {"type", "jot:number"}, {"default", 1.0}}, 
                {{"name", "resolution"}, {"type", "jot:number"}, {"default", 32}}
            })}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

template <typename P = JotVfsProtocol>
struct SpinYOp : SpinOpBase<P> {
    static constexpr const char* path = "jot/spinY";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double start, double end, int resolution) {
        SpinOpBase<P>::execute_spin(vfs, fulfilling, in, start, end, resolution, Matrix::rotationX(-0.25));
    }
    static std::vector<std::string> argument_keys() { return {"$in", "start", "end", "resolution"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/spinY"}, 
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "start"}, {"type", "jot:number"}, {"default", 0.0}}, 
                {{"name", "end"}, {"type", "jot:number"}, {"default", 1.0}}, 
                {{"name", "resolution"}, {"type", "jot:number"}, {"default", 32}}
            })}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void spin_init(fs::VFSNode* vfs) {
    Processor::register_op<SpinOp<>, Shape, double, double, int>(vfs, "jot/spin");
    Processor::register_op<SpinXOp<>, Shape, double, double, int>(vfs, "jot/spinX");
    Processor::register_op<SpinXOp<>, Shape, double, double, int>(vfs, "jot/sx"); // Alias for spinX
    Processor::register_op<SpinYOp<>, Shape, double, double, int>(vfs, "jot/spinY");
    Processor::register_op<SpinYOp<>, Shape, double, double, int>(vfs, "jot/sy"); // Alias for spinY
    Processor::register_op<SpinOp<>, Shape, double, double, int>(vfs, "jot/spinZ");
    Processor::register_op<SpinOp<>, Shape, double, double, int>(vfs, "jot/sz"); // Alias for spinZ
}

} // namespace geo
} // namespace jotcad
