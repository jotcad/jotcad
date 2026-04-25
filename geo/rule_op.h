#pragma once

#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"
#include "rs/ruled_surfaces_base.h"
#include "rs/ruled_surfaces_objective_min_area.h"
#include "rs/ruled_surfaces_strategy_linear_slg.h"
#include "rs/ruled_surfaces_strategy_linear_all.h"
#include "rs/ruled_surfaces_strategy_seam_search_all.h"
#include "rs/ruled_surfaces_join_strategy_naive.h"
#include "rs/ruled_surfaces_sa_stopping_rules.h"
#include <CGAL/Surface_mesh.h>
#include <map>
#include <set>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct RuleOp : P {
    static constexpr const char* path = "jot/Rule";

    using FT = double;

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& a, const Shape& b) {
        std::vector<ruled_surfaces::PolygonalChain> p_chains = extract_chains(vfs, a);
        std::vector<ruled_surfaces::PolygonalChain> q_chains = extract_chains(vfs, b);

        if (p_chains.empty() || q_chains.empty()) {
            vfs->write<Shape>(fulfilling, a, "$out");
            return;
        }

        ruled_surfaces::MinArea objective;
        ruled_surfaces::MultiSurfaceStats stats;
        ruled_surfaces::Mesh result_mesh;

        bool all_closed = true;
        for (const auto& c : p_chains) if (c.front() != c.back()) all_closed = false;
        for (const auto& c : q_chains) if (c.front() != c.back()) all_closed = false;

        ruled_surfaces::SolutionStats::Status status;
        // For simple ruling, use the Dijkstra-based strategy which is O(N^2)
        // rather than the exhaustive permutation strategy which is O(2^N).
        using TriangulationStrategy = ruled_surfaces::LinearSearchSlg<ruled_surfaces::MinArea>;
        
        if (all_closed) {
            // For loops, we search for the best seam alignment.
            // Exhaustive search is fine for N < 50.
            using SeamStrategy = ruled_surfaces::SeamSearchAll<TriangulationStrategy>;
            status = ruled_surfaces::NaiveJoinStrategy<SeamStrategy>::generate(
                p_chains, q_chains, objective, 1, &stats, &result_mesh);
        } else {
            // For open polylines, no seam search needed.
            status = ruled_surfaces::NaiveJoinStrategy<TriangulationStrategy>::generate(
                p_chains, q_chains, objective, 1, &stats, &result_mesh);
        }

        if (status != ruled_surfaces::SolutionStats::OK) {
            vfs->write<Shape>(fulfilling, a, "$out");
            return;
        }

        Geometry res = mesh_to_geometry(result_mesh);
        Shape out;
        out.geometry = vfs->write_anonymous<Geometry>(res);
        out.add_tag("type", "ruled_surface");
        vfs->write<Shape>(fulfilling, out, "$out");
    }

    static std::vector<ruled_surfaces::PolygonalChain> extract_chains(fs::VFSNode* vfs, const Shape& s) {
        std::vector<ruled_surfaces::PolygonalChain> chains;
        Matrix tf = Matrix::from_vec(s.tf);

        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            geo.apply_tf(tf.to_vec());

            for (const auto& face : geo.faces) {
                for (const auto& loop : face.loops) {
                    ruled_surfaces::PolygonalChain chain;
                    for (int idx : loop) {
                        if (idx >= 0 && idx < (int)geo.vertices.size()) {
                            const auto& v = geo.vertices[idx];
                            chain.push_back(ruled_surfaces::PointCgal(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)));
                        }
                    }
                    if (!chain.empty()) {
                        if (chain.front() != chain.back()) chain.push_back(chain.front());
                        chains.push_back(chain);
                    }
                }
            }

            if (!geo.segments.empty()) {
                std::map<int, std::vector<int>> adj;
                for (const auto& seg : geo.segments) {
                    adj[seg[0]].push_back(seg[1]);
                    adj[seg[1]].push_back(seg[0]);
                }

                std::set<int> visited;
                auto extract_path = [&](int start, bool is_loop) {
                    ruled_surfaces::PolygonalChain chain;
                    int curr = start;
                    int prev = -1;
                    while (curr != -1 && visited.find(curr) == visited.end()) {
                        visited.insert(curr);
                        const auto& v = geo.vertices[curr];
                        chain.push_back(ruled_surfaces::PointCgal(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)));
                        int next = -1;
                        for (int neighbor : adj[curr]) {
                            if (neighbor != prev && visited.find(neighbor) == visited.end()) {
                                next = neighbor; break;
                            }
                        }
                        prev = curr; curr = next;
                    }
                    if (is_loop && !chain.empty()) chain.push_back(chain.front());
                    if (chain.size() >= 2) chains.push_back(chain);
                };

                for (const auto& [v_idx, neighbors] : adj) {
                    if (neighbors.size() == 1 && visited.find(v_idx) == visited.end()) extract_path(v_idx, false);
                }
                for (const auto& [v_idx, neighbors] : adj) {
                    if (neighbors.size() == 2 && visited.find(v_idx) == visited.end()) extract_path(v_idx, true);
                }
            }
        }

        if (chains.empty() && !s.geometry.has_value() && !s.components.empty()) {
            ruled_surfaces::PolygonalChain chain;
            for (const auto& comp : s.components) {
                Matrix m = tf * Matrix::from_vec(comp.tf);
                Point_3 p = m.t.transform(Point_3(0, 0, 0));
                chain.push_back(ruled_surfaces::PointCgal(CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z())));
            }
            if (chain.size() >= 3) {
                if (chain.front() != chain.back()) chain.push_back(chain.front());
                chains.push_back(chain);
            }
        }

        for (const auto& comp : s.components) {
            Shape global_comp = comp;
            Matrix comp_tf = tf * Matrix::from_vec(comp.tf);
            global_comp.tf = comp_tf.to_vec();
            auto comp_chains = extract_chains(vfs, global_comp);
            chains.insert(chains.end(), comp_chains.begin(), comp_chains.end());
        }
        return chains;
    }

    static Geometry mesh_to_geometry(const ruled_surfaces::Mesh& mesh) {
        Geometry geo;
        std::map<ruled_surfaces::Mesh::Vertex_index, int> v_map;
        for (auto v : mesh.vertices()) {
            auto p = mesh.point(v);
            v_map[v] = geo.vertices.size();
            geo.vertices.push_back({FT(p.x()), FT(p.y()), FT(p.z())});
        }
        for (auto f : mesh.faces()) {
            typename Geometry::Face out_face;
            std::vector<int> loop;
            for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) {
                loop.push_back(v_map[v]);
            }
            out_face.loops.push_back(loop);
            geo.faces.push_back(out_face);
        }
        return geo;
    }

    static std::vector<std::string> argument_keys() { return {"$a", "$b"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Rule"},
            {"description", "Generates a ruled surface triangulating between two sets of boundary loops, supporting holes."},
            {"arguments", {
                {"$a", {{"type", "shape"}, {"affiliate", "$out"}}},
                {"$b", {{"type", "shape"}, {"affiliate", "$out"}}}
            }},
            {"outputs", {
                {"$out", {{"type", "shape"}}}
            }}
        };
    }
};

static void rule_init(fs::VFSNode* vfs) {
    Processor::register_op<RuleOp<>, Shape, Shape>(vfs, "jot/Rule");
}

} // namespace geo
} // namespace jotcad
