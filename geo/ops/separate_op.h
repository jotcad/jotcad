#pragma once
#include "protocols.h"
#include "processor.h"
#include <map>
#include <numeric>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SeparateOp : P {
    static constexpr const char* path = "jot/separate";

    struct UnionFind {
        std::vector<int> parent;
        UnionFind(int n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
        int find(int i) {
            if (parent[i] == i) return i;
            return parent[i] = find(parent[i]);
        }
        void unite(int i, int j) {
            int root_i = find(i);
            int root_j = find(j);
            if (root_i != root_j) parent[root_i] = root_j;
        }
    };

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        if (geo.vertices.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // 1. Find connected components using Union-Find on vertices
        UnionFind uf((int)geo.vertices.size());
        
        for (const auto& tri : geo.triangles) {
            uf.unite(tri[0], tri[1]);
            uf.unite(tri[1], tri[2]);
        }
        for (const auto& face : geo.faces) {
            for (const auto& loop : face.loops) {
                for (size_t i = 0; i < loop.size(); ++i) {
                    uf.unite(loop[i], loop[(i + 1) % loop.size()]);
                }
            }
        }
        for (const auto& seg : geo.segments) {
            uf.unite(seg[0], seg[1]);
        }
        // Points are their own components unless shared with something else

        // 2. Group primitives by their vertex component root
        std::map<int, std::vector<int>> components_vertices;
        for (int i = 0; i < (int)geo.vertices.size(); ++i) {
            components_vertices[uf.find(i)].push_back(i);
        }

        // Handle completely isolated points (those in geo.points but not united above)
        // Actually uf handles them fine - they are their own roots.

        if (components_vertices.size() <= 1) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Shape out_group;
        out_group.add_tag("type", "group");

        for (auto const& [root, v_indices] : components_vertices) {
            Geometry sub_geo;
            std::map<int, int> old_to_new;
            
            // Map used vertices
            for (int old_idx : v_indices) {
                old_to_new[old_idx] = (int)sub_geo.vertices.size();
                sub_geo.vertices.push_back(geo.vertices[old_idx]);
            }

            // Collect primitives belonging to this component
            for (const auto& tri : geo.triangles) {
                if (uf.find(tri[0]) == root) {
                    sub_geo.triangles.push_back({old_to_new[tri[0]], old_to_new[tri[1]], old_to_new[tri[2]]});
                }
            }
            for (const auto& face : geo.faces) {
                if (!face.loops.empty() && uf.find(face.loops[0][0]) == root) {
                    Geometry::Face nf;
                    for (const auto& loop : face.loops) {
                        std::vector<int> nl;
                        for (int idx : loop) nl.push_back(old_to_new[idx]);
                        nf.loops.push_back(nl);
                    }
                    sub_geo.faces.push_back(nf);
                }
            }
            for (const auto& seg : geo.segments) {
                if (uf.find(seg[0]) == root) {
                    sub_geo.segments.push_back({old_to_new[seg[0]], old_to_new[seg[1]]});
                }
            }
            for (int p_idx : geo.points) {
                if (uf.find(p_idx) == root) {
                    sub_geo.points.push_back(old_to_new[p_idx]);
                }
            }

            if (!sub_geo.vertices.empty()) {
                Shape component;
                component.tags = in.tags;
                component.geometry = vfs->materialize<Geometry>(sub_geo);
                out_group.components.push_back(component);
            }
        }

        vfs->write(fulfilling.with_output("$out"), out_group);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/separate"},
            {"description", "Splits the input geometry into separate shapes based on connected components (disconnected geometric islands)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void separate_init(fs::VFSNode* vfs) {
    Processor::register_op<SeparateOp<>, Shape>(vfs, "jot/separate");
}

} // namespace geo
} // namespace jotcad
