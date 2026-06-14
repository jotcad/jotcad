#pragma once
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/extrude.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Side_of_triangle_mesh.h>
#include <CGAL/Aff_transformation_3.h>
#include <vector>
#include <set>
#include <map>
#include <iostream>

#include "rs/ruled_surfaces_base.h"
#include "rs/ruled_surfaces_objective_min_area.h"
#include "rs/ruled_surfaces_strategy_linear_slg.h"
#include "rs/ruled_surfaces_strategy_seam_search_all.h"
#include "rs/ruled_surfaces_join_strategy_naive.h"

namespace jotcad {
namespace geo {
namespace decal {

typedef boolean::ExactMesh ExactMesh;

struct Projector {
    const ExactMesh& mesh;
    ExactMesh& out_mesh;
    double z;
    Projector(const ExactMesh& m, ExactMesh& out, double z) : mesh(m), out_mesh(out), z(z) {}
    template <typename VIN, typename VOUT>
    void operator()(VIN vin, VOUT vout) const {
        auto p = mesh.point(vin);
        out_mesh.point(vout) = EK::Point_3(p.x(), p.y(), z);
    }
};

inline bool is_on_xy_segment(const EK::Point_3& p, const EK::Point_3& s, const EK::Point_3& t) {
    if (CGAL::orientation(EK::Point_2(s.x(), s.y()), EK::Point_2(t.x(), t.y()), EK::Point_2(p.x(), p.y())) != CGAL::COLLINEAR) {
        return false;
    }
    auto min_x = std::min(s.x(), t.x());
    auto max_x = std::max(s.x(), t.x());
    auto min_y = std::min(s.y(), t.y());
    auto max_y = std::max(s.y(), t.y());
    return p.x() >= min_x && p.x() <= max_x && p.y() >= min_y && p.y() <= max_y;
}

inline EK::FT get_subject_z(const EK::FT& x, const EK::FT& y, const std::set<ExactMesh::Halfedge_index>& subject_hole_edges, const ExactMesh& subject_in_relief) {
    EK::Point_3 p(x, y, 0);
    EK::FT best_dist_sq = -1;
    EK::FT best_z = 0;

    for (auto h_s : subject_hole_edges) {
        auto p_s = subject_in_relief.point(subject_in_relief.source(h_s));
        auto p_t = subject_in_relief.point(subject_in_relief.target(h_s));
        
        if (is_on_xy_segment(p, p_s, p_t)) {
            auto dx = p_t.x() - p_s.x();
            auto dy = p_t.y() - p_s.y();
            auto len_sq = dx * dx + dy * dy;
            if (len_sq > 0) {
                EK::FT t = ((x - p_s.x()) * dx + (y - p_s.y()) * dy) / len_sq;
                if (t < 0) t = 0;
                if (t > 1) t = 1;
                return p_s.z() + t * (p_t.z() - p_s.z());
            } else {
                return p_s.z();
            }
        }
        
        auto dx = p_t.x() - p_s.x();
        auto dy = p_t.y() - p_s.y();
        auto len_sq = dx * dx + dy * dy;
        EK::FT dist_sq;
        EK::FT z_val;
        if (len_sq > 0) {
            EK::FT t = ((x - p_s.x()) * dx + (y - p_s.y()) * dy) / len_sq;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            auto proj_x = p_s.x() + t * dx;
            auto proj_y = p_s.y() + t * dy;
            dist_sq = (x - proj_x) * (x - proj_x) + (y - proj_y) * (y - proj_y);
            z_val = p_s.z() + t * (p_t.z() - p_s.z());
        } else {
            dist_sq = (x - p_s.x()) * (x - p_s.x()) + (y - p_s.y()) * (y - p_s.y());
            z_val = p_s.z();
        }
        if (best_dist_sq < 0 || dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_z = z_val;
        }
    }
    return best_z;
}

inline void merge_duplicate_vertices(ExactMesh& mesh) {
    std::map<EK::Point_3, ExactMesh::Vertex_index> unique_pts;
    std::vector<ExactMesh::Vertex_index> redirect(mesh.number_of_vertices());
    
    bool has_duplicates = false;
    for (auto v : mesh.vertices()) {
        auto p = mesh.point(v);
        if (unique_pts.count(p)) {
            redirect[v] = unique_pts[p];
            has_duplicates = true;
        } else {
            unique_pts[p] = v;
            redirect[v] = v;
        }
    }
    
    if (!has_duplicates) return;
    
    ExactMesh new_mesh;
    std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> old_to_new;
    for (auto v : mesh.vertices()) {
        if (redirect[v] == v) {
            old_to_new[v] = new_mesh.add_vertex(mesh.point(v));
        }
    }
    for (auto v : mesh.vertices()) {
        if (redirect[v] != v) {
            old_to_new[v] = old_to_new[redirect[v]];
        }
    }
    for (auto f : mesh.faces()) {
        std::vector<ExactMesh::Vertex_index> vs;
        for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) {
            vs.push_back(old_to_new[v]);
        }
        if (vs.size() >= 3 && vs[0] != vs[1] && vs[1] != vs[2] && vs[0] != vs[2]) {
            new_mesh.add_face(vs);
        }
    }
    mesh = new_mesh;
}

inline std::vector<ruled_surfaces::PolygonalChain> extract_boundary_loops(const ExactMesh& mesh, const std::set<ExactMesh::Halfedge_index>& border_halfedges) {
    std::vector<ruled_surfaces::PolygonalChain> chains;
    std::set<ExactMesh::Halfedge_index> visited;
    
    for (auto start_he : border_halfedges) {
        if (visited.count(start_he)) continue;
        
        ruled_surfaces::PolygonalChain chain;
        auto cur_he = start_he;
        do {
            visited.insert(cur_he);
            auto p = mesh.point(mesh.source(cur_he));
            chain.push_back(ruled_surfaces::PointCgal(CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z())));
            
            auto v_next = mesh.target(cur_he);
            bool found = false;
            for (auto next_he : CGAL::halfedges_around_source(v_next, mesh)) {
                if (border_halfedges.count(next_he)) {
                    cur_he = next_he;
                    found = true;
                    break;
                }
            }
            if (!found) break;
        } while (cur_he != start_he);
        
        if (chain.size() >= 2) {
            chain.push_back(chain.front());
            chains.push_back(chain);
        }
    }
    return chains;
}

inline ruled_surfaces::PolygonalChain rotate_loop(const ruled_surfaces::PolygonalChain& loop, size_t start_idx) {
    ruled_surfaces::PolygonalChain rotated;
    size_t n = loop.size() - 1; // loop has n+1 elements, front() == back()
    for (size_t i = 0; i < n; ++i) {
        rotated.push_back(loop[(start_idx + i) % n]);
    }
    rotated.push_back(rotated.front());
    return rotated;
}

inline ExactMesh apply_decal(const ExactMesh& subject_local, const ExactMesh& relief_local, const Matrix& tf_in, const Matrix& tf_relief, double fade_radius = 2.0) {
    // 1. Transform subject to relief's local space
    Matrix tf_subject_to_relief = tf_relief.inverse() * tf_in;
    ExactMesh subject_in_relief = subject_local;
    for (auto v : subject_in_relief.vertices()) {
        subject_in_relief.point(v) = tf_subject_to_relief.transform(subject_in_relief.point(v));
    }

    CGAL::Bbox_3 r_bbox = CGAL::Polygon_mesh_processing::bbox(relief_local);
    CGAL::Bbox_3 s_bbox = CGAL::Polygon_mesh_processing::bbox(subject_in_relief);
    double z_min = std::min(s_bbox.zmin(), r_bbox.zmin()) - 10.0;
    double z_max = std::max(s_bbox.zmax(), r_bbox.zmax()) + 10.0;

    // 2. Extrude the boundary of the relief directly to create a precise footprint cutter
    ExactMesh relief_copy = relief_local;
    ExactMesh footprint;
    std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> v_map;
    std::vector<ExactMesh::Halfedge_index> border;
    CGAL::Polygon_mesh_processing::border_halfedges(relief_copy, std::back_inserter(border));

    if (border.empty()) {
        CGAL::Bbox_3 bbox = CGAL::Polygon_mesh_processing::bbox(relief_copy);
        auto v0 = footprint.add_vertex(EK::Point_3(bbox.xmin(), bbox.ymin(), 0));
        auto v1 = footprint.add_vertex(EK::Point_3(bbox.xmax(), bbox.ymin(), 0));
        auto v2 = footprint.add_vertex(EK::Point_3(bbox.xmax(), bbox.ymax(), 0));
        auto v3 = footprint.add_vertex(EK::Point_3(bbox.xmin(), bbox.ymax(), 0));
        footprint.add_face(v0, v1, v2, v3);
    } else {
        std::set<ExactMesh::Vertex_index> visited;
        for (auto start_he : border) {
            auto v_start = relief_copy.source(start_he);
            if (visited.count(v_start)) continue;
            
            std::vector<ExactMesh::Vertex_index> loop;
            auto cur_he = start_he;
            do {
                auto v = relief_copy.source(cur_he);
                visited.insert(v);
                if (!v_map.count(v)) {
                    auto p = relief_copy.point(v);
                    v_map[v] = footprint.add_vertex(EK::Point_3(p.x(), p.y(), 0));
                }
                loop.push_back(v_map[v]);
                
                auto v_next = relief_copy.target(cur_he);
                bool found = false;
                for (auto next_he : CGAL::halfedges_around_source(v_next, relief_copy)) {
                    if (relief_copy.is_border(next_he)) {
                        cur_he = next_he;
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            } while (relief_copy.source(cur_he) != v_start);
            
            if (loop.size() >= 3) {
                footprint.add_face(loop);
            }
        }
    }
    
    CGAL::Polygon_mesh_processing::triangulate_faces(footprint);
    ExactMesh precise_cutter;
    CGAL::Polygon_mesh_processing::extrude_mesh(footprint, precise_cutter, 
        Projector(footprint, precise_cutter, z_min), 
        Projector(footprint, precise_cutter, z_max));
    CGAL::Polygon_mesh_processing::triangulate_faces(precise_cutter);

    // 3. Cut the subject with the precise cutter
    std::cout << "      [Decal Debug] Corefining subject_in_relief and precise_cutter..." << std::endl;
    CGAL::Polygon_mesh_processing::corefine(subject_in_relief, precise_cutter);

    // 4. Identify subject faces inside the cutter (the hole)
    std::cout << "      [Decal Debug] Identifying interior faces..." << std::endl;
    CGAL::Side_of_triangle_mesh<ExactMesh, EK> inside_check(precise_cutter);
    std::set<ExactMesh::Face_index> interior_faces;
    for (auto f : subject_in_relief.faces()) {
        auto h = subject_in_relief.halfedge(f);
        auto source = subject_in_relief.point(subject_in_relief.source(h));
        auto target = subject_in_relief.point(subject_in_relief.target(h));
        auto next_target = subject_in_relief.point(subject_in_relief.target(subject_in_relief.next(h)));
        auto centroid = CGAL::centroid(source, target, next_target);
        
        auto side = inside_check(centroid);
        EK::Vector_3 normal = CGAL::Polygon_mesh_processing::compute_face_normal(f, subject_in_relief);
        bool is_interior = (side != CGAL::ON_UNBOUNDED_SIDE) && (normal.z() > 0);
        if (is_interior) {
            interior_faces.insert(f);
        }
    }

    // 5. Calculate Z-Offset to position the relief onto the subject's surface
    EK::FT target_z = 0;
    int count = 0;
    for (auto f : interior_faces) {
        for (auto he : subject_in_relief.halfedges_around_face(subject_in_relief.halfedge(f))) {
            auto v = subject_in_relief.target(he);
            target_z += subject_in_relief.point(v).z();
            count++;
        }
    }
    if (count > 0) {
        target_z /= count;
    }

    if (std::abs(CGAL::to_double(r_bbox.zmin())) < 1e-5) {
        for (auto v : relief_copy.vertices()) {
            auto p = relief_copy.point(v);
            relief_copy.point(v) = EK::Point_3(p.x(), p.y(), p.z() + target_z);
        }
    }

    // 6. Assemble the result mesh
    ExactMesh result;
    std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> s_to_new, r_to_new;
    std::set<ExactMesh::Halfedge_index> subject_hole_edges;
    std::map<ruled_surfaces::PointCgal, ExactMesh::Vertex_index> rs_vertex_map;

    // Add subject exterior
    for (auto f : subject_in_relief.faces()) {
        if (interior_faces.count(f)) continue;
        std::vector<ExactMesh::Vertex_index> vs;
        for (auto he : subject_in_relief.halfedges_around_face(subject_in_relief.halfedge(f))) {
            auto v = subject_in_relief.target(he);
            if (!s_to_new.count(v)) {
                s_to_new[v] = result.add_vertex(subject_in_relief.point(v));
                auto p = subject_in_relief.point(v);
                ruled_surfaces::PointCgal pt_double(CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z()));
                rs_vertex_map[pt_double] = s_to_new[v];
            }
            vs.push_back(s_to_new[v]);
            
            auto opp = subject_in_relief.face(subject_in_relief.opposite(he));
            if (opp != ExactMesh::null_face() && interior_faces.count(opp)) {
                subject_hole_edges.insert(he);
            }
        }
        result.add_face(vs);
    }

    // Populate subject_hole_edges vertices that might not have been added
    for (auto he : subject_hole_edges) {
        auto v = subject_in_relief.source(he);
        if (!s_to_new.count(v)) {
            s_to_new[v] = result.add_vertex(subject_in_relief.point(v));
            auto p = subject_in_relief.point(v);
            ruled_surfaces::PointCgal pt_double(CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z()));
            rs_vertex_map[pt_double] = s_to_new[v];
        }
        auto v_tgt = subject_in_relief.target(he);
        if (!s_to_new.count(v_tgt)) {
            s_to_new[v_tgt] = result.add_vertex(subject_in_relief.point(v_tgt));
            auto p = subject_in_relief.point(v_tgt);
            ruled_surfaces::PointCgal pt_double(CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z()));
            rs_vertex_map[pt_double] = s_to_new[v_tgt];
        }
    }

