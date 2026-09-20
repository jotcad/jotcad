#pragma once
#include "types.h"
#include "../mold/rotation.h"
#include <CGAL/convex_hull_3.h>
#include <cmath>
#include <queue>
#include <vector>
#include <algorithm>
#include <iostream>

#include <random>

namespace jotcad {
namespace geo {
namespace pour {

/**
 * Evaluates candidate pour orientations using the Hydraulic Watershed DAG
 * and Lexicographic Tiering from docs/POUR_PREP_DRAINAGE_DESIGN.md.
 * 
 * Tier 1 (Absolute Priority): Auxiliary Air Trap Count N_traps (0 is best, strictly dominates).
 * Tier 2 (Bottleneck Inclination): Maximize Minimal Inclination Angle alpha_min across upward ceilings (higher is better).
 * Tier 3 (Physical Drainage Hazard): Minimize Flat Ceiling Hazard Area (alpha < 5 deg, mm2, lower is better).
 * Tier 4 (Upright Buoyancy Drive): Maximize Drainage Slope Power (sum sin alpha_f * Area_f, higher is better).
 */
inline EK::Vector_3 find_optimal_pour_orientation(
    const ExactMesh& mesh,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    double min_angle_turns = default_min_angle
) {
    // 1. Primary Feature Seeds: exact geometric features (no arbitrary roll tables)
    std::vector<EK::Vector_3> candidate_seeds;

    // Cardinal axes (6 directions)
    for (int s : {-1, 1}) {
        candidate_seeds.push_back(EK::Vector_3(s, 0, 0));
        candidate_seeds.push_back(EK::Vector_3(0, s, 0));
        candidate_seeds.push_back(EK::Vector_3(0, 0, s));
    }

    // 3D Corner diagonals (8 directions)
    for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
            for (int sz : {-1, 1}) {
                candidate_seeds.push_back(EK::Vector_3(sx, sy, sz));
            }
        }
    }

    // Planar Edge diagonals (12 directions)
    for (int s1 : {-1, 1}) {
        for (int s2 : {-1, 1}) {
            candidate_seeds.push_back(EK::Vector_3(s1, s2, 0));
            candidate_seeds.push_back(EK::Vector_3(s1, 0, s2));
            candidate_seeds.push_back(EK::Vector_3(0, s1, s2));
        }
    }

    // Dominant face normals from the mesh
    for (const auto& fn : face_normals) {
        double fn_len = std::sqrt(CGAL::to_double(fn.squared_length()));
        if (fn_len > 1e-6) {
            candidate_seeds.push_back(fn);
            candidate_seeds.push_back(-fn);
        }
    }

    // Pre-extract mesh vertices and topology
    int n_verts = (int)mesh.number_of_vertices();
    std::vector<EK::Point_3> pts;
    pts.reserve(n_verts);
    for (auto v : mesh.vertices()) {
        pts.push_back(mesh.point(v));
    }

    struct PrecomputedEdge {
        int u, v;
        double length;
    };
    std::vector<PrecomputedEdge> edge_list;
    edge_list.reserve(mesh.number_of_edges());
    std::vector<std::vector<int>> mesh_neighbors(n_verts);

    for (auto e : mesh.edges()) {
        auto h = mesh.halfedge(e);
        int u = (int)mesh.source(h);
        int v = (int)mesh.target(h);
        mesh_neighbors[u].push_back(v);
        mesh_neighbors[v].push_back(u);

        double dx = CGAL::to_double(pts[v].x() - pts[u].x());
        double dy = CGAL::to_double(pts[v].y() - pts[u].y());
        double dz = CGAL::to_double(pts[v].z() - pts[u].z());
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > 1e-6) {
            edge_list.push_back({u, v, dist});
        }
    }

    // Pre-convert face normals and areas
    int n_faces = (int)face_normals.size();
    struct PrecomputedFace {
        double nx, ny, nz;
        double area;
    };
    std::vector<PrecomputedFace> face_list;
    face_list.reserve(n_faces);
    for (int i = 0; i < n_faces; ++i) {
        double fn_sq = CGAL::to_double(face_normals[i].squared_length());
        if (fn_sq < 1e-12) continue;
        double fn_len = std::sqrt(fn_sq);
        face_list.push_back({
            CGAL::to_double(face_normals[i].x()) / fn_len,
            CGAL::to_double(face_normals[i].y()) / fn_len,
            CGAL::to_double(face_normals[i].z()) / fn_len,
            CGAL::to_double(face_areas[i])
        });
    }

