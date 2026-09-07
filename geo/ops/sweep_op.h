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
    static void collect_profile(fs::VFSNode* vfs, const Shape& s, Geometry& combined) {
        for (const auto& node : s.shapes()) {
            if (!node.has_positive_geometry()) continue;
            Geometry geo = vfs->read<Geometry>(node.geometry.value());
            std::map<int, int> v_map;
            for (size_t i = 0; i < geo.vertices.size(); ++i) {
                Point_3 p(geo.vertices[i].x, geo.vertices[i].y, geo.vertices[i].z);
                Point_3 tp = node.tf.transform(p);
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
    }

    static void collect_paths(fs::VFSNode* vfs, const Shape& s, std::vector<std::vector<Point_3>>& paths) {
        for (const auto& node : s.shapes()) {
            if (!node.has_positive_geometry()) continue;
            Geometry geo = vfs->read<Geometry>(node.geometry.value());
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
                            chain.push_back(node.tf.transform(Point_3(geo.vertices[start_node].x, geo.vertices[start_node].y, geo.vertices[start_node].z)));
                            
                            int curr = start_node;
                            int next = neighbor;

                            while (true) {
                                int e_idx = get_edge_idx(curr, next);
                                if (e_idx == -1) break;
                                remaining_edges.erase(remaining_edges.begin() + e_idx);
                                
                                chain.push_back(node.tf.transform(Point_3(geo.vertices[next].x, geo.vertices[next].y, geo.vertices[next].z)));
                                
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
                    chain.push_back(node.tf.transform(Point_3(geo.vertices[start].x, geo.vertices[start].y, geo.vertices[start].z)));
                    chain.push_back(node.tf.transform(Point_3(geo.vertices[curr].x, geo.vertices[curr].y, geo.vertices[curr].z)));

                    while (true) {
                        int next = -1;
                        for (int neighbor : adj[curr]) {
                            int e_idx = get_edge_idx(curr, neighbor);
                            if (e_idx != -1) { next = neighbor; break; }
                        }
                        if (next == -1) break;
                        
                        int e_idx = get_edge_idx(curr, next);
                        remaining_edges.erase(remaining_edges.begin() + e_idx);
                        chain.push_back(node.tf.transform(Point_3(geo.vertices[next].x, geo.vertices[next].y, geo.vertices[next].z)));
                        curr = next;
                    }
                    if (chain.size() >= 2) paths.push_back(chain);
                }
            } else if (!geo.vertices.empty()) {
                // Isolated points (ignored by sweep logic usually, but collected anyway)
                std::vector<Point_3> chain;
                for (const auto& v : geo.vertices) {
                    chain.push_back(node.tf.transform(Point_3(v.x, v.y, v.z)));
                }
                paths.push_back(chain);
            }
        }
    }

    static void execute_sweep(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                             const Geometry& profile, const std::vector<std::vector<Point_3>>& paths, 
                             bool closed_path = false, bool solid = true, double radius = 1.0, double zag = 0.05,
                             const nlohmann::json& base_tags = {}) {
        if (paths.empty() || profile.vertices.empty()) {
            vfs->write(fulfilling.with_output("$out"), Shape());
            return;
        }

        // 1. Profile Bounding Box & Inner/Outer Extents in local (x, y) plane
        double x_min = 1e9, x_max = -1e9;
        for (const auto& v : profile.vertices) {
            double vx = CGAL::to_double(v.x);
            if (vx < x_min) x_min = vx;
            if (vx > x_max) x_max = vx;
        }
        double profile_width = (x_max > x_min) ? (x_max - x_min) : 1.0;

        Geometry res;

        auto bridge = [&](const std::vector<std::vector<int>>& path_grid, int i0, int i1) {
            if (i0 == i1) return;
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
                            Point_3 p00(res.vertices[v00].x, res.vertices[v00].y, res.vertices[v00].z);
                            Point_3 p01(res.vertices[v01].x, res.vertices[v01].y, res.vertices[v01].z);
                            Point_3 p10(res.vertices[v10].x, res.vertices[v10].y, res.vertices[v10].z);
                            Point_3 p11(res.vertices[v11].x, res.vertices[v11].y, res.vertices[v11].z);

                            double d_diag_A = CGAL::to_double((p00 - p11).squared_length());
                            double d_diag_B = CGAL::to_double((p01 - p10).squared_length());

                            if (d_diag_A <= d_diag_B) {
                                res.faces.push_back({{{v00, v10, v11}}});
                                res.faces.push_back({{{v00, v11, v01}}});
                            } else {
                                res.faces.push_back({{{v01, v00, v10}}});
                                res.faces.push_back({{{v01, v10, v11}}});
                            }
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

        for (const auto& raw_path : paths) {
            if (raw_path.size() < 2) continue;

            std::vector<Point_3> path = raw_path;
            bool is_closed = closed_path;
            if (path.size() >= 3 && (path.front() - path.back()).squared_length() < 1e-9) {
                is_closed = true;
                path.pop_back();
            }
            size_t M = path.size();
            if (M < 2) continue;

            // Compute Segment Tangents, Lengths, and Reference Frames
            size_t num_segs = is_closed ? M : (M - 1);
            std::vector<Vector_3> seg_t(num_segs);
            std::vector<Vector_3> seg_n(num_segs);
            std::vector<Vector_3> seg_b(num_segs);
            std::vector<double> seg_l(num_segs);

            Vector_3 global_up(0, 0, 1);

            for (size_t i = 0; i < num_segs; ++i) {
                size_t next_v = (i + 1) % M;
                Vector_3 v = path[next_v] - path[i];
                double len = std::sqrt(CGAL::to_double(v.squared_length()));
                seg_l[i] = len;
                Vector_3 t = (len > 1e-9) ? (v / len) : Vector_3(1, 0, 0);
                seg_t[i] = t;

                Vector_3 u = global_up;
                if (std::abs(CGAL::to_double(CGAL::scalar_product(t, u))) > 0.95) {
                    u = Vector_3(0, 1, 0);
                }
                Vector_3 n = CGAL::cross_product(t, u);
                double n_len = std::sqrt(CGAL::to_double(n.squared_length()));
                if (n_len > 1e-9) n = n / n_len;
                Vector_3 b = CGAL::cross_product(n, t);
                double b_len = std::sqrt(CGAL::to_double(b.squared_length()));
                if (b_len > 1e-9) b = b / b_len;

                seg_n[i] = n;
                seg_b[i] = b;
            }

            // Compute Corner Turn Angle, Setbacks, and Pivots
            size_t num_corners = is_closed ? M : (M > 2 ? M - 2 : 0);
            std::vector<bool> has_corner(M, false);
            std::vector<double> corner_theta(M, 0.0);
            std::vector<double> corner_setback(M, 0.0);
            std::vector<double> corner_radius(M, 0.0);
            std::vector<double> corner_sign(M, 1.0);
            std::vector<Vector_3> corner_axis(M);
            std::vector<Vector_3> corner_n_turn(M);

            for (size_t i = 0; i < M; ++i) {
                if (!is_closed && (i == 0 || i == M - 1)) continue;
                size_t prev_seg = (i == 0) ? (num_segs - 1) : (i - 1);
                size_t next_seg = i;

                Vector_3 t_in = seg_t[prev_seg];
                Vector_3 t_out = seg_t[next_seg];

                double dot_val = CGAL::to_double(CGAL::scalar_product(t_in, t_out));
                if (dot_val > 1.0) dot_val = 1.0;
                if (dot_val < -1.0) dot_val = -1.0;
                double theta = std::acos(dot_val);

                if (theta > 1e-3 && theta < (M_PI - 1e-3)) {
                    Vector_3 ax = CGAL::cross_product(t_in, t_out);
                    double ax_len = std::sqrt(CGAL::to_double(ax.squared_length()));
                    if (ax_len > 1e-9) {
                        ax = ax / ax_len;
                        double sign = (CGAL::to_double(CGAL::scalar_product(global_up, ax)) >= 0) ? 1.0 : -1.0;
                        Vector_3 n_turn = -sign * seg_n[prev_seg]; // Normal pointing strictly toward inside of turn

                        // Profile extreme offsets along inside and outside turn normals
                        double d_inner = 0.0;
                        double d_outer = 0.0;
                        for (const auto& v : profile.vertices) {
                            double px = CGAL::to_double(v.x);
                            double off_in = -sign * px;
                            double off_out = sign * px;
                            if (off_in > d_inner) d_inner = off_in;
                            if (off_out > d_outer) d_outer = off_out;
                        }

                        double r_turn = d_inner + std::max(0.0, radius);
                        if (r_turn < 0.01) r_turn = 0.01;
                        double setback = r_turn * std::tan(theta / 2.0);

                        has_corner[i] = true;
                        corner_theta[i] = theta;
                        corner_setback[i] = setback;
                        corner_radius[i] = r_turn;
                        corner_sign[i] = sign;
                        corner_axis[i] = ax;
                        corner_n_turn[i] = n_turn;
                    }
                }
            }

            // Validate Feasibility on all segments across all profile tracks
            for (size_t i = 0; i < num_segs; ++i) {
                size_t v0 = i;
                size_t v1 = (i + 1) % M;
                if (!has_corner[v0] && !has_corner[v1]) continue;

                double max_required_sb = 0.0;
                for (const auto& v : profile.vertices) {
                    double px = CGAL::to_double(v.x);
                    double sb0 = 0.0, sb1 = 0.0;
                    if (has_corner[v0]) {
                        double r0 = corner_radius[v0] + corner_sign[v0] * px;
                        if (r0 < 0.0) r0 = 0.0;
                        sb0 = r0 * std::tan(corner_theta[v0] / 2.0);
                    }
                    if (has_corner[v1]) {
                        double r1 = corner_radius[v1] - corner_sign[v1] * px;
                        if (r1 < 0.0) r1 = 0.0;
                        sb1 = r1 * std::tan(corner_theta[v1] / 2.0);
                    }
                    double total = sb0 + sb1;
                    if (total > max_required_sb) max_required_sb = total;
                }

                double seg_length = seg_l[i];
                if (seg_length > 1e-6 && max_required_sb > seg_length) {
                    std::stringstream ss;
                    ss << "SweepOp: Path segment " << i << " of length " << seg_length 
                       << "mm is too short for required corner setbacks (" << max_required_sb 
                       << "mm). Reduce turning radius, increase segment length, or use LinkCurve.";
                    throw std::runtime_error(ss.str());
                }
            }

            // Generate Path Slices
            std::vector<std::vector<int>> vertex_grid;

            auto add_frame_slice = [&](const Point_3& pos, const Vector_3& n, const Vector_3& b) -> int {
                std::vector<int> slice;
                slice.reserve(profile.vertices.size());
                for (const auto& v : profile.vertices) {
                    double px = CGAL::to_double(v.x);
                    double py = CGAL::to_double(v.y);
                    Point_3 wp = pos + px * n + py * b;
                    slice.push_back((int)res.vertices.size());
                    res.vertices.push_back({wp.x(), wp.y(), wp.z()});
                }
                vertex_grid.push_back(slice);
                return (int)vertex_grid.size() - 1;
            };

            for (size_t i = 0; i < num_segs; ++i) {
                size_t v_start = i;
                size_t v_end = (i + 1) % M;

                Point_3 p0 = path[v_start];
                Point_3 p1 = path[v_end];
                Vector_3 t = seg_t[i];
                Vector_3 n = seg_n[i];
                Vector_3 b = seg_b[i];

                double sb_start = has_corner[v_start] ? corner_setback[v_start] : 0.0;
                double sb_end = has_corner[v_end] ? corner_setback[v_end] : 0.0;

                Point_3 seg_start_pt = p0 + sb_start * t;
                Point_3 seg_end_pt = p1 - sb_end * t;

                // 1. Straight Segment Start Slice
                add_frame_slice(seg_start_pt, n, b);

                // 2. Straight Segment End Slice
                add_frame_slice(seg_end_pt, n, b);

                // 3. Corner Arc (if vertex v_end is a corner)
                if (has_corner[v_end]) {
                    double theta = corner_theta[v_end];
                    double r_turn = corner_radius[v_end];
                    double sign = corner_sign[v_end];
                    Vector_3 ax = corner_axis[v_end];
                    Vector_3 n_turn = corner_n_turn[v_end];

                    Point_3 p_pivot = seg_end_pt + r_turn * n_turn;

                    double r_outer = r_turn + profile_width;
                    double tol = (zag > 1e-4) ? zag : 0.05;
                    double dphi = 2.0 * std::acos(std::max(0.0, 1.0 - tol / r_outer));
                    if (dphi < 0.05) dphi = 0.05;
                    int num_arc_slices = (int)std::ceil(theta / dphi);
                    if (num_arc_slices < 4) num_arc_slices = 4;

                    for (int s = 1; s < num_arc_slices; ++s) {
                        double frac = (double)s / (double)num_arc_slices;
                        double angle = theta * frac;

                        std::vector<int> arc_slice;
                        arc_slice.reserve(profile.vertices.size());
                        for (const auto& v : profile.vertices) {
                            double px = CGAL::to_double(v.x);
                            double py = CGAL::to_double(v.y);
                            Point_3 wp_in = seg_end_pt + px * n + py * b;
                            Point_3 wp_rot = pivot_rotate(wp_in, p_pivot, ax, angle);
                            arc_slice.push_back((int)res.vertices.size());
                            res.vertices.push_back({wp_rot.x(), wp_rot.y(), wp_rot.z()});
                        }
                        vertex_grid.push_back(arc_slice);
                    }
                }
            }

            // Bridge all consecutive slices
            for (size_t i = 0; i < vertex_grid.size() - 1; ++i) {
                bridge(vertex_grid, (int)i, (int)i + 1);
            }

            if (is_closed && !vertex_grid.empty()) {
                bridge(vertex_grid, (int)vertex_grid.size() - 1, 0);
            } else if (solid && !vertex_grid.empty()) {
                // Caps for open path
                for (const auto& f : profile.faces) {
                    Geometry::Face start_cap, end_cap;
                    for (const auto& loop : f.loops) {
                        std::vector<int> sl, el;
                        for (int idx : loop) sl.push_back(vertex_grid.front()[idx]);
                        for (int idx : loop) el.push_back(vertex_grid.back()[idx]);
                        std::reverse(sl.begin(), sl.end());
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
                           bool closed_path = false, bool solid = true, double radius = 1.0, double zag = 0.05) {
            Geometry profile_geo;
            for (const auto& s : profiles) collect_profile(vfs, s, profile_geo);
            
            std::vector<std::vector<Point_3>> paths;
            collect_paths(vfs, path_shape, paths);
            
            execute_sweep(vfs, fulfilling, profile_geo, paths, closed_path, solid, radius, zag);
        }
        static std::vector<std::string> argument_keys() { return {"profiles", "path", "closed_path", "solid", "radius", "zag"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"inputs", nlohmann::json::object()},
                {"arguments", json::array({
                    {{"name", "profiles"}, {"type", "jot:shapes"}},
                    {{"name", "path"}, {"type", "jot:shape"}},
                    {{"name", "closed_path"}, {"type", "jot:boolean"}, {"default", false}},
                    {{"name", "solid"}, {"type", "jot:boolean"}, {"default", true}},
                    {{"name", "radius"}, {"type", "jot:number"}, {"default", 1.0}},
                    {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.05}}
                })},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    struct Method {
        static constexpr const char* path = "jot/sweep";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                           const Shape& in, const Shape& path_shape, 
                           bool closed_path = false, bool solid = true, double radius = 1.0, double zag = 0.05) {
            Geometry profile_geo;
            collect_profile(vfs, in, profile_geo);
            
            std::vector<std::vector<Point_3>> paths;
            collect_paths(vfs, path_shape, paths);
            
            execute_sweep(vfs, fulfilling, profile_geo, paths, closed_path, solid, radius, zag, in.tags);
        }
        static std::vector<std::string> argument_keys() { return {"$in", "path", "closed_path", "solid", "radius", "zag"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
                {"arguments", json::array({
                    {{"name", "path"}, {"type", "jot:shape"}},
                    {{"name", "closed_path"}, {"type", "jot:boolean"}, {"default", false}},
                    {{"name", "solid"}, {"type", "jot:boolean"}, {"default", true}},
                    {{"name", "radius"}, {"type", "jot:number"}, {"default", 1.0}},
                    {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.05}}
                })},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    struct SweepBy {
        static constexpr const char* path = "jot/sweepBy";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                           const Shape& in, const Shape& profile_shape, 
                           bool closed_path = false, bool solid = true, double radius = 1.0, double zag = 0.05) {
            // Subject is the PATH
            std::vector<std::vector<Point_3>> paths;
            collect_paths(vfs, in, paths);
            
            // Tool is the PROFILE
            Geometry profile_geo;
            collect_profile(vfs, profile_shape, profile_geo);
            
            execute_sweep(vfs, fulfilling, profile_geo, paths, closed_path, solid, radius, zag, in.tags);
        }
        static std::vector<std::string> argument_keys() { return {"$in", "profile", "closed_path", "solid", "radius", "zag"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Extrudes a profile along the subject path with controlled corner radius and chordal tolerance."},
                {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
                {"arguments", {
                    {{"name", "profile"}, {"type", "jot:shape"}},
                    {{"name", "closed_path"}, {"type", "jot:boolean"}, {"default", false}},
                    {{"name", "solid"}, {"type", "jot:boolean"}, {"default", true}},
                    {{"name", "radius"}, {"type", "jot:number"}, {"default", 1.0}},
                    {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.05}}
                }},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };
};

static void sweep_init(fs::VFSNode* vfs) {
    Processor::register_op<SweepOp<>::Constructor, std::vector<Shape>, Shape, bool, bool, double, double>(vfs, "jot/Sweep");
    Processor::register_op<SweepOp<>::Method, Shape, Shape, bool, bool, double, double>(vfs, "jot/sweep");
    Processor::register_op<SweepOp<>::SweepBy, Shape, Shape, bool, bool, double, double>(vfs, "jot/sweepBy");
}

} // namespace geo
} // namespace jotcad