    // Add relief faces
    for (auto f : relief_copy.faces()) {
        std::vector<ExactMesh::Vertex_index> vs;
        for (auto v : relief_copy.vertices_around_face(relief_copy.halfedge(f))) {
            if (!r_to_new.count(v)) {
                r_to_new[v] = result.add_vertex(relief_copy.point(v));
                auto p = relief_copy.point(v);
                ruled_surfaces::PointCgal pt_double(CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z()));
                rs_vertex_map[pt_double] = r_to_new[v];
            }
            vs.push_back(r_to_new[v]);
        }
        result.add_face(vs);
    }

    // 7. Skirting (Ruled Surface between relief_border and subject_hole_edges)
    std::vector<ExactMesh::Halfedge_index> r_border_vec;
    CGAL::Polygon_mesh_processing::border_halfedges(relief_copy, std::back_inserter(r_border_vec));
    std::set<ExactMesh::Halfedge_index> r_border(r_border_vec.begin(), r_border_vec.end());

    std::vector<ruled_surfaces::PolygonalChain> p_chains = extract_boundary_loops(relief_copy, r_border);
    std::vector<ruled_surfaces::PolygonalChain> q_chains = extract_boundary_loops(subject_in_relief, subject_hole_edges);

    std::cout << "      [Decal Debug] subject_hole_edges size = " << subject_hole_edges.size() << " chains = " << q_chains.size() << std::endl;
    std::cout << "      [Decal Debug] relief_border size = " << r_border.size() << " chains = " << p_chains.size() << std::endl;

    if (!p_chains.empty() && !q_chains.empty()) {
        ruled_surfaces::MinArea objective;
        ruled_surfaces::Mesh skirting_mesh;

        using TriangulationStrategy = ruled_surfaces::LinearSearchSlg<ruled_surfaces::MinArea>;
        typename TriangulationStrategy::Options ls_options;
        ls_options.max_total_paths = 1;
        ls_options.stitch = true; // Yes, we want it closed/stitched
        TriangulationStrategy strategy(objective, ls_options);
        
        ruled_surfaces::SolutionStats stats;
        ruled_surfaces::BestTriangulationSearchSolutionVisitor visitor(&skirting_mesh, &stats);
        
        auto p_chain = p_chains[0];
        auto q_chain = q_chains[0];
        
        // Find the closest pair of points to use as the starting seam
        size_t best_p = 0, best_q = 0;
        double min_dist_sq = -1;
        for (size_t i = 0; i < p_chain.size() - 1; ++i) {
            for (size_t j = 0; j < q_chain.size() - 1; ++j) {
                auto dx = p_chain[i].x() - q_chain[j].x();
                auto dy = p_chain[i].y() - q_chain[j].y();
                auto dz = p_chain[i].z() - q_chain[j].z();
                double dist_sq = dx*dx + dy*dy + dz*dz;
                if (min_dist_sq < 0 || dist_sq < min_dist_sq) {
                    min_dist_sq = dist_sq;
                    best_p = i;
                    best_q = j;
                }
            }
        }
        
        auto rotated_p = rotate_loop(p_chain, best_p);
        auto rotated_q = rotate_loop(q_chain, best_q);
        
        strategy.generate(rotated_p, rotated_q, visitor);

        if (!skirting_mesh.is_empty()) {
            int added_count = 0;
            int failed_count = 0;
            for (auto f : skirting_mesh.faces()) {
                std::vector<ExactMesh::Vertex_index> vs;
                for (auto v : skirting_mesh.vertices_around_face(skirting_mesh.halfedge(f))) {
                    auto pt_double = skirting_mesh.point(v);
                    if (rs_vertex_map.count(pt_double)) {
                        vs.push_back(rs_vertex_map[pt_double]);
                    } else {
                        auto new_v = result.add_vertex(EK::Point_3(pt_double.x(), pt_double.y(), pt_double.z()));
                        rs_vertex_map[pt_double] = new_v;
                        vs.push_back(new_v);
                    }
                }
                if (vs.size() >= 3) {
                    auto f_idx = result.add_face(vs);
                    if (f_idx == ExactMesh::null_face()) {
                        std::reverse(vs.begin(), vs.end());
                        f_idx = result.add_face(vs);
                        if (f_idx == ExactMesh::null_face()) {
                            throw std::runtime_error("[Skirting] Fatal: Failed to add skirting face even after reversal!");
                        } else {
                            added_count++;
                        }
                    } else {
                        added_count++;
                    }
                }
            }
            std::cout << "      [Skirting] Generated ruled surface skirting with " << num_faces(skirting_mesh) << " faces. Added " << added_count << " successfully, " << failed_count << " failed." << std::endl;
        } else {
            std::cerr << "      [Skirting] Error: Ruled surface generation failed." << std::endl;
        }
    }

    // Final topological cleanup
    merge_duplicate_vertices(result);
    CGAL::Polygon_mesh_processing::triangulate_faces(result);
    CGAL::Polygon_mesh_processing::stitch_borders(result);

    // 8. Transform back to world space
    Matrix relief_to_subject = tf_subject_to_relief.inverse();
    for (auto v : result.vertices()) {
        result.point(v) = relief_to_subject.transform(result.point(v));
    }

    std::vector<ExactMesh::Halfedge_index> open_borders;
    CGAL::Polygon_mesh_processing::border_halfedges(result, std::back_inserter(open_borders));
    std::cout << "      [Decal Debug] Result border halfedges size = " << open_borders.size() << std::endl;

    CGAL::Polygon_mesh_processing::remove_isolated_vertices(result);
    return result;
}

} // namespace decal
} // namespace geo
} // namespace jotcad