    // Physical bubble detachment threshold: sin(5°)
    const double sin_min = std::sin(min_angle_turns * 2.0 * M_PI);

    // Lexicographic Ranking Structure
    struct OrientationRank {
        int auxiliary_traps = 999999;  // Tier 1: True hydraulic air traps (0 is best, strictly dominates)
        double min_angle_deg = -1e18;  // Tier 2: Maximize minimal inclination angle across upward ceilings (deg)
        double flat_hazard_area = 1e18;// Tier 3: Minimize flat ceiling hazard area with alpha < min_angle (mm²)
        double drainage_power = -1e18; // Tier 4: Maximize overall upright drainage power (mm²)
        EK::Vector_3 dir = EK::Vector_3(0, 0, 1);

        bool is_better_than(const OrientationRank& o) const {
            // Tier 1: Zero-vent / minimal auxiliary vent mandate
            if (auxiliary_traps != o.auxiliary_traps) {
                return auxiliary_traps < o.auxiliary_traps;
            }
            // Tier 2: Maximize minimal inclination angle (bottleneck)
            if (std::abs(min_angle_deg - o.min_angle_deg) > 0.1) {
                return min_angle_deg > o.min_angle_deg;
            }
            // Tier 3: Minimize flat ceiling hazard
            if (std::abs(flat_hazard_area - o.flat_hazard_area) > 1e-4) {
                return flat_hazard_area < o.flat_hazard_area;
            }
            // Tier 4: Maximize drainage steepness / upright stance
            return drainage_power > o.drainage_power;
        }
    };

    // Working buffers reused across candidate evaluations
    std::vector<double> heights(n_verts);
    std::vector<std::vector<int>> ascending_predecessors(n_verts);
    std::vector<bool> can_drain(n_verts);
    std::queue<int> q;
    std::vector<bool> visited(n_verts);

