#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include "fix/repair.h"
#include "math/zag.h"
#include "math/rational_approx.h"
#include "math/interval.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <string>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OrbOp : P {
    static constexpr const char* path = "jot/Orb";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Interval diameter, Interval width, Interval height, Interval depth, double zag_val, std::string method = "geodesic") {
        Geometry res;
        Interval x_range = (width.size() > 0) ? width : diameter;
        Interval y_range = (height.size() > 0) ? height : diameter;
        Interval z_range = (depth.size() > 0) ? depth : diameter;

        double w = x_range.size();
        double h = y_range.size();
        double d = z_range.size();
        double cx = x_range.center();
        double cy = y_range.center();
        double cz = z_range.center();

        if (method == "uv") {
            // Legacy UV Grid method
            double max_dim = std::max({w, h, d});
            int lon_sides = zag(max_dim, zag_val);
            int lat_sides = std::max(3, lon_sides / 2);

            FT w2 = FT(w) / FT(2);
            FT h2 = FT(h) / FT(2);
            FT d2 = FT(d) / FT(2);
            FT f_cx = FT(cx);
            FT f_cy = FT(cy);
            FT f_cz = FT(cz);

            std::vector<std::vector<int>> grid(lat_sides + 1, std::vector<int>(lon_sides));

            for (int i = 0; i <= lat_sides; ++i) {
                double phi_turns = (double)i / lat_sides * 0.5 - 0.25; // -0.25 to 0.25 turns
                auto [s_phi, c_phi] = get_approx_sincos(phi_turns);

                for (int j = 0; j < lon_sides; ++j) {
                    double theta_turns = (double)j / lon_sides; // 0 to 1 turns
                    auto [s_theta, c_theta] = get_approx_sincos(theta_turns);

                    grid[i][j] = (int)res.vertices.size();
                    res.vertices.push_back({
                        f_cx + c_phi * c_theta * w2,
                        f_cy + c_phi * s_theta * h2,
                        f_cz + s_phi * d2
                    });
                }
            }

            for (int i = 0; i < lat_sides; ++i) {
                for (int j = 0; j < lon_sides; ++j) {
                    int next_j = (j + 1) % lon_sides;
                    int v1 = grid[i][j];
                    int v2 = grid[i][next_j];
                    int v3 = grid[i+1][next_j];
                    int v4 = grid[i+1][j];

                    if (i == 0) {
                        res.faces.push_back({{{v1, v3, v4}}}); 
                    } else if (i == lat_sides - 1) {
                        res.faces.push_back({{{v1, v2, v3}}});
                    } else {
                        res.faces.push_back({{{v1, v2, v3, v4}}});
                    }
                }
            }
        } else {
            // New Geodesic subdivision method
            double max_dim = std::max({w, h, d});
            double R = max_dim / 2.0;
            int level = 0;
            if (zag_val > 0) {
                double ratio = (0.1375 * R) / zag_val;
                if (ratio > 1.0) {
                    level = (int)std::ceil(std::log2(ratio) / 2.0);
                }
            }
            level = std::clamp(level, 0, 6);

            // Unit sphere base regular icosahedron
            double t = (1.0 + std::sqrt(5.0)) / 2.0;
            double len = std::sqrt(1.0 + t * t);
            double a = 1.0 / len;
            double b = t / len;

            struct UnitVertex {
                double x, y, z;
            };
            std::vector<UnitVertex> unit_vertices = {
                {-a,  b,  0}, { a,  b,  0}, {-a, -b,  0}, { a, -b,  0},
                { 0, -a,  b}, { 0,  a,  b}, { 0, -a, -b}, { 0,  a, -b},
                { b,  0, -a}, { b,  0,  a}, {-b,  0, -a}, {-b,  0,  a}
            };

            std::vector<std::vector<int>> faces = {
                {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
                {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
                {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
            };

            struct PairHash {
                size_t operator()(const std::pair<int, int>& p) const {
                    return (size_t)p.first * 1000000ULL + (size_t)p.second;
                }
            };
            std::unordered_map<std::pair<int, int>, int, PairHash> midpoint_cache;

            auto get_midpoint = [&](int v1, int v2) {
                std::pair<int, int> edge = {std::min(v1, v2), std::max(v1, v2)};
                auto it = midpoint_cache.find(edge);
                if (it != midpoint_cache.end()) return it->second;

                UnitVertex pt1 = unit_vertices[v1];
                UnitVertex pt2 = unit_vertices[v2];
                double mx = pt1.x + pt2.x;
                double my = pt1.y + pt2.y;
                double mz = pt1.z + pt2.z;
                double length = std::sqrt(mx * mx + my * my + mz * mz);

                int new_idx = (int)unit_vertices.size();
                unit_vertices.push_back({mx / length, my / length, mz / length});
                midpoint_cache[edge] = new_idx;
                return new_idx;
            };

            for (int l = 0; l < level; ++l) {
                std::vector<std::vector<int>> next_faces;
                next_faces.reserve(faces.size() * 4);
                for (const auto& f : faces) {
                    int v1 = f[0];
                    int v2 = f[1];
                    int v3 = f[2];

                    int a_mid = get_midpoint(v1, v2);
                    int b_mid = get_midpoint(v2, v3);
                    int c_mid = get_midpoint(v3, v1);

                    next_faces.push_back({v1, a_mid, c_mid});
                    next_faces.push_back({v2, b_mid, a_mid});
                    next_faces.push_back({v3, c_mid, b_mid});
                    next_faces.push_back({a_mid, b_mid, c_mid});
                }
                faces = std::move(next_faces);
            }

            FT w2 = FT(w) / FT(2);
            FT h2 = FT(h) / FT(2);
            FT d2 = FT(d) / FT(2);
            FT f_cx = FT(cx);
            FT f_cy = FT(cy);
            FT f_cz = FT(cz);

            res.vertices.reserve(unit_vertices.size());
            for (const auto& uv : unit_vertices) {
                res.vertices.push_back({
                    f_cx + FT(uv.x) * w2,
                    f_cy + FT(uv.y) * h2,
                    f_cz + FT(uv.z) * d2
                });
            }

            res.faces.reserve(faces.size());
            for (const auto& f : faces) {
                res.faces.push_back({{{f[0], f[1], f[2]}}});
            }
        }
        
        // Assert closure and manifoldness
        assert(fix::is_geometry_solid(boolean::Engine::geometry_to_mesh(res)));

        Shape out = P::make_shape(vfs, res, {{"type", "closed"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"diameter", "width", "height", "depth", "zag", "method"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Orb"},
            {"description", "Generates a 3D sphere or ellipsoid solid."},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "diameter"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "width"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "depth"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "zag"}, {"type", "jot:number"}, {"default", 0.1}},
                {{"name", "method"}, {"type", "jot:string"}, {"default", "geodesic"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void orb_init(fs::VFSNode* vfs) {
    Processor::register_op<OrbOp<>, Interval, Interval, Interval, Interval, double, std::string>(vfs, "jot/Orb");
}

} // namespace geo
} // namespace jotcad
