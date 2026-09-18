#pragma once
#include "types.h"
#include "../mold/rotation.h"
#include <CGAL/convex_hull_3.h>
#include <cmath>
#include <queue>
#include <vector>
#include <algorithm>
#include <iostream>

namespace jotcad {
namespace geo {
namespace pour {

/**
 * Evaluates candidate pour orientations using the Hydraulic Watershed DAG
 * and Lexicographic Tiering from docs/POUR_PREP_DRAINAGE_DESIGN.md.
 * 
 * Tier 1 (Absolute Priority): Auxiliary Air Trap Count N_traps (0 is best, strictly dominates).
 * Tier 2 (Physical Drainage Hazard): Flat Ceiling Hazard Area (alpha < 5 deg, mm2, lower is better).
 * Tier 3 (Drainage Quality / Upright Stance): Drainage Slope Power (sum sin alpha_f * Area_f, higher is better).
 */
inline EK::Vector_3 find_optimal_pour_orientation(
    const ExactMesh& mesh,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    double min_angle_turns = default_min_angle
) {
    std::vector<EK::Vector_3> candidate_dirs;

    // 1. Cardinal axes (6 directions)
    for (int s : {-1, 1}) {
        candidate_dirs.push_back(EK::Vector_3(s, 0, 0));
        candidate_dirs.push_back(EK::Vector_3(0, s, 0));
        candidate_dirs.push_back(EK::Vector_3(0, 0, s));
    }

    // 2. 3D Corner diagonals (8 directions)
    for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
            for (int sz : {-1, 1}) {
                candidate_dirs.push_back(EK::Vector_3(sx, sy, sz));
            }
        }
    }

    // 3. Planar Edge diagonals (12 directions)
    for (int s1 : {-1, 1}) {
        for (int s2 : {-1, 1}) {
            candidate_dirs.push_back(EK::Vector_3(s1, s2, 0));
            candidate_dirs.push_back(EK::Vector_3(s1, 0, s2));
            candidate_dirs.push_back(EK::Vector_3(0, s1, s2));
        }
    }

    // 4. Dominant face normals from the mesh
    for (const auto& fn : face_normals) {
        double fn_len = std::sqrt(CGAL::to_double(fn.squared_length()));
        if (fn_len > 1e-6) {
            candidate_dirs.push_back(fn);
            candidate_dirs.push_back(-fn);
        }
    }

    // 5. Canonical Compound Tangent Offsets (Section 6 of docs/POUR_PREP_DRAINAGE_DESIGN.md):
    // For every primary base direction (especially 45° planar diagonals and corner diagonals),
    // construct an orthonormal tangent frame (u, v) on S² and generate compound roll offsets.
    // Testing rolls of 6°, 8°, and 10° (all exceeding the 5° bubble detachment threshold)
    // pitches transverse features so air drains continuously toward the primary gate.
    std::vector<EK::Vector_3> primary_bases = candidate_dirs;
    const std::vector<double> roll_angles_deg = {6.0, 8.0, 10.0};