    auto evaluate_candidate = [&](double udx, double udy, double udz) -> OrientationRank {
        double len = std::sqrt(udx * udx + udy * udy + udz * udz);
        if (len < 1e-9) return OrientationRank();
        udx /= len; udy /= len; udz /= len;
        EK::Vector_3 u_dir{FT(udx), FT(udy), FT(udz)};

        // 1. Calculate projected elevation along candidate up-vector
        int g_apex = 0;
        double max_h = -1e18;
        for (int i = 0; i < n_verts; ++i) {
            double h = CGAL::to_double(pts[i].x()) * udx +
                       CGAL::to_double(pts[i].y()) * udy +
                       CGAL::to_double(pts[i].z()) * udz;
            heights[i] = h;
            if (h > max_h) {
                max_h = h;
                g_apex = i;
            }
        }

        // 2. Build the Ascending Drainage Graph:
        // An edge u -> v allows buoyant bubble ascent iff (h(v) - h(u)) / length >= sin(min_angle)
        for (int i = 0; i < n_verts; ++i) {
            ascending_predecessors[i].clear();
            can_drain[i] = false;
            visited[i] = false;
        }

        for (const auto& edge : edge_list) {
            double dh = heights[edge.v] - heights[edge.u];
            if (dh > 0.0) {
                if (dh >= sin_min * edge.length) {
                    ascending_predecessors[edge.v].push_back(edge.u);
                }
            } else if (dh < 0.0) {
                double neg_dh = -dh;
                if (neg_dh >= sin_min * edge.length) {
                    ascending_predecessors[edge.u].push_back(edge.v);
                }
            }
        }

        // 3. Hydraulic Watershed Reachability from Gate G:
        // Gate G is the single connected summit plateau containing g_apex.
        std::queue<int> gate_q;
        gate_q.push(g_apex);
        can_drain[g_apex] = true;
        q.push(g_apex);

        while (!gate_q.empty()) {
            int curr = gate_q.front();
            gate_q.pop();

            for (int n_idx : mesh_neighbors[curr]) {
                if (!can_drain[n_idx] && std::abs(heights[n_idx] - max_h) <= 1e-6) {
                    can_drain[n_idx] = true;
                    gate_q.push(n_idx);
                    q.push(n_idx);
                }
            }
        }

        while (!q.empty()) {
            int curr = q.front();
            q.pop();

            for (int pred : ascending_predecessors[curr]) {
                if (!can_drain[pred]) {
                    can_drain[pred] = true;
                    q.push(pred);
                }
            }
        }

        // 4. Count undrained hydraulic air traps
        int auxiliary_traps = 0;
        for (int i = 0; i < n_verts; ++i) {
            if (can_drain[i] || visited[i]) continue;

            auxiliary_traps++;
            std::queue<int> comp_q;
            comp_q.push(i);
            visited[i] = true;

            while (!comp_q.empty()) {
                int curr = comp_q.front();
                comp_q.pop();

                for (int n_idx : mesh_neighbors[curr]) {
                    if (!can_drain[n_idx] && !visited[n_idx]) {
                        visited[n_idx] = true;
                        comp_q.push(n_idx);
                    }
                }
            }
        }

        // 5. Calculate Minimal Inclination Angle, Flat Hazard Area, and Drainage Power
        double min_angle_deg = 90.0;
        double flat_hazard_area = 0.0;
        double drainage_power = 0.0;
        bool has_upward_facet = false;

        for (const auto& f : face_list) {
            if (f.area < 1e-4) continue;
            double dot_up = f.nx * udx + f.ny * udy + f.nz * udz;
            // Upward cavity ceiling facet (normal points into ceiling)
            if (dot_up > 0.001) {
                has_upward_facet = true;
                double sin_alpha = std::sqrt((std::max)(0.0, 1.0 - dot_up * dot_up));
                double alpha_deg = std::asin((std::min)(1.0, sin_alpha)) * (180.0 / M_PI);
                if (alpha_deg < min_angle_deg) {
                    min_angle_deg = alpha_deg;
                }
                if (sin_alpha < sin_min) {
                    flat_hazard_area += f.area;
                } else {
                    drainage_power += sin_alpha * f.area;
                }
            }
        }
        if (!has_upward_facet) {
            min_angle_deg = 0.0;
        }

        OrientationRank rank;
        rank.auxiliary_traps = auxiliary_traps;
        rank.min_angle_deg = min_angle_deg;
        rank.flat_hazard_area = flat_hazard_area;
        rank.drainage_power = drainage_power;
        rank.dir = u_dir;
        return rank;
    };

    // Evaluate baseline geometric feature seeds
    OrientationRank best_rank;
    std::vector<std::pair<OrientationRank, EK::Vector_3>> seed_results;
    for (const auto& raw_dir : candidate_seeds) {
        double len = std::sqrt(CGAL::to_double(raw_dir.squared_length()));
        if (len < 1e-9) continue;
        double udx = CGAL::to_double(raw_dir.x()) / len;
        double udy = CGAL::to_double(raw_dir.y()) / len;
        double udz = CGAL::to_double(raw_dir.z()) / len;
        auto rank = evaluate_candidate(udx, udy, udz);
        seed_results.push_back({rank, EK::Vector_3(FT(udx), FT(udy), FT(udz))});
        if (rank.is_better_than(best_rank)) {
            best_rank = rank;
        }
    }

    // Sort seeds best-first
    std::sort(seed_results.begin(), seed_results.end(), [](const auto& a, const auto& b) {
        return a.first.is_better_than(b.first);
    });

    // 6. Guided Deterministic Annealed Random Walk on S²
    // Uses a fixed seed PRNG for 100% platform-independent determinism
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist_01(0.0, 1.0);

    int n_walk_seeds = (std::min)((int)seed_results.size(), 4);
    const int steps_per_seed = 150;
    int total_evals = (int)seed_results.size();

