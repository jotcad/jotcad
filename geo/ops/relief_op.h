#pragma once
#include "protocols.h"
#include "processor.h"
#include "../../fs/cpp/vendor/stb_image.h"
#include "../render/contour_utils.h"
#include "../render/triangulation.h"
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ReliefOp : P {
    static constexpr const char* path = "jot/relief";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const nlohmann::json& image_identity, double width, double height, double depth, double base, int levels, double minArea, double smooth, bool close = true) {
        try {
            std::cout << "[Relief] Loading image bytes..." << std::endl;
            fs::Selector img_sel = image_identity.get<fs::Selector>();
            if (img_sel.output.empty()) img_sel = img_sel.with_output("$out");
            std::vector<uint8_t> img_bytes = vfs->read<std::vector<uint8_t>>(img_sel);
            if (img_bytes.empty()) throw std::runtime_error("Relief: Image data empty");

            int W, H, channels;
            unsigned char* data = stbi_load_from_memory(img_bytes.data(), (int)img_bytes.size(), &W, &H, &channels, 3);
            if (!data) throw std::runtime_error("Relief: stbi_load failed");

            std::cout << "[Relief] Processing image of size " << W << "x" << H << "..." << std::endl;
            std::vector<double> intensity_data(W * H);
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    int idx = (x + y * W) * 3;
                    double r = data[idx] / 255.0;
                    double g = data[idx+1] / 255.0;
                    double b = data[idx+2] / 255.0;
                    intensity_data[y * W + x] = 0.299 * r + 0.587 * g + 0.114 * b;
                }
            }

            // 1. Uniform Quantization
            std::vector<int> labels(W * H);
            for (int i = 0; i < W * H; ++i) {
                labels[i] = std::clamp((int)(intensity_data[i] * (levels - 1) + 0.5), 0, levels - 1);
            }

            // 2. Median Filter Despeckle (3x3)
            std::vector<int> clean_labels = labels;
            for (int y = 1; y < H - 1; ++y) {
                for (int x = 1; x < W - 1; ++x) {
                    std::map<int, int> neighbors;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) neighbors[labels[(y+dy)*W + (x+dx)]]++;
                    }
                    int best = labels[y*W+x], max_c = 0;
                    for (auto const& [id, count] : neighbors) if (count > max_c) { max_c = count; best = id; }
                    if (max_c >= 5) clean_labels[y*W + x] = best;
                }
            }

            // 3. BFS-based Component Merging
            bool changed = true;
            int passes = 0;
            while (changed && passes < 5) {
                changed = false;
                passes++;
                std::vector<bool> visited(W * H, false);
                for (int y = 0; y < H; ++y) {
                    for (int x = 0; x < W; ++x) {
                        int start_idx = y * W + x;
                        if (visited[start_idx]) continue;

                        int target_label = clean_labels[start_idx];
                        std::vector<int> component;
                        std::vector<int> queue;
                        queue.push_back(start_idx);
                        visited[start_idx] = true;

                        size_t q_head = 0;
                        while (q_head < queue.size()) {
                            int idx = queue[q_head++];
                            component.push_back(idx);

                            int cx = idx % W;
                            int cy = idx / W;

                            int dx[4] = {-1, 1, 0, 0};
                            int dy[4] = {0, 0, -1, 1};
                            for (int dir = 0; dir < 4; ++dir) {
                                int nx = cx + dx[dir];
                                int ny = cy + dy[dir];
                                if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                                    int nidx = ny * W + nx;
                                    if (clean_labels[nidx] == target_label && !visited[nidx]) {
                                        visited[nidx] = true;
                                        queue.push_back(nidx);
                                    }
                                }
                            }
                        }

                        if (component.size() < (size_t)minArea) {
                            std::set<int> neighbor_labels;
                            for (int idx : component) {
                                int cx = idx % W;
                                int cy = idx / W;
                                int dx[4] = {-1, 1, 0, 0};
                                int dy[4] = {0, 0, -1, 1};
                                for (int dir = 0; dir < 4; ++dir) {
                                    int nx = cx + dx[dir];
                                    int ny = cy + dy[dir];
                                    if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                                        int nidx = ny * W + nx;
                                        if (clean_labels[nidx] != target_label) {
                                            neighbor_labels.insert(clean_labels[nidx]);
                                        }
                                    }
                                }
                            }

                            if (!neighbor_labels.empty()) {
                                int best_neighbor = -1;
                                int min_dist = 999999;
                                for (int n_lbl : neighbor_labels) {
                                    int d = std::abs(target_label - n_lbl);
                                    if (d < min_dist) {
                                        min_dist = d;
                                        best_neighbor = n_lbl;
                                    }
                                }

                                if (best_neighbor != -1) {
                                    for (int idx : component) {
                                        clean_labels[idx] = best_neighbor;
                                    }
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }

            stbi_image_free(data);

            // 4. Connected Component Partitioning & Boundary Injection
            std::vector<int> comp_ids(W * H, -1);
            int comp_count = 0;
            std::vector<int> comp_levels;
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    int idx = y * W + x;
                    if (comp_ids[idx] != -1) continue;

                    int lvl = clean_labels[idx];
                    int comp_id = comp_count++;
                    comp_levels.push_back(lvl);

                    std::vector<int> queue;
                    queue.push_back(idx);
                    comp_ids[idx] = comp_id;
                    size_t q_head = 0;
                    while (q_head < queue.size()) {
                        int curr = queue[q_head++];
                        int cx = curr % W;
                        int cy = curr / W;
                        int dx[4] = {-1, 1, 0, 0};
                        int dy[4] = {0, 0, -1, 1};
                        for (int dir = 0; dir < 4; ++dir) {
                            int nx = cx + dx[dir];
                            int ny = cy + dy[dir];
                            if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                                int nidx = ny * W + nx;
                                if (clean_labels[nidx] == lvl && comp_ids[nidx] == -1) {
                                    comp_ids[nidx] = comp_id;
                                    queue.push_back(nidx);
                                }
                            }
                        }
                    }
                }
            }

            int border_comp_id = comp_count++;
            comp_levels.push_back(-1);

            int p_w = W + 2, p_h = H + 2;
            std::vector<int> padded(p_w * p_h, border_comp_id);
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    padded[(y+1)*p_w + (x+1)] = comp_ids[y*W + x];
                }
            }

            // 5. Trace segments and weld loops for each component
            std::vector<int> targets(comp_count);
            for (int i = 0; i < comp_count; ++i) targets[i] = i;
            auto comp_segments = ContourUtils::generate_marching_triangles_segments(padded, p_w, p_h, H, targets);

            std::vector<std::vector<Polygon>> comp_loops(comp_count);
            for (int i = 0; i < comp_count; ++i) {
                comp_loops[i] = ContourUtils::weld_segments(comp_segments[i], smooth, 0.1, false);
            }

            // 6. Geometry generation setup
            struct VertexKey {
                double x, y, z;
                bool operator<(const VertexKey& o) const {
                    if (x != o.x) return x < o.x;
                    if (y != o.y) return y < o.y;
                    return z < o.z;
                }
            };
            std::map<VertexKey, int> vertex_map;
            std::vector<Vertex> unique_vertices;
            std::vector<Vec3> pts_double;
            std::vector<std::array<int, 3>> triangles;

            auto add_vertex = [&](double px, double py, double pz) {
                double sx = std::round(px * 1000000.0) / 1000000.0;
                double sy = std::round(py * 1000000.0) / 1000000.0;
                double sz = std::round(pz * 1000000.0) / 1000000.0;
                VertexKey key{sx, sy, sz};
                auto it = vertex_map.find(key);
                if (it != vertex_map.end()) return it->second;
                int idx = unique_vertices.size();
                unique_vertices.push_back(Vertex{FT(sx), FT(sy), FT(sz)});
                pts_double.push_back(Vec3{sx, sy, sz});
                vertex_map[key] = idx;
                return idx;
            };

            auto to_phys_x = [&](double px_pixel) { return -width / 2.0 + (px_pixel / W) * width; };
            auto to_phys_y = [&](double py_pixel) { return -height / 2.0 + (py_pixel / H) * height; };
            auto get_z = [&](int L) { return base + depth * (double)L / (levels - 1); };

            double z_base = base - 1.0;

            // A. Top Horizontal Faces (for each component except border)
            for (int i = 0; i < comp_count - 1; ++i) {
                const auto& polys = comp_loops[i];
                if (polys.empty()) continue;

                int c = comp_levels[i];
                double z = get_z(c);

                // Find the outer boundary (longest loop)
                size_t longest_idx = 0;
                size_t max_size = 0;
                for (size_t j = 0; j < polys.size(); ++j) {
                    if (polys[j].size() > max_size) {
                        max_size = polys[j].size();
                        longest_idx = j;
                    }
                }

                std::vector<Polygon> face_polys;
                Polygon outer = polys[longest_idx];
                if (outer.is_clockwise_oriented()) outer.reverse_orientation();
                face_polys.push_back(outer);

                for (size_t j = 0; j < polys.size(); ++j) {
                    if (j != longest_idx) {
                        Polygon hole = polys[j];
                        if (hole.is_counterclockwise_oriented()) hole.reverse_orientation();
                        face_polys.push_back(hole);
                    }
                }

                // Add vertices to unique_vertices and record their indices
                std::vector<std::vector<int>> face_loops_indices;
                for (const auto& poly : face_polys) {
                    std::vector<int> loop_indices;
                    for (auto it = poly.vertices_begin(); it != poly.vertices_end(); ++it) {
                        double px = to_phys_x(CGAL::to_double(it->x()));
                        double py = to_phys_y(CGAL::to_double(it->y()));
                        loop_indices.push_back(add_vertex(px, py, z));
                    }
                    face_loops_indices.push_back(std::move(loop_indices));
                }

                // Triangulate directly using the 2D polygon coordinates
                CDT cdt;
                std::map<CDT::Vertex_handle, int> v_map;
                for (size_t l = 0; l < face_polys.size(); ++l) {
                    const auto& poly = face_polys[l];
                    const auto& indices = face_loops_indices[l];
                    std::vector<CDT::Vertex_handle> vh;
                    for (size_t idx = 0; idx < poly.size(); ++idx) {
                        vh.push_back(cdt.insert(poly[idx]));
                        v_map[vh.back()] = indices[idx];
                    }
                    if (vh.size() >= 2) {
                        for (size_t idx = 0; idx < vh.size(); ++idx) {
                            cdt.insert_constraint(vh[idx], vh[(idx + 1) % vh.size()]);
                        }
                    }
                }
                CGAL::mark_domain_in_triangulation(cdt);
                for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
                    if (fit->info().in_domain) {
                        triangles.push_back({v_map[fit->vertex(0)], v_map[fit->vertex(1)], v_map[fit->vertex(2)]});
                    }
                }
            }

            // B. Single Bottom Cap (for the border component)
            if (close) {
                const auto& polys = comp_loops[border_comp_id];
                if (!polys.empty()) {
                    // Find the outer boundary (longest loop)
                    size_t longest_idx = 0;
                    size_t max_size = 0;
                    for (size_t j = 0; j < polys.size(); ++j) {
                        if (polys[j].size() > max_size) {
                            max_size = polys[j].size();
                            longest_idx = j;
                        }
                    }

                    std::vector<Polygon> face_polys;
                    Polygon outer = polys[longest_idx];
                    if (outer.is_clockwise_oriented()) outer.reverse_orientation();
                    face_polys.push_back(outer);

                    for (size_t j = 0; j < polys.size(); ++j) {
                        if (j != longest_idx) {
                            Polygon hole = polys[j];
                            if (hole.is_counterclockwise_oriented()) hole.reverse_orientation();
                            face_polys.push_back(hole);
                        }
                    }

                    // Add vertices to unique_vertices and record their indices
                    std::vector<std::vector<int>> face_loops_indices;
                    for (const auto& poly : face_polys) {
                        std::vector<int> loop_indices;
                        for (auto it = poly.vertices_begin(); it != poly.vertices_end(); ++it) {
                            double px = to_phys_x(CGAL::to_double(it->x()));
                            double py = to_phys_y(CGAL::to_double(it->y()));
                            loop_indices.push_back(add_vertex(px, py, z_base));
                        }
                        face_loops_indices.push_back(std::move(loop_indices));
                    }

                    // Triangulate directly using the 2D polygon coordinates
                    CDT cdt;
                    std::map<CDT::Vertex_handle, int> v_map;
                    for (size_t l = 0; l < face_polys.size(); ++l) {
                        const auto& poly = face_polys[l];
                        const auto& indices = face_loops_indices[l];
                        std::vector<CDT::Vertex_handle> vh;
                        for (size_t idx = 0; idx < poly.size(); ++idx) {
                            vh.push_back(cdt.insert(poly[idx]));
                            v_map[vh.back()] = indices[idx];
                        }
                        if (vh.size() >= 2) {
                            for (size_t idx = 0; idx < vh.size(); ++idx) {
                                cdt.insert_constraint(vh[idx], vh[(idx + 1) % vh.size()]);
                            }
                        }
                    }
                    CGAL::mark_domain_in_triangulation(cdt);
                    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
                        if (fit->info().in_domain) {
                            triangles.push_back({v_map[fit->vertex(0)], v_map[fit->vertex(2)], v_map[fit->vertex(1)]}); // Oriented downwards
                        }
                    }
                }
            }

            // C. Vertical Walls
            std::set<std::pair<std::pair<long long, long long>, std::pair<long long, long long>>> built_walls;

            auto to_key_point = [](double x, double y) {
                return std::make_pair(
                    (long long)std::round(x * 1000000),
                    (long long)std::round(y * 1000000)
                );
            };

            std::set<int> unique_levels(clean_labels.begin(), clean_labels.end());
            unique_levels.insert(-1);
            std::vector<int> existing_levels(unique_levels.begin(), unique_levels.end());
            int k = existing_levels.size();

            std::vector<int> level_to_idx(levels + 1, -1);
            for (int i = 0; i < k; ++i) {
                level_to_idx[existing_levels[i] + 1] = i;
            }

            for (int y = 0; y < p_h - 1; ++y) {
                for (int x = 0; x < p_w - 1; ++x) {
                    int v0_lbl = padded[y*p_w+x];
                    int v1_lbl = padded[y*p_w+(x+1)];
                    int v2_lbl = padded[(y+1)*p_w+(x+1)];
                    int v3_lbl = padded[(y+1)*p_w+x];

                    int cell_colors[4];
                    int num_colors = 0;
                    auto add_unique_color = [&](int lbl) {
                        if (lbl == border_comp_id) return;
                        for (int j = 0; j < num_colors; ++j) {
                            if (cell_colors[j] == lbl) return;
                        }
                        cell_colors[num_colors++] = lbl;
                    };
                    add_unique_color(v0_lbl);
                    add_unique_color(v1_lbl);
                    add_unique_color(v2_lbl);
                    add_unique_color(v3_lbl);

                    for (int color_idx = 0; color_idx < num_colors; ++color_idx) {
                         int c = cell_colors[color_idx];

                         FT fx(x-1), fy(H - (y-1)), h = FT(1)/2;
                         EK::Point_2 e0(fx+h, fy);
                         EK::Point_2 e1(fx+1, fy-h);
                         EK::Point_2 e2(fx+h, fy-1);
                         EK::Point_2 e3(fx, fy-h);
                         EK::Point_2 d0(fx+h, fy-h);

                        auto add_wall = [&](EK::Point_2 p1, EK::Point_2 p2, int neighbor_label, bool normal_right) {
                            double px1 = to_phys_x(CGAL::to_double(p1.x()));
                            double py1 = to_phys_y(CGAL::to_double(p1.y()));
                            double px2 = to_phys_x(CGAL::to_double(p2.x()));
                            double py2 = to_phys_y(CGAL::to_double(p2.y()));

                            auto k1 = to_key_point(px1, py1);
                            auto k2 = to_key_point(px2, py2);
                            auto key = std::make_pair(std::min(k1, k2), std::max(k1, k2));
                            if (built_walls.count(key)) return;
                            built_walls.insert(key);

                            int idx_start = level_to_idx[comp_levels[c] + 1];
                            int idx_end = level_to_idx[comp_levels[neighbor_label] + 1];
                            if (idx_start == -1 || idx_end == -1) return;

                            int i_min = std::min(idx_start, idx_end);
                            int i_max = std::max(idx_start, idx_end);

                            for (int idx = i_min; idx < i_max; ++idx) {
                                int l1 = existing_levels[idx];
                                int l2 = existing_levels[idx + 1];

                                double h1 = (l1 == -1) ? z_base : get_z(l1);
                                double h2 = (l2 == -1) ? z_base : get_z(l2);

                                if (std::abs(h1 - h2) < 1e-9) continue;

                                double hz1 = std::min(h1, h2);
                                double hz2 = std::max(h1, h2);

                                int b1 = add_vertex(px1, py1, hz1);
                                int b2 = add_vertex(px2, py2, hz1);
                                int a1 = add_vertex(px1, py1, hz2);
                                int a2 = add_vertex(px2, py2, hz2);

                                bool point_right = (c > l1) ? normal_right : !normal_right;

                                if (point_right) {
                                    triangles.push_back({b1, b2, a1});
                                    triangles.push_back({a1, b2, a2});
                                } else {
                                    triangles.push_back({b1, a1, b2});
                                    triangles.push_back({a1, a2, b2});
                                }
                            }
                        };

                        // Triangle 1: v0_lbl, v1_lbl, v3_lbl
                        bool v0 = (v0_lbl == c);
                        bool v1 = (v1_lbl == c);
                        bool v3 = (v3_lbl == c);
                        if (v0) {
                            if (!v1 && !v3) {
                                if (v1_lbl == v3_lbl) {
                                    add_wall(e0, e3, v1_lbl, true);
                                } else {
                                    add_wall(e0, d0, v1_lbl, true);
                                    add_wall(d0, e3, v3_lbl, true);
                                }
                            } else if (v1 && !v3) {
                                add_wall(d0, e3, v3_lbl, true);
                            } else if (v3 && !v1) {
                                add_wall(e0, d0, v1_lbl, true);
                            }
                        } else {
                            if (v1 && v3) {
                                add_wall(e0, e3, v0_lbl, true);
                            } else if (v1 && !v3) {
                                add_wall(e0, d0, v0_lbl, true);
                            } else if (v3 && !v1) {
                                add_wall(d0, e3, v0_lbl, true);
                            }
                        }

                        // Triangle 2: v1_lbl, v2_lbl, v3_lbl
                        bool tv1 = (v1_lbl == c);
                        bool tv2 = (v2_lbl == c);
                        bool tv3 = (v3_lbl == c);
                        if (tv1) {
                            if (!tv2 && !tv3) {
                                if (v2_lbl == v3_lbl) {
                                    add_wall(e1, d0, v2_lbl, true);
                                } else {
                                    add_wall(e1, e2, v2_lbl, true);
                                    add_wall(e2, d0, v3_lbl, true);
                                }
                            } else if (tv2 && !tv3) {
                                add_wall(e2, d0, v3_lbl, true);
                            } else if (tv3 && !tv2) {
                                add_wall(e1, e2, v2_lbl, true);
                            }
                        } else {
                            if (tv2 && tv3) {
                                add_wall(e1, d0, v1_lbl, true);
                            } else if (tv2 && !tv3) {
                                add_wall(e1, e2, v1_lbl, true);
                            } else if (tv3 && !tv2) {
                                add_wall(e2, d0, v1_lbl, true);
                            }
                        }
                    }
                }
            }

            Geometry geo;
            geo.vertices = std::move(unique_vertices);
            geo.triangles = std::move(triangles);

            vfs->write(fulfilling.with_output("$out"), P::make_shape(vfs, geo, {}));
        } catch (const std::exception& e) {
            std::cerr << "[ReliefOp] Error: " << e.what() << std::endl;
            throw;
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "width", "height", "depth", "base", "levels", "minArea", "smooth", "close"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/relief"},
            {"inputs", {
                {"$in", {{"type", "jot:image"}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "width"}, {"type", "jot:number"}, {"default", 10.0}},
                {{"name", "height"}, {"type", "jot:number"}, {"default", 10.0}},
                {{"name", "depth"}, {"type", "jot:number"}, {"default", 2.0}},
                {{"name", "base"}, {"type", "jot:number"}, {"default", 1.0}},
                {{"name", "levels"}, {"type", "jot:number"}, {"default", 16}},
                {{"name", "minArea"}, {"type", "jot:number"}, {"default", 25.0}},
                {{"name", "smooth"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "close"}, {"type", "jot:boolean"}, {"default", true}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void relief_init(fs::VFSNode* vfs) {
    Processor::register_op<ReliefOp<>, nlohmann::json, double, double, double, double, int, double, double, bool>(vfs, "jot/relief");
}

} // namespace geo
} // namespace jotcad
