#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include <map>
#include <set>
#include <queue>
#include <algorithm>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SewOp : P {
    struct Edge {
        int v1, v2;
        bool operator<(const Edge& other) const {
            if (v1 != other.v1) return v1 < other.v1;
            return v2 < other.v2;
        }
    };

    struct FaceRef {
        int face_idx;
        bool reversed; // True if the face uses the edge in (v_max, v_min) order
    };

    struct InternalFace {
        std::vector<int> v_indices;
        bool visited = false;
        bool flip = false;
    };

    static void execute_impl(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& inputs) {
        std::vector<Vertex> unique_vertices;
        std::map<std::tuple<FT, FT, FT>, int> v_map;
        std::vector<InternalFace> faces;

        // 1. Collect and Weld
        for (const auto& s : inputs) {
            if (!s.geometry.has_value()) continue;
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            geo.apply_tf(s.tf);

            std::vector<int> remap;
            for (const auto& v : geo.vertices) {
                auto key = std::make_tuple(v.x, v.y, v.z);
                if (v_map.find(key) == v_map.end()) {
                    v_map[key] = unique_vertices.size();
                    unique_vertices.push_back(v);
                }
                remap.push_back(v_map[key]);
            }

            for (const auto& f : geo.faces) {
                if (f.loops.empty()) continue;
                InternalFace iface;
                for (int idx : f.loops[0]) iface.v_indices.push_back(remap[idx]);
                faces.push_back(iface);
            }
        }

        if (faces.empty()) {
            vfs->write(fulfilling.with_output("$out"), Shape{});
            return;
        }

        // 2. Build Connectivity
        std::map<Edge, std::vector<FaceRef>> edge_map;
        for (size_t i = 0; i < faces.size(); ++i) {
            const auto& v = faces[i].v_indices;
            for (size_t k = 0; k < v.size(); ++k) {
                int a = v[k], b = v[(k + 1) % v.size()];
                if (a == b) continue;
                edge_map[{std::min(a, b), std::max(a, b)}].push_back({(int)i, a > b});
            }
        }

        // 3. Topology Guard (Non-Manifold)
        for (const auto& [edge, refs] : edge_map) {
            if (refs.size() > 2) {
                throw std::runtime_error("jot/sew: Non-manifold edge detected (self-touching or ambiguous intersection). Edge shared by " + std::to_string(refs.size()) + " faces.");
            }
        }

        // 4. Orientation Solver (BFS)
        std::vector<std::vector<int>> components;
        for (size_t i = 0; i < faces.size(); ++i) {
            if (faces[i].visited) continue;

            std::vector<int> component;
            std::queue<int> q;
            q.push(i);
            faces[i].visited = true;

            while (!q.empty()) {
                int curr_id = q.front(); q.pop();
                component.push_back(curr_id);
                const auto& curr_face = faces[curr_id];

                for (size_t k = 0; k < curr_face.v_indices.size(); ++k) {
                    int a = curr_face.v_indices[k], b = curr_face.v_indices[(k + 1) % curr_face.v_indices.size()];
                    Edge e = {std::min(a, b), std::max(a, b)};
                    
                    for (const auto& neighbor : edge_map[e]) {
                        if (neighbor.face_idx == curr_id) continue;
                        bool a_is_rev = (a > b) ^ faces[curr_id].flip;
                        bool b_natural_is_rev = neighbor.reversed;
                        
                        if (!faces[neighbor.face_idx].visited) {
                            faces[neighbor.face_idx].flip = (b_natural_is_rev == a_is_rev);
                            faces[neighbor.face_idx].visited = true;
                            q.push(neighbor.face_idx);
                        } else if (b_natural_is_rev ^ faces[neighbor.face_idx].flip == a_is_rev) {
                            throw std::runtime_error("jot/sew: Non-orientable surface (Möbius strip) detected.");
                        }
                    }
                }
            }
            components.push_back(component);
        }

        // 5. Global Bias (Outward / Upward)
        Geometry out_geo;
        out_geo.vertices = unique_vertices;

        for (const auto& comp : components) {
            bool is_closed = true;
            FT total_vol = 0;
            Vector_3 net_normal(0, 0, 0);

            for (int f_id : comp) {
                const auto& f = faces[f_id];
                for (size_t k = 0; k < f.v_indices.size(); ++k) {
                    if (edge_map[{std::min(f.v_indices[k], f.v_indices[(k+1)%f.v_indices.size()]), 
                                  std::max(f.v_indices[k], f.v_indices[(k+1)%f.v_indices.size()])}].size() == 1) {
                        is_closed = false; break;
                    }
                }
                // Calculate properties for biasing (Assuming CCW winding)
                const auto& v0 = unique_vertices[f.v_indices[0]];
                const auto& v1 = unique_vertices[f.v_indices[1]];
                const auto& v2 = unique_vertices[f.v_indices[2]];
                Point_3 p0(v0.x, v0.y, v0.z), p1(v1.x, v1.y, v1.z), p2(v2.x, v2.y, v2.z);
                Vector_3 fn = CGAL::cross_product(p1 - p0, p2 - p1);
                if (faces[f_id].flip) fn = -fn;
                net_normal = net_normal + fn;

                for (size_t k = 1; k < f.v_indices.size() - 1; ++k) {
                    const auto& va = unique_vertices[f.v_indices[0]];
                    const auto& vb = unique_vertices[f.v_indices[k]];
                    const auto& vc = unique_vertices[f.v_indices[k+1]];
                    FT det = (va.x * (vb.y * vc.z - vb.z * vc.y) - va.y * (vb.x * vc.z - vb.z * vc.x) + va.z * (vb.x * vc.y - vb.y * vc.x));
                    total_vol += (faces[f_id].flip ? -det : det);
                }
            }

            bool flip_comp = is_closed ? (total_vol < 0) : (net_normal.z() < 0);
            for (int f_id : comp) {
                Geometry::Face out_f;
                std::vector<int> loop = faces[f_id].v_indices;
                if (faces[f_id].flip ^ flip_comp) std::reverse(loop.begin(), loop.end());
                out_f.loops.push_back(loop);
                out_geo.faces.push_back(out_f);
            }
        }

        vfs->write(fulfilling.with_output("$out"), P::make_shape(vfs, out_geo, {{"type", "solid"}}));
    }

    struct Constructor {
        static constexpr const char* path = "jot/Sew";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes) {
            execute_impl(vfs, fulfilling, shapes);
        }
        static std::vector<std::string> argument_keys() { return {"shapes"}; }
        static typename P::json schema() {
            return { {"path", path}, {"arguments", {{{"name", "shapes"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}}}, {"outputs", {{"$out", {{"type", "jot:shape"}}}}} };
        }
    };

    struct Method {
        static constexpr const char* path = "jot/sew";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& shapes) {
            std::vector<Shape> all = {in};
            all.insert(all.end(), shapes.begin(), shapes.end());
            execute_impl(vfs, fulfilling, all);
        }
        static std::vector<std::string> argument_keys() { return {"$in", "shapes"}; }
        static typename P::json schema() {
            return { {"path", path}, {"arguments", {{{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}}, {{"name", "shapes"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}}}, {"outputs", {{"$out", {{"type", "jot:shape"}}}}} };
        }
    };
};

static void sew_init(fs::VFSNode* vfs) {
    Processor::register_op<SewOp<>::Constructor, std::vector<Shape>>(vfs, "jot/Sew");
    Processor::register_op<SewOp<>::Method, Shape, std::vector<Shape>>(vfs, "jot/sew");
}

} // namespace geo
} // namespace jotcad
