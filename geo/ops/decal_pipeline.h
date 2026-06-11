#pragma once
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/extrude.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Aff_transformation_3.h>
#include <vector>
#include <set>
#include <map>
#include <queue>
#include <iostream>
#include <stdexcept>

namespace jotcad {
namespace geo {
namespace decal {

typedef boolean::ExactMesh ExactMesh;

struct Patch {
    size_t id;
    std::vector<ExactMesh::Face_index> faces;
    EK::Vector_3 normal;
    Matrix unfold_tf; // Transforms from subject local to flattened UV space (Z=0)
};

// Functor for CGAL::extrude_mesh to project vertices to a specific Z plane.
template <typename InMesh, typename OutMesh>
struct ZProjector {
    ZProjector(const InMesh& in_mesh, OutMesh& out_mesh, EK::FT z) 
        : in_mesh(in_mesh), out_mesh(out_mesh), z(z) {}
    
    template <typename VIN, typename VOUT>
    void operator()(VIN vin, VOUT vout) const {
        auto p = in_mesh.point(vin);
        out_mesh.point(vout) = EK::Point_3(p.x(), p.y(), z);
    }
    
    const InMesh& in_mesh;
    OutMesh& out_mesh;
    EK::FT z;
};

inline std::vector<Patch> segment_into_patches(const ExactMesh& mesh) {
    std::vector<Patch> patches;
    std::set<ExactMesh::Face_index> visited;
    size_t next_id = 0;
    for (auto f : mesh.faces()) {
        if (visited.count(f)) continue;
        EK::Vector_3 normal = CGAL::Polygon_mesh_processing::compute_face_normal(f, mesh);
        Patch p;
        p.id = next_id++;
        p.normal = normal;
        std::vector<ExactMesh::Face_index> queue = {f};
        visited.insert(f);
        size_t head = 0;
        while (head < queue.size()) {
            auto curr = queue[head++];
            p.faces.push_back(curr);
            for (auto he : mesh.halfedges_around_face(mesh.halfedge(curr))) {
                auto opp = mesh.face(mesh.opposite(he));
                if (opp != ExactMesh::null_face() && !visited.count(opp)) {
                    if (normal == CGAL::Polygon_mesh_processing::compute_face_normal(opp, mesh)) {
                        visited.insert(opp);
                        queue.push_back(opp);
                    }
                }
            }
        }
        patches.push_back(p);
    }
    return patches;
}

inline std::map<size_t, std::vector<std::pair<size_t, ExactMesh::Halfedge_index>>> build_patch_adjacency(const ExactMesh& mesh, const std::vector<Patch>& patches) {
    std::map<ExactMesh::Face_index, size_t> face_to_patch;
    for (const auto& p : patches) {
        for (auto f : p.faces) face_to_patch[f] = p.id;
    }
    std::map<size_t, std::vector<std::pair<size_t, ExactMesh::Halfedge_index>>> adj;
    for (const auto& p : patches) {
        for (auto f : p.faces) {
            for (auto he : mesh.halfedges_around_face(mesh.halfedge(f))) {
                auto opp_he = mesh.opposite(he);
                auto opp_f = mesh.face(opp_he);
                if (opp_f != ExactMesh::null_face()) {
                    size_t neighbor_id = face_to_patch[opp_f];
                    if (neighbor_id != p.id) {
                        adj[p.id].push_back({neighbor_id, he});
                    }
                }
            }
        }
    }
    return adj;
}

/**
 * unfold_patches: Breadth-First Search to unroll patches into Z=0 UV plane.
 */
inline void unfold_patches(const ExactMesh& mesh, std::vector<Patch>& patches, const Matrix& tf_relief_inv_in) {
    auto adj = build_patch_adjacency(mesh, patches);
    std::map<size_t, bool> unfolded;
    
    for (size_t start_id = 0; start_id < patches.size(); ++start_id) {
        if (unfolded[start_id]) continue;

        // Force root patch to Z=0 in UV space
        // 1. Move a vertex of the root patch to the origin
        EK::Point_3 root_v = mesh.point(mesh.target(mesh.halfedge(patches[start_id].faces[0])));
        
        // 2. Initial transform: LookAt the patch's normal, anchored at its own surface.
        // We want to preserve the (x,y) positioning from tf_relief_inv_in, but force Z=0.
        Matrix root_tf = tf_relief_inv_in;
        EK::Point_3 root_v_uv = root_tf.transform(root_v);
        
        // Shift Z so the patch surface is at Z=0
        patches[start_id].unfold_tf = Matrix::translate(0, 0, -root_v_uv.z()) * root_tf;
        
        unfolded[start_id] = true;
        std::queue<size_t> q;
        q.push(start_id);

        while (!q.empty()) {
            size_t curr_id = q.front();
            q.pop();
            for (const auto& edge_pair : adj[curr_id]) {
                size_t next_id = edge_pair.first;
                if (unfolded[next_id]) continue;

                ExactMesh::Halfedge_index he = edge_pair.second;
                EK::Point_3 p1 = mesh.point(mesh.source(he));
                EK::Point_3 p2 = mesh.point(mesh.target(he));
                EK::Vector_3 n2 = patches[next_id].normal;
                EK::Point_3 p1_uv = patches[curr_id].unfold_tf.transform(p1);
                EK::Point_3 p2_uv = patches[curr_id].unfold_tf.transform(p2);
                EK::Vector_3 edge_uv = p2_uv - p1_uv;
                if (edge_uv.squared_length() == 0) continue;

                Matrix align_z = Matrix::lookAt(p1, n2);
                EK::Point_3 p2_temp = align_z.transform(p2);
                double angle_temp = std::atan2(CGAL::to_double(p2_temp.y()), CGAL::to_double(p2_temp.x()));
                double angle_uv = std::atan2(CGAL::to_double(edge_uv.y()), CGAL::to_double(edge_uv.x()));
                double rotate_z = (angle_uv - angle_temp) / (2.0 * M_PI);
                
                patches[next_id].unfold_tf = Matrix::translate(p1_uv.x(), p1_uv.y(), p1_uv.z()) * 
                                             Matrix::rotationZ(rotate_z) * 
                                             align_z;
                unfolded[next_id] = true;
                q.push(next_id);
            }
        }
    }
}

inline ExactMesh map_patch_to_uv_space(const ExactMesh& subject, const Patch& patch) {
    ExactMesh mapped;
    std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> v_map;
    for (auto f : patch.faces) {
        std::vector<ExactMesh::Vertex_index> vs;
        for (auto v : subject.vertices_around_face(subject.halfedge(f))) {
            if (!v_map.count(v)) {
                v_map[v] = mapped.add_vertex(patch.unfold_tf.transform(subject.point(v)));
            }
            vs.push_back(v_map[v]);
        }
        mapped.add_face(vs);
    }
    return mapped;
}

inline ExactMesh build_extrusion_slab(const ExactMesh& patch_in_uv, double z_min, double z_max) {
    ExactMesh slab;
    ZProjector<ExactMesh, ExactMesh> bottom_proj(patch_in_uv, slab, EK::FT(z_min));
    ZProjector<ExactMesh, ExactMesh> top_proj(patch_in_uv, slab, EK::FT(z_max));
    CGAL::Polygon_mesh_processing::extrude_mesh(patch_in_uv, slab, bottom_proj, top_proj);
    CGAL::Polygon_mesh_processing::triangulate_faces(slab);
    return slab;
}

inline void clip_relief_by_slab(ExactMesh& relief, ExactMesh& slab) {
    boolean::Engine::clip_mesh_by_mesh(relief, slab);
}

inline void map_mesh_back_to_subject_space(ExactMesh& mesh, const Matrix& unfold_tf) {
    Matrix refold_tf = unfold_tf.inverse();
    for (auto v : mesh.vertices()) {
        mesh.point(v) = refold_tf.transform(mesh.point(v));
    }
}

/**
 * apply_decal: The high-level orchestration function for the "Unfold-Clip-Refold" pipeline.
 */
inline ExactMesh apply_decal(const ExactMesh& subject_local, const ExactMesh& relief_local, const Matrix& tf_in, const Matrix& tf_relief) {
    auto patches = segment_into_patches(subject_local);
    Matrix tf_relief_inv_in = tf_relief.inverse() * tf_in;
    unfold_patches(subject_local, patches, tf_relief_inv_in);
    
    CGAL::Bbox_3 r_bbox = CGAL::Polygon_mesh_processing::bbox(relief_local);
    double z_min = r_bbox.zmin() - 1.0;
    double z_max = r_bbox.zmax() + 1.0;

    ExactMesh result_mesh;
    for (const auto& patch : patches) {
        ExactMesh patch_in_uv = map_patch_to_uv_space(subject_local, patch);
        CGAL::Bbox_3 p_bbox = CGAL::Polygon_mesh_processing::bbox(patch_in_uv);

        if (p_bbox.xmin() < r_bbox.xmin() || p_bbox.xmax() > r_bbox.xmax() ||
            p_bbox.ymin() < r_bbox.ymin() || p_bbox.ymax() > r_bbox.ymax()) {
            throw std::runtime_error("Decal relief does not provide complete coverage for subject patch.");
        }

        ExactMesh slab = build_extrusion_slab(patch_in_uv, z_min, z_max);
        ExactMesh relief_chunk = relief_local;
        clip_relief_by_slab(relief_chunk, slab);
        map_mesh_back_to_subject_space(relief_chunk, patch.unfold_tf);

        std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> v_map;
        for (auto f : relief_chunk.faces()) {
            std::vector<ExactMesh::Vertex_index> vs;
            for (auto v : relief_chunk.vertices_around_face(relief_chunk.halfedge(f))) {
                auto pt = relief_chunk.point(v);
                if (!v_map.count(v)) v_map[v] = result_mesh.add_vertex(pt);
                vs.push_back(v_map[v]);
            }
            if (vs.size() >= 3) result_mesh.add_face(vs);
        }
    }
    
    CGAL::Polygon_mesh_processing::triangulate_faces(result_mesh);
    return result_mesh;
}

} // namespace decal
} // namespace geo
} // namespace jotcad