    for (int s = 0; s < n_walk_seeds; ++s) {
        OrientationRank curr_rank = seed_results[s].first;
        double ux = CGAL::to_double(seed_results[s].second.x());
        double uy = CGAL::to_double(seed_results[s].second.y());
        double uz = CGAL::to_double(seed_results[s].second.z());

        for (int step = 0; step < steps_per_seed; ++step) {
            double progress = (double)step / (double)steps_per_seed;
            // Quadratic temperature cooling
            double T = (1.0 - progress) * (1.0 - progress);
            // Step scale: wide exploration early (~25°), fine local climb late (~0.5°)
            double sigma = (25.0 * M_PI / 180.0) * T + (0.5 * M_PI / 180.0);

            // Construct orthonormal tangent frame (t1, t2) on S² at current vector u
            double t1x, t1y, t1z;
            if (std::abs(uz) < 0.9) {
                double l = std::sqrt(ux * ux + uy * uy);
                t1x = -uy / l; t1y = ux / l; t1z = 0.0;
            } else {
                double l = std::sqrt(ux * ux + uz * uz);
                t1x = -uz / l; t1y = 0.0; t1z = ux / l;
            }
            double t2x = uy * t1z - uz * t1y;
            double t2y = uz * t1x - ux * t1z;
            double t2z = ux * t1y - uy * t1x;

            double theta = sigma * dist_01(rng);
            double psi = dist_01(rng) * 2.0 * M_PI;

            double wx = std::cos(psi) * t1x + std::sin(psi) * t2x;
            double wy = std::cos(psi) * t1y + std::sin(psi) * t2y;
            double wz = std::cos(psi) * t1z + std::sin(psi) * t2z;

            double next_ux = std::cos(theta) * ux + std::sin(theta) * wx;
            double next_uy = std::cos(theta) * uy + std::sin(theta) * wy;
            double next_uz = std::cos(theta) * uz + std::sin(theta) * wz;
            double next_l = std::sqrt(next_ux * next_ux + next_uy * next_uy + next_uz * next_uz);
            next_ux /= next_l; next_uy /= next_l; next_uz /= next_l;

            auto new_rank = evaluate_candidate(next_ux, next_uy, next_uz);
            total_evals++;

            if (new_rank.is_better_than(curr_rank)) {
                curr_rank = new_rank;
                ux = next_ux; uy = next_uy; uz = next_uz;
                if (curr_rank.is_better_than(best_rank)) {
                    best_rank = curr_rank;
                }
            } else {
                double dE = 0.0;
                if (new_rank.auxiliary_traps > curr_rank.auxiliary_traps) {
                    dE += 10.0 * (new_rank.auxiliary_traps - curr_rank.auxiliary_traps);
                }
                if (new_rank.min_angle_deg < curr_rank.min_angle_deg) {
                    dE += (curr_rank.min_angle_deg - new_rank.min_angle_deg) / 10.0;
                }
                double prob = std::exp(-dE / (std::max)(1e-4, T * 2.0));
                if (dist_01(rng) < prob) {
                    curr_rank = new_rank;
                    ux = next_ux; uy = next_uy; uz = next_uz;
                }
            }
        }
    }

    std::cout << "  [Pour Orientation] Evaluated " << total_evals 
              << " candidates via deterministic annealed walk on S²." << std::endl
              << "    Best Rank -> Auxiliary Vents: " << best_rank.auxiliary_traps 
              << ", Min Angle: " << best_rank.min_angle_deg << "°"
              << ", Flat Hazard Area: " << best_rank.flat_hazard_area << " mm²"
              << ", Drainage Power: " << best_rank.drainage_power << " mm²"
              << ", Dir: (" << CGAL::to_double(best_rank.dir.x()) << ", "
              << CGAL::to_double(best_rank.dir.y()) << ", "
              << CGAL::to_double(best_rank.dir.z()) << ")" << std::endl << std::flush;

    return best_rank.dir;
}

inline Transformation get_gravity_rotation(const EK::Vector_3& up_dir) {
    return mold::compute_exact_z_rotation(up_dir).first;
}

inline ExactMesh rotate_mesh_to_gravity(const ExactMesh& in, const Transformation& rot_tf) {
    ExactMesh out = in;
    for (auto v : out.vertices()) {
        out.point(v) = rot_tf.transform(out.point(v));
    }
    return out;
}

} // namespace pour
} // namespace geo
} // namespace jotcad
