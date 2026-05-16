#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/rmf.h"
#include "boolean/engine.h"
#include <map>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SweepOp : P {
    static void collect_profile(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, Geometry& combined) {
        Matrix current_tf = parent_tf * s.tf;
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            std::map<int, int> v_map;
            for (size_t i = 0; i < geo.vertices.size(); ++i) {
                Point_3 p(geo.vertices[i].x, geo.vertices[i].y, geo.vertices[i].z);
                Point_3 tp = current_tf.transform(p);
                v_map[i] = (int)combined.vertices.size();
                combined.vertices.push_back({tp.x(), tp.y(), tp.z()});
            }
            for (const auto& f : geo.faces) {
                Geometry::Face nf;
                for (const auto& loop : f.loops) {
                    std::vector<int> nl;
                    for (int idx : loop) nl.push_back(v_map[idx]);
                    nf.loops.push_back(nl);
                }
                combined.faces.push_back(nf);
            }
            for (const auto& s_seg : geo.segments) {
                combined.segments.push_back({v_map[s_seg[0]], v_map[s_seg[1]]});
            }
            for (const auto& tri : geo.triangles) {
                combined.triangles.push_back({v_map[tri[0]], v_map[tri[1]], v_map[tri[2]]});
            }
        }
        for (const auto& child : s.components) {
            collect_profile(vfs, child, current_tf, combined);
        }
    }

    static void collect_paths(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<std::vector<Point_3>>& paths) {
        Matrix current_tf = parent_tf * s.tf;
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            if (!geo.segments.empty()) {
                // Adjacency map for segments
                std::map<int, std::vector<int>> adj;
                std::vector<std::pair<int, int>> remaining_edges;
                for (const auto& seg : geo.segments) {
                    adj[seg[0]].push_back(seg[1]);
                    adj[seg[1]].push_back(seg[0]);
                    remaining_edges.push_back({std::min(seg[0], seg[1]), std::max(seg[0], seg[1])});
                }

                auto get_edge_idx = [&](int v1, int v2) {
                    int a = std::min(v1, v2), b = std::max(v1, v2);
                    for (size_t i = 0; i < remaining_edges.size(); ++i) {
                        if (remaining_edges[i].first == a && remaining_edges[i].second == b) return (int)i;
                    }
                    return -1;
                };

                // 1. Follow chains starting from leaves or junctions
                for (size_t n = 0; n < geo.vertices.size(); ++n) {
                    int start_node = (int)n;
                    // We start a new path from any node that is a leaf (1) or junction (>2)
                    if (adj[start_node].size() != 2) {
                        for (int neighbor : adj[start_node]) {
                            int edge_idx = get_edge_idx(start_node, neighbor);
                            if (edge_idx == -1) continue; // Already swept

                            std::vector<Point_3> chain;
                            chain.push_back(current_tf.transform(Point_3(geo.vertices[start_node].x, geo.vertices[start_node].y, geo.vertices[start_node].z)));
                            
                            int curr = start_node;
                            int next = neighbor;

                            while (true) {
                                int e_idx = get_edge_idx(curr, next);
                                if (e_idx == -1) break;
                                remaining_edges.erase(remaining_edges.begin() + e_idx);
                                
                                chain.push_back(current_tf.transform(Point_3(geo.vertices[next].x, geo.vertices[next].y, geo.vertices[next].z)));
                                
                                // Continue if the next node is a simple turn (degree 2)
                                if (adj[next].size() == 2) {
                                    int prev = curr;
                                    curr = next;
                                    next = (adj[curr][0] == prev) ? adj[curr][1] : adj[curr][0];
                                } else {
                                    break; // Junction or Leaf
                                }
                            }
                            if (chain.size() >= 2) paths.push_back(chain);
                        }
                    }
                }

                // 2. Handle remaining isolated closed loops (all vertices degree 2)
                while (!remaining_edges.empty()) {
                    auto edge = remaining_edges[0];
                    remaining_edges.erase(remaining_edges.begin());

                    std::vector<Point_3> chain;
                    int start = edge.first;
                    int curr = edge.second;
                    chain.push_back(current_tf.transform(Point_3(geo.vertices[start].x, geo.vertices[start].y, geo.vertices[start].z)));
                    chain.push_back(current_tf.transform(Point_3(geo.vertices[curr].x, geo.vertices[curr].y, geo.vertices[curr].z)));

                    while (true) {
                        int next = -1;
                        for (int neighbor : adj[curr]) {
                            int e_idx = get_edge_idx(curr, neighbor);
                            if (e_idx != -1) { next = neighbor; break; }
                        }
                        if (next == -1) break;
                        
                        int e_idx = get_edge_idx(curr, next);
                        remaining_edges.erase(remaining_edges.begin() + e_idx);
                        chain.push_back(current_tf.transform(Point_3(geo.vertices[next].x, geo.vertices[next].y, geo.vertices[next].z)));
                        curr = next;
                    }
                    if (chain.size() >= 2) paths.push_back(chain);
                }
            } else if (!geo.vertices.empty()) {
                // Isolated points (ignored by sweep logic usually, but collected anyway)
                std::vector<Point_3> chain;
                for (const auto& v : geo.vertices) {
                    chain.push_back(current_tf.transform(Point_3(v.x, v.y, v.z)));
                }
                paths.push_back(chain);
            }
        }
        for (const auto& child : s.components) {
            collect_paths(vfs, child, current_tf, paths);
        }
    }

    static void execute_sweep(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                             const Geometry& profile, const std::vector<std::vector<Point_3>>& paths,
                             bool closed_path, bool solid, const nlohmann::json& base_tags = {}) {
        if (paths.empty()) {
            vfs->write(fulfilling.with_output("$out"), Shape());
            return;
        }

        Geometry res;

        for (const auto& path : paths) {
            if (path.size() < 2) continue;
            
            std::vector<Frame> frames = generate_rmf(path, closed_path);

            // Generate Slices
            std::vector<std::vector<int>> grid;
            for (const auto& frame : frames) {
                std::vector<int> slice;
                for (const auto& v : profile.vertices) {
                    Point_3 lp(v.x, v.y, v.z);
                    Point_3 wp = frame.transform(lp);
                    slice.push_back((int)res.vertices.size());
                    res.vertices.push_back({wp.x(), wp.y(), wp.z()});
                }
                grid.push_back(slice);
            }

            auto bridge = [&](const std::vector<std::vector<int>>& path_grid, size_t i0, size_t i1) {
                // Bridge faces
                for (const auto& f : profile.faces) {
                    for (const auto& loop : f.loops) {
                        for (size_t j = 0; j < loop.size(); ++j) {
                            int v_sub0 = loop[j];
                            int v_sub1 = loop[(j + 1) % loop.size()];

                            int v00 = path_grid[i0][v_sub0];
                            int v01 = path_grid[i0][v_sub1];
                            int v10 = path_grid[i1][v_sub0];
                            int v11 = path_grid[i1][v_sub1];

                            if (solid) {
                                res.faces.push_back({{{v00, v01, v11, v10}}});
                            } else {
                                res.segments.push_back({v00, v10});
                                res.segments.push_back({v01, v11});
                                res.segments.push_back({v00, v01});
                            }
                        }
                    }
                }
                // Bridge segments (longitudinal)
                for (const auto& seg : profile.segments) {
                    res.segments.push_back({path_grid[i0][seg[0]], path_grid[i1][seg[0]]});
                    res.segments.push_back({path_grid[i0][seg[1]], path_grid[i1][seg[1]]});
                    if (!solid) {
                        res.segments.push_back({path_grid[i0][seg[0]], path_grid[i0][seg[1]]});
                    }
                }
            };

            for (size_t i = 0; i < grid.size() - 1; ++i) {
                bridge(grid, i, i + 1);
            }

            if (closed_path) {
                bridge(grid, grid.size() - 1, 0);
            } else if (solid) {
                // Caps
                for (const auto& f : profile.faces) {
                    Geometry::Face start_cap, end_cap;
                    for (const auto& loop : f.loops) {
                        std::vector<int> sl, el;
                        for (int idx : loop) sl.push_back(grid.front()[idx]);
                        for (int idx : loop) el.push_back(grid.back()[idx]);
                        std::reverse(sl.begin(), sl.end()); // Flip start cap
                        start_cap.loops.push_back(sl);
                        end_cap.loops.push_back(el);
                    }
                    res.faces.push_back(start_cap);
                    res.faces.push_back(end_cap);
                }
            }
        }

        Shape out;
        out.tags = base_tags;
        if (solid) {
            // Check if profile was a surface or just segments
            if (profile.faces.empty() && profile.triangles.empty()) {
                out.add_tag("type", "open");
            } else {
                out.add_tag("type", "closed");
            }
        } else {
            out.add_tag("type", "segments");
        }
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    struct Constructor {
        static constexpr const char* path = "jot/Sweep";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                           const std::vector<Shape>& profiles, const Shape& path_shape, 
                           bool closed_path = false, bool solid = true) {
            Geometry profile_geo;
            for (const auto& s : profiles) collect_profile(vfs, s, Matrix::identity(), profile_geo);
            
            std::vector<std::vector<Point_3>> paths;
            collect_paths(vfs, path_shape, Matrix::identity(), paths);
            
            execute_sweep(vfs, fulfilling, profile_geo, paths, closed_path, solid);
        }
        static std::vector<std::string> argument_keys() { return {"profiles", "path", "closed_path", "solid"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"arguments", {
                    {{"name", "profiles"}, {"type", "jot:shapes"}},
                    {{"name", "path"}, {"type", "jot:shape"}},
                    {{"name", "closed_path"}, {"type", "jot:boolean"}, {"default", false}},
                    {{"name", "solid"}, {"type", "jot:boolean"}, {"default", true}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    struct Method {
        static constexpr const char* path = "jot/sweep";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                           const Shape& in, const Shape& path_shape, 
                           bool closed_path = false, bool solid = true) {
            Geometry profile_geo;
            collect_profile(vfs, in, Matrix::identity(), profile_geo);
            
            std::vector<std::vector<Point_3>> paths;
            collect_paths(vfs, path_shape, Matrix::identity(), paths);
            
            execute_sweep(vfs, fulfilling, profile_geo, paths, closed_path, solid, in.tags);
        }
        static std::vector<std::string> argument_keys() { return {"$in", "path", "closed_path", "solid"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"arguments", {
                    {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                    {{"name", "path"}, {"type", "jot:shape"}},
                    {{"name", "closed_path"}, {"type", "jot:boolean"}, {"default", false}},
                    {{"name", "solid"}, {"type", "jot:boolean"}, {"default", true}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    struct SweepBy {
        static constexpr const char* path = "jot/sweepBy";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                           const Shape& in, const Shape& profile_shape, 
                           bool closed_path = false, bool solid = true) {
            // Subject is the PATH
            std::vector<std::vector<Point_3>> paths;
            collect_paths(vfs, in, Matrix::identity(), paths);
            
            // Tool is the PROFILE
            Geometry profile_geo;
            collect_profile(vfs, profile_shape, Matrix::identity(), profile_geo);
            
            execute_sweep(vfs, fulfilling, profile_geo, paths, closed_path, solid, in.tags);
        }
        static std::vector<std::string> argument_keys() { return {"$in", "profile", "closed_path", "solid"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Extrudes a profile along the subject path."},
                {"arguments", {
                    {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                    {{"name", "profile"}, {"type", "jot:shape"}},
                    {{"name", "closed_path"}, {"type", "jot:boolean"}, {"default", false}},
                    {{"name", "solid"}, {"type", "jot:boolean"}, {"default", true}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };
};

static void sweep_init(fs::VFSNode* vfs) {
    Processor::register_op<SweepOp<>::Constructor, std::vector<Shape>, Shape, bool, bool>(vfs, "jot/Sweep");
    Processor::register_op<SweepOp<>::Method, Shape, Shape, bool, bool>(vfs, "jot/sweep");
    Processor::register_op<SweepOp<>::SweepBy, Shape, Shape, bool, bool>(vfs, "jot/sweepBy");
}

} // namespace geo
} // namespace jotcad
