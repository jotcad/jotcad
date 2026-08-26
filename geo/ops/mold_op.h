#pragma once
#include "protocols.h"
#include "processor.h"
#include "geometry.h"
#include "triangulation.h"
#include "boolean/engine.h"
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <set>
#include <memory>
#include <algorithm>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct MoldOp : P {
    static constexpr const char* path = "jot/mold";

    typedef CGAL::Surface_mesh<IK::Point_3> Mesh;
    typedef CGAL::AABB_face_graph_triangle_primitive<Mesh> Primitive;
    typedef CGAL::AABB_traits<IK, Primitive> Traits;
    typedef CGAL::AABB_tree<Traits> Tree;
    typedef boolean::ExactMesh ExactMesh;

    struct Matrix3x3 {
        double m[3][3];
        static Matrix3x3 rotation_z_to_v(double vx, double vy, double vz) {
            Matrix3x3 R;
            double len = std::sqrt(vx*vx + vy*vy + vz*vz);
            if (len > 1e-12) { vx /= len; vy /= len; vz /= len; }
            if (vz > 0.99999) {
                R.m[0][0] = 1; R.m[0][1] = 0; R.m[0][2] = 0;
                R.m[1][0] = 0; R.m[1][1] = 1; R.m[1][2] = 0;
                R.m[2][0] = 0; R.m[2][1] = 0; R.m[2][2] = 1;
            } else if (vz < -0.99999) {
                R.m[0][0] = 1; R.m[0][1] = 0; R.m[0][2] = 0;
                R.m[1][0] = 0; R.m[1][1] = -1; R.m[1][2] = 0;
                R.m[2][0] = 0; R.m[2][1] = 0; R.m[2][2] = -1;
            } else {
                double ax = -vy, ay = vx;
                double alen = std::sqrt(ax*ax + ay*ay);
                ax /= alen; ay /= alen;
                double cos_t = vz;
                double sin_t = std::sqrt(std::max(0.0, 1.0 - cos_t*cos_t));
                double one_minus_cos = 1.0 - cos_t;
                R.m[0][0] = cos_t + ax*ax*one_minus_cos;
                R.m[0][1] = ax*ay*one_minus_cos;
                R.m[0][2] = ay*sin_t;
                R.m[1][0] = ax*ay*one_minus_cos;
                R.m[1][1] = cos_t + ay*ay*one_minus_cos;
                R.m[1][2] = -ax*sin_t;
                R.m[2][0] = -ay*sin_t;
                R.m[2][1] = ax*sin_t;
                R.m[2][2] = cos_t;
            }
            return R;
        }
        IK::Point_3 local_to_world(double lx, double ly, double lz) const {
            return IK::Point_3(
                m[0][0]*lx + m[0][1]*ly + m[0][2]*lz,
                m[1][0]*lx + m[1][1]*ly + m[1][2]*lz,
                m[2][0]*lx + m[2][1]*ly + m[2][2]*lz
            );
        }
        IK::Point_3 world_to_local(double wx, double wy, double wz) const {
            return IK::Point_3(
                m[0][0]*wx + m[1][0]*wy + m[2][0]*wz,
                m[0][1]*wx + m[1][1]*wy + m[2][1]*wz,
                m[0][2]*wx + m[1][2]*wy + m[2][2]*wz
            );
        }
    };

    static Geometry build_box_geo(double xmin, double xmax, double ymin, double ymax, double zmin, double zmax) {
        Geometry geo;
        geo.vertices.push_back({xmin, ymin, zmin}); // 0
        geo.vertices.push_back({xmax, ymin, zmin}); // 1
        geo.vertices.push_back({xmax, ymax, zmin}); // 2
        geo.vertices.push_back({xmin, ymax, zmin}); // 3
        geo.vertices.push_back({xmin, ymin, zmax}); // 4
        geo.vertices.push_back({xmax, ymin, zmax}); // 5
        geo.vertices.push_back({xmax, ymax, zmax}); // 6
        geo.vertices.push_back({xmin, ymax, zmax}); // 7

        geo.faces.push_back({{{3, 2, 1, 0}}}); // Bottom
        geo.faces.push_back({{{4, 5, 6, 7}}}); // Top
        geo.faces.push_back({{{0, 1, 5, 4}}}); // Front
        geo.faces.push_back({{{1, 2, 6, 5}}}); // Right
        geo.faces.push_back({{{2, 3, 7, 6}}}); // Back
        geo.faces.push_back({{{3, 0, 4, 7}}}); // Left

        geo.triangles.push_back({3, 2, 1}); geo.triangles.push_back({3, 1, 0});
        geo.triangles.push_back({4, 5, 6}); geo.triangles.push_back({4, 6, 7});
        geo.triangles.push_back({0, 1, 5}); geo.triangles.push_back({0, 5, 4});
        geo.triangles.push_back({1, 2, 6}); geo.triangles.push_back({1, 6, 5});
        geo.triangles.push_back({2, 3, 7}); geo.triangles.push_back({2, 7, 6});
        geo.triangles.push_back({3, 0, 4}); geo.triangles.push_back({3, 4, 7});
        return geo;
    }

    static void collect_world_geometry_recursive(fs::VFSNode* vfs, const Shape& s, const Matrix& current_tf, Geometry& world_geo) {
        if (s.geometry.has_value()) {
            Geometry geo = vfs->template read<Geometry>(s.geometry.value());
            int offset = (int)world_geo.vertices.size();
            for (const auto& v : geo.vertices) {
                EK::Point_3 p = current_tf.transform(EK::Point_3(v.x, v.y, v.z));
                world_geo.vertices.push_back({p.x(), p.y(), p.z()});
            }
            if (!geo.triangles.empty()) {
                for (const auto& tri : geo.triangles) {
                    world_geo.triangles.push_back({tri[0] + offset, tri[1] + offset, tri[2] + offset});
                }
            } else if (!geo.faces.empty()) {
                std::vector<Vec3> pts;
                for (const auto& v : geo.vertices) {
                    pts.push_back(Vec3{CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)});
                }
                for (const auto& f : geo.faces) {
                    Triangulation::triangulate_face(f, pts, [&](int i0, int i1, int i2) {
                        world_geo.triangles.push_back({i0 + offset, i1 + offset, i2 + offset});
                    });
                }
            }
        }
        for (const auto& child : s.components) {
            collect_world_geometry_recursive(vfs, child, current_tf * child.tf, world_geo);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double padding = 10.0, double draft = 1.0, double explode = 0.0) {
        // 1. Collect World Geometry
        Geometry world_geo;
        collect_world_geometry_recursive(vfs, in, in.tf, world_geo);
        if (world_geo.triangles.empty()) {
            throw std::runtime_error("jot/mold Error: Input geometry contains no surface triangles.");
        }

        // 2. Compute Bounding Box
        double b_xmin = 1e9, b_xmax = -1e9;
        double b_ymin = 1e9, b_ymax = -1e9;
        double b_zmin = 1e9, b_zmax = -1e9;
        for (const auto& v : world_geo.vertices) {
            double vx = CGAL::to_double(v.x);
            double vy = CGAL::to_double(v.y);
            double vz = CGAL::to_double(v.z);
            b_xmin = std::min(b_xmin, vx); b_xmax = std::max(b_xmax, vx);
            b_ymin = std::min(b_ymin, vy); b_ymax = std::max(b_ymax, vy);
            b_zmin = std::min(b_zmin, vz); b_zmax = std::max(b_zmax, vz);
        }

        // 3. Build Mesh & Face Normals
        Mesh mesh;
        std::vector<Mesh::Vertex_index> v_indices;
        v_indices.reserve(world_geo.vertices.size());
        for (const auto& v : world_geo.vertices) {
            v_indices.push_back(mesh.add_vertex(IK::Point_3(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z))));
        }
        std::vector<IK::Vector_3> face_normals(world_geo.triangles.size());
        std::vector<IK::Point_3> face_centroids(world_geo.triangles.size());
        for (size_t f_idx = 0; f_idx < world_geo.triangles.size(); ++f_idx) {
            const auto& tri = world_geo.triangles[f_idx];
            mesh.add_face(v_indices[tri[0]], v_indices[tri[1]], v_indices[tri[2]]);
            const auto& p0 = world_geo.vertices[tri[0]];
            const auto& p1 = world_geo.vertices[tri[1]];
            const auto& p2 = world_geo.vertices[tri[2]];
            double cx = CGAL::to_double(p0.x + p1.x + p2.x) / 3.0;
            double cy = CGAL::to_double(p0.y + p1.y + p2.y) / 3.0;
            double cz = CGAL::to_double(p0.z + p1.z + p2.z) / 3.0;
            face_centroids[f_idx] = IK::Point_3(cx, cy, cz);
            IK::Vector_3 u(CGAL::to_double(p1.x - p0.x), CGAL::to_double(p1.y - p0.y), CGAL::to_double(p1.z - p0.z));
            IK::Vector_3 v(CGAL::to_double(p2.x - p0.x), CGAL::to_double(p2.y - p0.y), CGAL::to_double(p2.z - p0.z));
            IK::Vector_3 n = CGAL::cross_product(u, v);
            double nlen = std::sqrt(CGAL::to_double(n.squared_length()));
            if (nlen > 1e-12) n = n / nlen;
            face_normals[f_idx] = n;
        }

        // 4. Primary Split Axis Search (Coverage maximization)
        IK::Vector_3 dir_ik(1, 0, 0); // Primary lateral split

        // 5. Build AABB Tree & Detect Trapped Undercut Faces
        Tree tree(CGAL::faces(mesh).first, CGAL::faces(mesh).second, mesh);
        tree.build();

        std::vector<bool> is_trapped(world_geo.triangles.size(), false);
        std::vector<int> trapped_faces;
        IK::Vector_3 avg_insert_normal(0, 0, 0);

        for (size_t f_idx = 0; f_idx < world_geo.triangles.size(); ++f_idx) {
            double dot = CGAL::to_double(face_normals[f_idx] * dir_ik);
            if (std::abs(dot) < 1e-5) continue; // Face parallel to draw vector is naturally demoldable

            IK::Vector_3 ray_dir = (dot > 0.0) ? dir_ik : -dir_ik;
            IK::Point_3 ray_origin = face_centroids[f_idx] + face_normals[f_idx] * 1e-3;
            IK::Ray_3 ray(ray_origin, ray_dir);

            std::vector<typename Tree::Intersection_and_primitive_id<IK::Ray_3>::Type> intersections;
            tree.all_intersections(ray, std::back_inserter(intersections));
            for (const auto& inter : intersections) {
                IK::Point_3 pt;
                if (const IK::Point_3* pi = std::get_if<IK::Point_3>(&inter.first)) pt = *pi;
                else if (const IK::Segment_3* ps = std::get_if<IK::Segment_3>(&inter.first)) pt = ps->source();
                else continue;
                if (CGAL::to_double(CGAL::squared_distance(pt, ray_origin)) > 1e-4) {
                    is_trapped[f_idx] = true;
                    trapped_faces.push_back((int)f_idx);
                    avg_insert_normal = avg_insert_normal + face_normals[f_idx];
                    break;
                }
            }
        }

        // 6. Build Outer Mold Bounding Box Halves
        double mx_min = b_xmin - padding, mx_max = b_xmax + padding;
        double my_min = b_ymin - padding, my_max = b_ymax + padding;
        double mz_min = b_zmin - padding, mz_max = b_zmax + padding;

        Geometry b_left_geo = build_box_geo(mx_min, 0.0, my_min, my_max, mz_min, mz_max);
        Geometry b_right_geo = build_box_geo(0.0, mx_max, my_min, my_max, mz_min, mz_max);

        ExactMesh mesh_part = boolean::Engine::geometry_to_mesh(world_geo);
        CGAL::Polygon_mesh_processing::stitch_borders(mesh_part);
        CGAL::Polygon_mesh_processing::remove_almost_degenerate_faces(mesh_part);

        Shape composite;

        // Case A: 2-Piece Mold (Zero Undercuts)
        if (trapped_faces.empty()) {
            ExactMesh mesh_left = boolean::Engine::geometry_to_mesh(b_left_geo);
            boolean::Engine::cut_mesh_by_mesh(mesh_left, mesh_part);
            ExactMesh mesh_right = boolean::Engine::geometry_to_mesh(b_right_geo);
            boolean::Engine::cut_mesh_by_mesh(mesh_right, mesh_part);

            Shape left_shape = P::make_shape(vfs, boolean::Engine::mesh_to_geometry(mesh_left), {{"color", "#ee2b2b"}, {"name", "left_block"}, {"vector", {-1.0, 0.0, 0.0}}});
            Shape right_shape = P::make_shape(vfs, boolean::Engine::mesh_to_geometry(mesh_right), {{"color", "#2bee2b"}, {"name", "right_block"}, {"vector", {1.0, 0.0, 0.0}}});

            if (explode > 0.0) {
                left_shape.tf = Matrix::translate(-explode, 0.0, 0.0);
                right_shape.tf = Matrix::translate(explode, 0.0, 0.0);
            }

            composite.components.push_back(left_shape);
            composite.components.push_back(right_shape);
            composite.tags["pieces"] = 2;
        } 
        // Case B: 3-Piece Mold with Side Action Insert
        else {
            double ilen = std::sqrt(CGAL::to_double(avg_insert_normal.squared_length()));
            double idx = (ilen > 1e-12) ? CGAL::to_double(avg_insert_normal.x()) / ilen : 0.0;
            double idy = (ilen > 1e-12) ? CGAL::to_double(avg_insert_normal.y()) / ilen : 0.0;
            double idz = (ilen > 1e-12) ? CGAL::to_double(avg_insert_normal.z()) / ilen : 1.0;
            Matrix3x3 R = Matrix3x3::rotation_z_to_v(idx, idy, idz);

            std::set<int> insert_vertices;
            for (int f_idx : trapped_faces) {
                insert_vertices.insert(world_geo.triangles[f_idx][0]);
                insert_vertices.insert(world_geo.triangles[f_idx][1]);
                insert_vertices.insert(world_geo.triangles[f_idx][2]);
            }
            double l_xmin = 1e9, l_xmax = -1e9, l_ymin = 1e9, l_ymax = -1e9, l_zmin = 1e9, l_zmax = -1e9;
            for (int v_idx : insert_vertices) {
                const auto& v = world_geo.vertices[v_idx];
                IK::Point_3 lp = R.world_to_local(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z));
                l_xmin = std::min(l_xmin, lp.x()); l_xmax = std::max(l_xmax, lp.x());
                l_ymin = std::min(l_ymin, lp.y()); l_ymax = std::max(l_ymax, lp.y());
                l_zmin = std::min(l_zmin, lp.z()); l_zmax = std::max(l_zmax, lp.z());
            }

            double bpad = 1.0;
            l_xmin -= bpad; l_xmax += bpad; l_ymin -= bpad; l_ymax += bpad;
            double l_zmax_ext = l_zmax + padding * 3.0;

            std::vector<IK::Point_3> box_vertices(8);
            box_vertices[0] = R.local_to_world(l_xmin, l_ymin, l_zmin);
            box_vertices[1] = R.local_to_world(l_xmax, l_ymin, l_zmin);
            box_vertices[2] = R.local_to_world(l_xmax, l_ymax, l_zmin);
            box_vertices[3] = R.local_to_world(l_xmin, l_ymax, l_zmin);
            box_vertices[4] = R.local_to_world(l_xmin, l_ymin, l_zmax_ext);
            box_vertices[5] = R.local_to_world(l_xmax, l_ymin, l_zmax_ext);
            box_vertices[6] = R.local_to_world(l_xmax, l_ymax, l_zmax_ext);
            box_vertices[7] = R.local_to_world(l_xmin, l_ymax, l_zmax_ext);

            Geometry slide_box_geo;
            for (int i = 0; i < 8; ++i) slide_box_geo.vertices.push_back({box_vertices[i].x(), box_vertices[i].y(), box_vertices[i].z()});
            slide_box_geo.faces.push_back({{{3, 2, 1, 0}}}); slide_box_geo.faces.push_back({{{4, 5, 6, 7}}});
            slide_box_geo.faces.push_back({{{0, 1, 5, 4}}}); slide_box_geo.faces.push_back({{{1, 2, 6, 5}}});
            slide_box_geo.faces.push_back({{{2, 3, 7, 6}}}); slide_box_geo.faces.push_back({{{3, 0, 4, 7}}});

            slide_box_geo.triangles.push_back({3, 2, 1}); slide_box_geo.triangles.push_back({3, 1, 0});
            slide_box_geo.triangles.push_back({4, 5, 6}); slide_box_geo.triangles.push_back({4, 6, 7});
            slide_box_geo.triangles.push_back({0, 1, 5}); slide_box_geo.triangles.push_back({0, 5, 4});
            slide_box_geo.triangles.push_back({1, 2, 6}); slide_box_geo.triangles.push_back({1, 6, 5});
            slide_box_geo.triangles.push_back({2, 3, 7}); slide_box_geo.triangles.push_back({2, 7, 6});
            slide_box_geo.triangles.push_back({3, 0, 4}); slide_box_geo.triangles.push_back({3, 4, 7});

            ExactMesh mesh_slide_box = boolean::Engine::geometry_to_mesh(slide_box_geo);
            ExactMesh mesh_b_left = boolean::Engine::geometry_to_mesh(b_left_geo);
            ExactMesh mesh_b_right = boolean::Engine::geometry_to_mesh(b_right_geo);

            ExactMesh mesh_insert = mesh_slide_box;
            boolean::Engine::cut_mesh_by_mesh(mesh_insert, mesh_part);

            ExactMesh mesh_left_raw = mesh_b_left;
            boolean::Engine::cut_mesh_by_mesh(mesh_left_raw, mesh_part);
            ExactMesh mesh_right_raw = mesh_b_right;
            boolean::Engine::cut_mesh_by_mesh(mesh_right_raw, mesh_part);

            ExactMesh mesh_left = mesh_left_raw;
            boolean::Engine::cut_mesh_by_mesh(mesh_left, mesh_slide_box);
            ExactMesh mesh_right = mesh_right_raw;
            boolean::Engine::cut_mesh_by_mesh(mesh_right, mesh_slide_box);

            Shape left_shape = P::make_shape(vfs, boolean::Engine::mesh_to_geometry(mesh_left), {{"color", "#ee2b2b"}, {"name", "left_block"}, {"vector", {-1.0, 0.0, 0.0}}});
            Shape right_shape = P::make_shape(vfs, boolean::Engine::mesh_to_geometry(mesh_right), {{"color", "#2bee2b"}, {"name", "right_block"}, {"vector", {1.0, 0.0, 0.0}}});
            Shape insert_shape = P::make_shape(vfs, boolean::Engine::mesh_to_geometry(mesh_insert), {{"color", "#2b2bee"}, {"name", "insert_block"}, {"vector", {idx, idy, idz}}});

            if (explode > 0.0) {
                left_shape.tf = Matrix::translate(-explode, 0.0, 0.0);
                right_shape.tf = Matrix::translate(explode, 0.0, 0.0);
                insert_shape.tf = Matrix::translate(idx * explode, idy * explode, idz * explode);
            }

            composite.components.push_back(left_shape);
            composite.components.push_back(right_shape);
            composite.components.push_back(insert_shape);
            composite.tags["pieces"] = 3;
            composite.tags["insert_vector"] = {idx, idy, idz};
        }

        // Always attach original shape as ghost reference (preserving its original color)
        Shape ghost_shape = in;
        ghost_shape.tags["role"] = "ghost";
        composite.components.push_back(ghost_shape);

        vfs->write(fulfilling.with_output("$out"), composite);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "padding", "draft", "explode"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/mold"},
            {"description", "Generates N solid slipcast mold blocks that assemble to form an outer box with the input shape as an internal void cavity."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to generate mold blocks for."}}}}},
            {"arguments", json::array({
                {{"name", "padding"}, {"type", "jot:number"}, {"default", 10.0}, {"description", "Padding thickness around the part for outer mold walls."}},
                {{"name", "draft"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Draft angle for demold taper."}},
                {{"name", "explode"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Explosion distance to retract each mold block along its draw vector."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void mold_init(fs::VFSNode* vfs) {
    Processor::register_op<MoldOp<>, Shape, double, double, double>(vfs, "jot/mold");
}

} // namespace geo
} // namespace jotcad