    for (const auto& raw_base : primary_bases) {
        double len = std::sqrt(CGAL::to_double(raw_base.squared_length()));
        if (len < 1e-6) continue;
        double bx = CGAL::to_double(raw_base.x()) / len;
        double by = CGAL::to_double(raw_base.y()) / len;
        double bz = CGAL::to_double(raw_base.z()) / len;

        // Construct orthonormal tangent frame (u, v) on S²
        double ux, uy, uz;
        if (std::abs(bz) < 0.9) {
            double u_len = std::sqrt(bx * bx + by * by);
            ux = -by / u_len;
            uy = bx / u_len;
            uz = 0.0;
        } else {
            double u_len = std::sqrt(bx * bx + bz * bz);
            ux = -bz / u_len;
            uy = 0.0;
            uz = bx / u_len;
        }
        double vx = by * uz - bz * uy;
        double vy = bz * ux - bx * uz;
        double vz = bx * uy - by * ux;

        for (double deg : roll_angles_deg) {
            double tan_delta = std::tan(deg * M_PI / 180.0);

            // Single-axis tangent offsets
            for (double su : {-tan_delta, tan_delta}) {
                candidate_dirs.push_back(EK::Vector_3(FT(bx + su * ux), FT(by + su * uy), FT(bz + su * uz)));
            }
            for (double sv : {-tan_delta, tan_delta}) {
                candidate_dirs.push_back(EK::Vector_3(FT(bx + sv * vx), FT(by + sv * vy), FT(bz + sv * vz)));
            }

            // Dual-axis compound roll offsets (simultaneous pitch & roll)
            for (double su : {-tan_delta, tan_delta}) {
                for (double sv : {-tan_delta, tan_delta}) {
                    candidate_dirs.push_back(EK::Vector_3(
                        FT(bx + su * ux + sv * vx),
                        FT(by + su * uy + sv * vy),
                        FT(bz + su * uz + sv * vz)
                    ));
                }
            }
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

    // Lexicographic Ranking Structure (ZERO arbitrary weights)
    struct OrientationRank {
        int auxiliary_traps = 999999;  // Tier 1: True hydraulic air traps (0 is best, strictly dominates)
        double flat_hazard_area = 1e18;// Tier 2: Flat ceiling hazard area with α < min_angle (mm², lower is better)
        double drainage_power = -1e18; // Tier 3: Upright buoyancy power ∑ sin α_f · Area_f (mm², higher is better)
        EK::Vector_3 dir = EK::Vector_3(0, 0, 1);

        bool is_better_than(const OrientationRank& o) const {
            // Tier 1: Zero-vent mandate
            if (auxiliary_traps != o.auxiliary_traps) {
                return auxiliary_traps < o.auxiliary_traps;
            }
            // Tier 2: Minimize flat ceiling hazard
            if (std::abs(flat_hazard_area - o.flat_hazard_area) > 1e-4) {
                return flat_hazard_area < o.flat_hazard_area;
            }
            // Tier 3: Maximize drainage steepness / upright stance
            return drainage_power > o.drainage_power;
        }
    };

    OrientationRank best_rank;

    // Working buffers reused across candidate evaluations
    std::vector<double> heights(n_verts);
    std::vector<std::vector<int>> ascending_predecessors(n_verts);
    std::vector<bool> can_drain(n_verts);
    std::queue<int> q;
    std::vector<bool> visited(n_verts);

    for (const auto& raw_dir : candidate_dirs) {
        double len = std::sqrt(CGAL::to_double(raw_dir.squared_length()));
        if (len < 1e-9) continue;
        double udx = CGAL::to_double(raw_dir.x()) / len;
        double udy = CGAL::to_double(raw_dir.y()) / len;
        double udz = CGAL::to_double(raw_dir.z()) / len;
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
        // An edge u -> v allows buoyant bubble ascent iff:
        // (h(v) - h(u)) / length >= sin(min_angle)
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

            // Found a connected component of trapped vertices
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

        // 5. Calculate Tier 2 Flat Ceiling Hazard Area and Tier 3 Upright Drainage Power
        double flat_hazard_area = 0.0;
        double drainage_power = 0.0;

        for (const auto& f : face_list) {
            double dot_up = f.nx * udx + f.ny * udy + f.nz * udz;
            // Upward cavity ceiling facet (normal points into ceiling)
            if (dot_up > 0.001) {
                double sin_phi = std::sqrt((std::max)(0.0, 1.0 - dot_up * dot_up));
                if (sin_phi < sin_min) {
                    flat_hazard_area += f.area;
                } else {
                    drainage_power += sin_phi * f.area;
                }
            }
        }

        OrientationRank current_rank;
        current_rank.auxiliary_traps = auxiliary_traps;
        current_rank.flat_hazard_area = flat_hazard_area;
        current_rank.drainage_power = drainage_power;
        current_rank.dir = u_dir;

        if (current_rank.is_better_than(best_rank)) {
            best_rank = current_rank;
        }
    }

    std::cout << "  [Pour Orientation] Evaluated " << candidate_dirs.size() 
              << " candidate up-vectors (min_angle=" << min_angle_turns << " turns)." << std::endl
              << "    Best Rank -> Auxiliary Vents: " << best_rank.auxiliary_traps 
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
