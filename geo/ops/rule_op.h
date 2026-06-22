#pragma once

#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "rs/ruled_surfaces_base.h"
#include "rs/ruled_surfaces_objective_min_area.h"
#include "rs/ruled_surfaces_strategy_linear_slg.h"
#include "rs/ruled_surfaces_strategy_linear_all.h"
#include "rs/ruled_surfaces_strategy_seam_search_all.h"
#include "rs/ruled_surfaces_join_strategy_naive.h"
#include "rs/ruled_surfaces_sa_stopping_rules.h"
#include "boolean/engine.h"
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <map>
#include <set>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct RuleOp : P {
    static constexpr const char* path = "jot/Rule";

    using FT = EK::FT;

    static Geometry merge_and_weld(const Geometry& g1, const Geometry& g2, double tolerance = 1e-9) {
        Geometry out = g1;
        std::vector<int> v_map(g2.vertices.size());
        for (size_t i = 0; i < g2.vertices.size(); ++i) {
            const auto& v = g2.vertices[i];
            double vx = CGAL::to_double(v.x);
            double vy = CGAL::to_double(v.y);
            double vz = CGAL::to_double(v.z);
            
            int match_idx = -1;
            for (size_t j = 0; j < out.vertices.size(); ++j) {
                const auto& ov = out.vertices[j];
                double dx = CGAL::to_double(ov.x) - vx;
                double dy = CGAL::to_double(ov.y) - vy;
                double dz = CGAL::to_double(ov.z) - vz;
                if (std::sqrt(dx*dx + dy*dy + dz*dz) < tolerance) {
                    match_idx = (int)j;
                    break;
                }
            }
            if (match_idx != -1) {
                v_map[i] = match_idx;
            } else {
                v_map[i] = (int)out.vertices.size();
                out.vertices.push_back(v);
            }
        }

        for (const auto& f : g2.faces) {
            Geometry::Face new_f;
            for (const auto& loop : f.loops) {
                std::vector<int> new_loop;
                for (int idx : loop) {
                    new_loop.push_back(v_map[idx]);
                }
                new_f.loops.push_back(new_loop);
            }
            out.faces.push_back(new_f);
        }

        for (int p : g2.points) {
            out.points.push_back(v_map[p]);
        }

        for (const auto& seg : g2.segments) {
            out.segments.push_back({v_map[seg[0]], v_map[seg[1]]});
        }

        for (const auto& tri : g2.triangles) {
            out.triangles.push_back({v_map[tri[0]], v_map[tri[1]], v_map[tri[2]]});
        }

        return out;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& a, const Shape& b) {
        std::vector<ruled_surfaces::PolygonalChain> p_chains = extract_chains(vfs, a);
        std::vector<ruled_surfaces::PolygonalChain> q_chains = extract_chains(vfs, b);

        std::cout << "[DEBUG RuleOp] p_chains size: " << p_chains.size() << ", q_chains size: " << q_chains.size() << std::endl;
        for (size_t i = 0; i < p_chains.size(); ++i) {
            std::cout << "  p_chain " << i << " vertices: " << p_chains[i].size() << ", closed: " << (p_chains[i].front() == p_chains[i].back()) << std::endl;
        }
        for (size_t i = 0; i < q_chains.size(); ++i) {
            std::cout << "  q_chain " << i << " vertices: " << q_chains[i].size() << ", closed: " << (q_chains[i].front() == q_chains[i].back()) << std::endl;
        }

        if (p_chains.empty() || q_chains.empty()) {
            std::cout << "[DEBUG RuleOp] Empty chains, aborting" << std::endl;
            vfs->write(fulfilling.with_output("$out"), a);
            return;
        }

        ruled_surfaces::MinArea objective;
        ruled_surfaces::MultiSurfaceStats stats;
        ruled_surfaces::Mesh result_mesh;

        bool all_closed = true;
        for (const auto& c : p_chains) if (c.front() != c.back()) all_closed = false;
        for (const auto& c : q_chains) if (c.front() != c.back()) all_closed = false;

        ruled_surfaces::SolutionStats::Status status;
        using TriangulationStrategy = ruled_surfaces::LinearSearchSlg<ruled_surfaces::MinArea>;
        
        if (all_closed) {
            using SeamStrategy = ruled_surfaces::SeamSearchAll<TriangulationStrategy>;
            status = ruled_surfaces::NaiveJoinStrategy<SeamStrategy>::generate(
                p_chains, q_chains, objective, 1, &stats, &result_mesh);
        } else {
            status = ruled_surfaces::NaiveJoinStrategy<TriangulationStrategy>::generate(
                p_chains, q_chains, objective, 1, &stats, &result_mesh);
        }

        std::cout << "[DEBUG RuleOp] NaiveJoinStrategy status: " << status << ", result_mesh faces: " << result_mesh.number_of_faces() << std::endl;

        if (status != ruled_surfaces::SolutionStats::OK) {
            vfs->write(fulfilling.with_output("$out"), a);
            return;
        }

        Geometry res = mesh_to_geometry(result_mesh);
        
        Geometry g_a;
        if (a.geometry.has_value()) {
            g_a = vfs->read<Geometry>(a.geometry.value());
            g_a.apply_tf(a.tf);
        }
        Geometry g_b;
        if (b.geometry.has_value()) {
            g_b = vfs->read<Geometry>(b.geometry.value());
            g_b.apply_tf(b.tf);
        }

        std::cout << "[DEBUG RuleOp] res vertices: " << res.vertices.size() << ", faces: " << res.faces.size() << std::endl;
        std::cout << "[DEBUG RuleOp] g_a vertices: " << g_a.vertices.size() << ", faces: " << g_a.faces.size() << std::endl;
        std::cout << "[DEBUG RuleOp] g_b vertices: " << g_b.vertices.size() << ", faces: " << g_b.faces.size() << std::endl;

        Geometry final_geo = res;
        if (a.geometry.has_value()) {
            final_geo = merge_and_weld(final_geo, g_a);
        }
        if (b.geometry.has_value()) {
            final_geo = merge_and_weld(final_geo, g_b);
        }

        std::cout << "[DEBUG RuleOp] final_geo (before orient) vertices: " << final_geo.vertices.size() << ", faces: " << final_geo.faces.size() << ", triangles: " << final_geo.triangles.size() << std::endl;

        try {
            boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(final_geo);
            std::cout << "[DEBUG RuleOp] converted Surface_mesh vertices: " << mesh.number_of_vertices() << ", faces: " << mesh.number_of_faces() << ", closed: " << CGAL::is_closed(mesh) << std::endl;
            if (CGAL::is_closed(mesh)) {
                CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(mesh);
            }
            final_geo = boolean::Engine::mesh_to_geometry(mesh);
        } catch (const std::exception& e) {
            std::cout << "[DEBUG RuleOp] Mesh conversion exception: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "[DEBUG RuleOp] Unknown mesh conversion exception" << std::endl;
        }

        std::cout << "[DEBUG RuleOp] final_geo (after orient) vertices: " << final_geo.vertices.size() << ", faces: " << final_geo.faces.size() << ", triangles: " << final_geo.triangles.size() << std::endl;

        Shape out;
        final_geo.triangulate();
        out.geometry = vfs->materialize<Geometry>(final_geo);
        out.add_tag("type", "ruled_surface");
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<ruled_surfaces::PolygonalChain> extract_mesh_boundary_chains(const Geometry& geo) {
        boolean::ExactMesh mesh = boolean::Engine::geometry_to_mesh(geo);
        std::vector<ruled_surfaces::PolygonalChain> chains;
        std::set<boolean::ExactMesh::Halfedge_index> visited;

        for (auto h : mesh.halfedges()) {
            if (mesh.is_border(h) && visited.find(h) == visited.end()) {
                ruled_surfaces::PolygonalChain chain;
                auto curr = h;
                do {
                    visited.insert(curr);
                    auto v = mesh.target(curr);
                    auto p = mesh.point(v);
                    chain.push_back(ruled_surfaces::PointCgal(CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z())));
                    curr = mesh.next(curr);
                } while (curr != h);

                if (!chain.empty()) {
                    chain.push_back(chain.front()); // RuleOp expects closed loops to have duplicate start/end
                    chains.push_back(chain);
                }
            }
        }
        return chains;
    }

    static std::vector<ruled_surfaces::PolygonalChain> extract_chains(fs::VFSNode* vfs, const Shape& s) {
        std::vector<ruled_surfaces::PolygonalChain> chains;
        Matrix tf = s.tf;

        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());

            if (!geo.faces.empty()) {
                auto boundary_chains = extract_mesh_boundary_chains(geo);
                if (!boundary_chains.empty()) {
                    chains = boundary_chains;
                } else {
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

            // Transform all extracted chains by the shape's transform tf
            for (auto& chain : chains) {
                for (auto& pt : chain) {
                    Point_3 p(pt.x(), pt.y(), pt.z());
                    Point_3 tp = tf.transform(p);
                    pt = ruled_surfaces::PointCgal(CGAL::to_double(tp.x()), CGAL::to_double(tp.y()), CGAL::to_double(tp.z()));
                }
            }
        }

        if (chains.empty() && !s.geometry.has_value() && !s.components.empty()) {
            ruled_surfaces::PolygonalChain chain;
            for (const auto& comp : s.components) {
                Matrix m = tf * comp.tf;
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
            Matrix comp_tf = tf * comp.tf;
            global_comp.tf = comp_tf;
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
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "$a"}, {"type", "jot:shape"}},
                {{"name", "$b"}, {"type", "jot:shape"}}
            })},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}}}
            }}
        };
    }
};

template <typename P = JotVfsProtocol>
struct RuleMethodOp : RuleOp<P> {
    static constexpr const char* path = "jot/rule";
    static std::vector<std::string> argument_keys() { return {"$in", "$b"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/rule"},
            {"description", "Generates a ruled surface triangulating between two sets of boundary loops, supporting holes."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "$b"}, {"type", "jot:shape"}}
            })},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}}}
            }}
        };
    }
};

static void rule_init(fs::VFSNode* vfs) {
    Processor::register_op<RuleOp<>, Shape, Shape>(vfs, "jot/Rule");
    Processor::register_op<RuleMethodOp<>, Shape, Shape>(vfs, "jot/rule");
}

} // namespace geo
} // namespace jotcad
