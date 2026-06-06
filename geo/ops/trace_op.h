#pragma once
#include "../core/protocols.h"
#include "../core/processor.h"
#include "../render/contour_utils.h"
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <map>
#include <random>
#include <chrono>
#include <set>
#include "../../fs/cpp/vendor/stb_image.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct TraceOp : P {
    static constexpr const char* path = "jot/trace";

    struct PixelHSV { double h, s, v; };
    struct PixelRGB { uint8_t r, g, b; };

    static PixelHSV rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b) {
        double rd = r / 255.0, gd = g / 255.0, bd = b / 255.0;
        double max_v = std::max({rd, gd, bd}), min_v = std::min({rd, gd, bd});
        double h, s, v = max_v, d = max_v - min_v;
        s = (max_v == 0) ? 0 : d / max_v;
        if (max_v == min_v) h = 0;
        else {
            if (max_v == rd) h = (gd - bd) / d + (gd < bd ? 6 : 0);
            else if (max_v == gd) h = (bd - rd) / d + 2;
            else h = (rd - gd) / d + 4;
            h /= 6.0;
        }
        return { h, s, v };
    }

    static double hsv_dist_sq(PixelHSV a, PixelHSV b) {
        double dh = std::min(std::abs(a.h - b.h), 1.0 - std::abs(a.h - b.h));
        double ds = a.s - b.s, dv = a.v - b.v;
        return dh*dh * 4.0 + ds*ds + dv*dv;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const nlohmann::json& image_identity, int colors, double smooth, double minArea) {
        auto start_total = std::chrono::steady_clock::now();
        try {
            std::cout << "[Trace] Loading image bytes..." << std::endl;
            fs::Selector img_sel = image_identity.get<fs::Selector>();
            if (img_sel.output.empty()) img_sel = img_sel.with_output("$out");
            std::vector<uint8_t> img_bytes = vfs->read<std::vector<uint8_t>>(img_sel);
            if (img_bytes.empty()) throw std::runtime_error("Image data empty");

            int width, height, channels;
            unsigned char* data = stbi_load_from_memory(img_bytes.data(), (int)img_bytes.size(), &width, &height, &channels, 3);
            if (!data) throw std::runtime_error("stbi_load failed");
            
            std::vector<PixelHSV> hsv_data(width * height);
            for (int i = 0; i < width * height; ++i) hsv_data[i] = rgb_to_hsv(data[i*3], data[i*3+1], data[i*3+2]);

            // 1. K-Means Quantization (HSV)
            std::cout << "[Trace] Quantizing to " << colors << " colors (HSV)..." << std::endl;
            std::vector<PixelHSV> centers(colors);
            std::mt19937 rng(42);
            for (int i = 0; i < colors; ++i) centers[i] = hsv_data[std::uniform_int_distribution<int>(0, width * height - 1)(rng)];

            std::vector<int> labels(width * height);
            for (int iter = 0; iter < 10; ++iter) {
                std::vector<double> sum_h(colors, 0), sum_s(colors, 0), sum_v(colors, 0);
                std::vector<int> counts(colors, 0);
                for (int i = 0; i < width * height; ++i) {
                    int best = 0; double min_d = 1e18;
                    for (int j = 0; j < colors; ++j) {
                        double d = hsv_dist_sq(hsv_data[i], centers[j]);
                        if (d < min_d) { min_d = d; best = j; }
                    }
                    labels[i] = best;
                    sum_h[best] += hsv_data[i].h; sum_s[best] += hsv_data[i].s; sum_v[best] += hsv_data[i].v;
                    counts[best]++;
                }
                for (int j = 0; j < colors; ++j) {
                    if (counts[j] > 0) centers[j] = {sum_h[j]/counts[j], sum_s[j]/counts[j], sum_v[j]/counts[j]};
                }
            }

            // 2. Median Filter Despeckle (3x3)
            std::cout << "[Trace] Despeckling labels..." << std::endl;
            std::vector<int> clean_labels = labels;
            for (int y = 1; y < height - 1; ++y) {
                for (int x = 1; x < width - 1; ++x) {
                    std::map<int, int> neighbors;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) neighbors[labels[(y+dy)*width + (x+dx)]]++;
                    }
                    int best = labels[y*width+x], max_c = 0;
                    for (auto const& [id, count] : neighbors) if (count > max_c) { max_c = count; best = id; }
                    if (max_c >= 5) clean_labels[y*width + x] = best;
                }
            }

            // 2b. Merge components smaller than minArea pixels with their closest color neighbor
            std::cout << "[Trace] Merging components smaller than " << minArea << " pixels..." << std::endl;
            bool changed = true;
            int passes = 0;
            while (changed && passes < 5) {
                changed = false;
                passes++;
                std::vector<bool> visited(width * height, false);
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        int start_idx = y * width + x;
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

                            int cx = idx % width;
                            int cy = idx / width;

                            int dx[4] = {-1, 1, 0, 0};
                            int dy[4] = {0, 0, -1, 1};
                            for (int dir = 0; dir < 4; ++dir) {
                                int nx = cx + dx[dir];
                                int ny = cy + dy[dir];
                                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                    int nidx = ny * width + nx;
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
                                int cx = idx % width;
                                int cy = idx / width;
                                int dx[4] = {-1, 1, 0, 0};
                                int dy[4] = {0, 0, -1, 1};
                                for (int dir = 0; dir < 4; ++dir) {
                                    int nx = cx + dx[dir];
                                    int ny = cy + dy[dir];
                                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                        int nidx = ny * width + nx;
                                        if (clean_labels[nidx] != target_label) {
                                            neighbor_labels.insert(clean_labels[nidx]);
                                        }
                                    }
                                }
                            }

                            if (!neighbor_labels.empty()) {
                                int best_neighbor = -1;
                                double min_dist = 1e18;
                                for (int n_lbl : neighbor_labels) {
                                    double d = hsv_dist_sq(centers[target_label], centers[n_lbl]);
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

            // 3. Connected Component Partitioning & Boundary Injection
            std::vector<int> comp_ids(width * height, -1);
            int comp_count = 0;
            std::vector<int> comp_colors;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int idx = y * width + x;
                    if (comp_ids[idx] != -1) continue;

                    int col = clean_labels[idx];
                    int comp_id = comp_count++;
                    comp_colors.push_back(col);

                    std::vector<int> queue;
                    queue.push_back(idx);
                    comp_ids[idx] = comp_id;
                    size_t q_head = 0;
                    while (q_head < queue.size()) {
                        int curr = queue[q_head++];
                        int cx = curr % width;
                        int cy = curr / width;
                        int dx[4] = {-1, 1, 0, 0};
                        int dy[4] = {0, 0, -1, 1};
                        for (int dir = 0; dir < 4; ++dir) {
                            int nx = cx + dx[dir];
                            int ny = cy + dy[dir];
                            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                int nidx = ny * width + nx;
                                if (clean_labels[nidx] == col && comp_ids[nidx] == -1) {
                                    comp_ids[nidx] = comp_id;
                                    queue.push_back(nidx);
                                }
                            }
                        }
                    }
                }
            }

            int border_comp_id = comp_count++;
            comp_colors.push_back(-1);

            int p_w = width + 2, p_h = height + 2;
            std::vector<int> padded(p_w * p_h, border_comp_id);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    padded[(y+1)*p_w + (x+1)] = comp_ids[y*width + x];
                }
            }

            std::cout << "[Trace] Vectorizing " << comp_count - 1 << " components..." << std::endl;
            std::vector<int> targets(comp_count - 1);
            for (int i = 0; i < comp_count - 1; ++i) targets[i] = i;
            auto comp_segments = ContourUtils::generate_marching_triangles_segments(padded, p_w, p_h, height, targets);

            // Weld loops for all components
            std::vector<std::vector<Polygon>> comp_loops(comp_count - 1);
            for (int i = 0; i < comp_count - 1; ++i) {
                comp_loops[i] = ContourUtils::weld_segments(comp_segments[i], smooth, 0.5);
            }

            std::vector<Shape> buckets;
            for (int c = 0; c < colors; ++c) {
                auto tc_start = std::chrono::steady_clock::now();
                Geometry geo;

                for (int i = 0; i < comp_count - 1; ++i) {
                    if (comp_colors[i] != c) continue;
                    const auto& polys = comp_loops[i];
                    if (polys.empty()) continue;

                    // Find the outer boundary (longest loop)
                    size_t longest_idx = 0;
                    size_t max_size = 0;
                    for (size_t j = 0; j < polys.size(); ++j) {
                        if (polys[j].size() > max_size) {
                            max_size = polys[j].size();
                            longest_idx = j;
                        }
                    }

                    Geometry::Face f;
                    auto add_l = [&](const Polygon& p) {
                        std::vector<int> l;
                        for (auto it = p.vertices_begin(); it != p.vertices_end(); ++it) {
                            l.push_back(geo.vertices.size());
                            geo.vertices.push_back({it->x(), it->y(), 0});
                        }
                        return l;
                    };
                    f.loops.push_back(add_l(polys[longest_idx]));
                    for (size_t j = 0; j < polys.size(); ++j) {
                        if (j != longest_idx) {
                            f.loops.push_back(add_l(polys[j]));
                        }
                    }
                    geo.faces.push_back(f);
                }

                long long r=0, g=0, b=0, count=0;
                for(int i=0; i<width*height; ++i) if(clean_labels[i]==c) { r+=data[i*3]; g+=data[i*3+1]; b+=data[i*3+2]; count++; }
                char hex[8]; 
                if (count > 0) snprintf(hex, 8, "#%02x%02x%02x", (int)(r/count), (int)(g/count), (int)(b/count));
                else snprintf(hex, 8, "#000000");

                buckets.push_back(P::make_shape(vfs, geo, {{"color", hex}}));
                auto tc_end = std::chrono::steady_clock::now();
                std::cout << "  - bucket " << c << " (" << hex << "): Total=" 
                          << std::chrono::duration_cast<std::chrono::milliseconds>(tc_end - tc_start).count() << "ms" << std::endl;
            }
            stbi_image_free(data);
            vfs->write(fulfilling.with_output("$out"), Shape::group(buckets));
            auto end_total = std::chrono::steady_clock::now();
            std::cout << "[Trace] Total Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_total - start_total).count() << "ms" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[TraceOp] Error: " << e.what() << std::endl;
            throw;
        }
    }
    static std::vector<std::string> argument_keys() { return {"$in", "colors", "smooth", "minArea"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/trace"},
            {"inputs", {{"$in", {{"type", "jot:image"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "colors"}, {"type", "jot:number"}, {"default", 12}},
                {{"name", "smooth"}, {"type", "jot:number"}, {"default", 0.0}},
                {{"name", "minArea"}, {"type", "jot:number"}, {"default", 25.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void trace_init(fs::VFSNode* vfs) {
    Processor::register_op<TraceOp<>, nlohmann::json, int, double, double>(vfs, "jot/trace");
}

} // namespace geo
} // namespace jotcad
