#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include <map>
#include <list>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct StitchOp : P {
    static constexpr const char* path = "jot/stitch";

    struct Path {
        std::vector<Vertex> points;
        bool is_closed = false;
    };

    struct PointKey {
        long long x, y, z;
        bool operator<(const PointKey& o) const {
            if (x != o.x) return x < o.x;
            if (y != o.y) return y < o.y;
            return z < o.z;
        }
    };

    static PointKey to_key(const Vertex& v) {
        return {(long long)std::round(CGAL::to_double(v.x) * 1000000),
                (long long)std::round(CGAL::to_double(v.y) * 1000000),
                (long long)std::round(CGAL::to_double(v.z) * 1000000)};
    }

    static void walk(fs::VFSNode* vfs, const Shape& shape, const Matrix& parent_tf, std::vector<std::pair<Vertex, Vertex>>& all_segs) {
        Matrix current_tf = parent_tf * shape.tf;
        if (shape.geometry.has_value()) {
            try {
                Geometry geo = vfs->read<Geometry>(shape.geometry.value());
                for (const auto& s : geo.segments) {
                    Vertex v1 = geo.vertices[s[0]];
                    Vertex v2 = geo.vertices[s[1]];
                    // Apply TF
                    auto apply = [&](Vertex v) {
                        EK::Point_3 p(v.x, v.y, v.z);
                        EK::Point_3 res = current_tf.transform(p);
                        return Vertex{res.x(), res.y(), res.z()};
                    };
                    all_segs.push_back({apply(v1), apply(v2)});
                }
            } catch (...) {}
        }
        for (const auto& child : shape.components) walk(vfs, child, current_tf, all_segs);
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& repeat, const std::vector<double>& start, const std::vector<double>& end, double offset) {
        if (repeat.empty() && start.empty() && end.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        std::vector<std::pair<Vertex, Vertex>> raw_segs;
        walk(vfs, in, Matrix::identity(), raw_segs);

        // 1. Chain Segments
        std::map<PointKey, std::vector<Vertex>> adj;
        for (const auto& s : raw_segs) {
            adj[to_key(s.first)].push_back(s.second);
            adj[to_key(s.second)].push_back(s.first);
        }

        std::vector<Path> paths;
        std::map<PointKey, bool> visited;

        auto find_path = [&](PointKey start_key) {
            Path path;
            PointKey curr_key = start_key;
            Vertex start_v = Vertex{FT(start_key.x)/1000000, FT(start_key.y)/1000000, FT(start_key.z)/1000000};
            path.points.push_back(start_v);
            visited[curr_key] = true;

            while (true) {
                bool found = false;
                for (auto& next_v : adj[curr_key]) {
                    PointKey next_key = to_key(next_v);
                    if (!visited[next_key]) {
                        visited[next_key] = true;
                        path.points.push_back(next_v);
                        curr_key = next_key;
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            }
            return path;
        };

        for (auto const& [key, neighbors] : adj) {
            if (!visited[key] && neighbors.size() == 1) paths.push_back(find_path(key));
        }
        for (auto const& [key, neighbors] : adj) {
            if (!visited[key]) paths.push_back(find_path(key));
        }

        // 2. Apply Pattern
        Geometry res;
        for (const auto& path : paths) {
            if (path.points.size() < 2) continue;

            double L = 0;
            std::vector<double> dists = {0};
            for (size_t i = 0; i < path.points.size() - 1; ++i) {
                double d = std::sqrt(CGAL::to_double(CGAL::squared_distance(
                    EK::Point_3(path.points[i].x, path.points[i].y, path.points[i].z),
                    EK::Point_3(path.points[i+1].x, path.points[i+1].y, path.points[i+1].z))));
                L += d;
                dists.push_back(L);
            }

            std::vector<std::pair<double, double>> on_intervals;

            // Start Pattern (Positive lengths from 0)
            double t_start = 0;
            bool start_on = true;
            for (double len : start) {
                double t_next = t_start + std::abs(len);
                if (start_on) on_intervals.push_back({t_start, t_next});
                t_start = t_next;
                start_on = !start_on;
            }

            // End Pattern (Lengths working backward from L)
            double t_end_limit = L;
            if (!end.empty()) {
                double t_curr = L;
                bool end_on = true;
                for (double len : end) {
                    double t_prev = t_curr - std::abs(len);
                    if (end_on) on_intervals.push_back({t_prev, t_curr});
                    t_curr = t_prev;
                    end_on = !end_on;
                }
                t_end_limit = t_curr;
            }

            // Repeat Pattern
            if (!repeat.empty()) {
                double range_start = t_start;
                double range_end = t_end_limit;
                
                bool end_oriented = false;
                for (double r : repeat) if (r < 0) end_oriented = true;

                if (range_end > range_start) {
                    double repeat_len = 0;
                    for (double r : repeat) repeat_len += std::abs(r);

                    if (end_oriented) {
                        // Align cycle to range_end
                        double t = range_end - offset;
                        while (t > range_start) {
                            bool r_on = true;
                            double current_t = t;
                            for (size_t i = 0; i < repeat.size(); ++i) {
                                double step = std::abs(repeat[repeat.size() - 1 - i]);
                                double t0 = current_t - step;
                                double t1 = current_t;
                                if (r_on) {
                                    on_intervals.push_back({std::max(range_start, t0), std::min(range_end, t1)});
                                }
                                current_t -= step;
                                r_on = !r_on;
                            }
                            t -= repeat_len;
                        }
                    } else {
                        // Align cycle to range_start
                        double t = range_start + offset;
                        while (t < range_end) {
                            bool r_on = true;
                            for (double r : repeat) {
                                double t_next = t + std::abs(r);
                                if (r_on) {
                                    on_intervals.push_back({std::max(range_start, t), std::min(range_end, t_next)});
                                }
                                t = t_next;
                                r_on = !r_on;
                                if (t >= range_end) break;
                            }
                        }
                    }
                }
            }

            // 3. Clip and Generate
            for (auto& interval : on_intervals) {
                double t0 = std::max(0.0, interval.first);
                double t1 = std::min(L, interval.second);
                if (t1 <= t0) continue;

                // Trace path
                for (size_t i = 0; i < path.points.size() - 1; ++i) {
                    double s0 = dists[i];
                    double s1 = dists[i+1];
                    double inter_start = std::max(t0, s0);
                    double inter_end = std::min(t1, s1);

                    if (inter_end > inter_start) {
                        double seg_l = s1 - s0;
                        double f0 = (inter_start - s0) / seg_l;
                        double f1 = (inter_end - s0) / seg_l;

                        auto lerp = [&](const Vertex& v1, const Vertex& v2, double t) {
                            return Vertex{v1.x + (v2.x - v1.x) * FT(t),
                                          v1.y + (v2.y - v1.y) * FT(t),
                                          v1.z + (v2.z - v1.z) * FT(t)};
                        };

                        int v_idx = (int)res.vertices.size();
                        res.vertices.push_back(lerp(path.points[i], path.points[i+1], f0));
                        res.vertices.push_back(lerp(path.points[i], path.points[i+1], f1));
                        res.segments.push_back({v_idx, v_idx + 1});
                    }
                }
            }
        }

        Shape out = P::make_shape(vfs, res, {{"type", "segments"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "repeat", "start", "end", "offset"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/stitch"},
            {"description", "Applies a complex recurring on/off pattern to a segment chain."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "repeat"}, {"type", "jot:numbers"}, {"default", std::vector<double>{}}},
                {{"name", "start"}, {"type", "jot:numbers"}, {"default", std::vector<double>{}}},
                {{"name", "end"}, {"type", "jot:numbers"}, {"default", std::vector<double>{}}},
                {{"name", "offset"}, {"type", "jot:number"}, {"default", 0.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void stitch_init(fs::VFSNode* vfs) {
    Processor::register_op<StitchOp<>, Shape, std::vector<double>, std::vector<double>, std::vector<double>, double>(vfs, "jot/stitch");
}

} // namespace geo
} // namespace jotcad
