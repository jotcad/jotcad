#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/remesh.h>
#include <CGAL/Polygon_mesh_processing/tangential_relaxation.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/Kd_tree.h>
#include <CGAL/Fuzzy_sphere.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct DeformOp : P {
    typedef CGAL::Surface_mesh<IK::Point_3> Mesh;
    typedef boost::graph_traits<Mesh>::vertex_descriptor vertex_descriptor;
    typedef boost::graph_traits<Mesh>::edge_descriptor edge_descriptor;
    typedef CGAL::Search_traits_3<IK> SearchTraits;
    typedef CGAL::Kd_tree<SearchTraits> KdTree;
    typedef CGAL::Orthogonal_k_neighbor_search<SearchTraits> Neighbor_search;

    struct Constraint {
        vertex_descriptor v1, v2;
        double rest_length;
    };

    static void execute_pbd(Mesh& mesh, const std::vector<Constraint>& constraints, int iterations) {
        for (int iter = 0; iter < iterations; ++iter) {
            for (const auto& c : constraints) {
                IK::Point_3& p1 = mesh.point(c.v1);
                IK::Point_3& p2 = mesh.point(c.v2);
                IK::Vector_3 dir = p2 - p1;
                double current_len = std::sqrt(CGAL::to_double(dir.squared_length()));
                if (current_len < 1e-9) continue;

                IK::Vector_3 correction = dir * ((current_len - c.rest_length) / current_len * 0.5);
                p1 = p1 + correction;
                p2 = p2 - correction;
            }
        }
    }

    static std::vector<Constraint> build_spring_network(Mesh& mesh) {
        std::vector<Constraint> constraints;
        std::set<std::pair<vertex_descriptor, vertex_descriptor>> seen;

        auto add_constraint = [&](vertex_descriptor v1, vertex_descriptor v2) {
            if (v1 == v2) return;
            auto pair = std::make_pair(std::min(v1, v2), std::max(v1, v2));
            if (seen.find(pair) == seen.end()) {
                double len = std::sqrt(CGAL::to_double(CGAL::squared_distance(mesh.point(v1), mesh.point(v2))));
                constraints.push_back({v1, v2, len});
                seen.insert(pair);
            }
        };

        // 1. Mesh Edges (Skin)
        for (auto e : mesh.edges()) {
            add_constraint(mesh.source(mesh.halfedge(e)), mesh.target(mesh.halfedge(e)));
        }

        // 2. Proximity Springs (Muscle)
        KdTree tree;
        std::map<IK::Point_3, vertex_descriptor> pt_to_v;
        for (auto v : mesh.vertices()) {
            tree.insert(mesh.point(v));
            pt_to_v[mesh.point(v)] = v;
        }

        double avg_len = 0;
        if (!constraints.empty()) {
            for (const auto& c : constraints) avg_len += c.rest_length;
            avg_len /= constraints.size();
        } else {
            avg_len = 1.0;
        }

        for (auto v : mesh.vertices()) {
            std::vector<IK::Point_3> neighbors;
            CGAL::Fuzzy_sphere<SearchTraits> sphere(mesh.point(v), avg_len * 1.5);
            tree.search(std::back_inserter(neighbors), sphere);
            for (const auto& p : neighbors) {
                add_constraint(v, pt_to_v[p]);
            }
        }

        // 3. Volumetric Struts (Bulk)
        typedef CGAL::AABB_face_graph_triangle_primitive<Mesh> Primitive;
        typedef CGAL::AABB_traits<IK, Primitive> Traits;
        typedef CGAL::AABB_tree<Traits> AABBTree;
        AABBTree aabb(faces(mesh).first, faces(mesh).second, mesh);

        for (auto v : mesh.vertices()) {
            IK::Vector_3 n = CGAL::Polygon_mesh_processing::compute_vertex_normal(v, mesh);
            IK::Ray_3 ray(mesh.point(v), -n); // Ray into the volume
            
            std::vector<typename AABBTree::Intersection_and_primitive_id<IK::Ray_3>::Type> intersections;
            aabb.all_intersections(ray, std::back_inserter(intersections));

            for (const auto& inter : intersections) {
                if (const IK::Point_3* p = std::get_if<IK::Point_3>(&inter.first)) {
                    double dist = std::sqrt(CGAL::to_double(CGAL::squared_distance(mesh.point(v), *p)));
                    if (dist > avg_len * 0.1) { // Not the same point
                        // Find closest vertex to intersection
                        Neighbor_search search(tree, *p, 1);
                        add_constraint(v, pt_to_v[search.begin()->first]);
                        break; // Only need the first opposite wall
                    }
                }
            }
        }

        return constraints;
    }

    struct BendOp {
        static constexpr const char* path = "jot/bend";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double tau = 0.25, double radius = 0.0) {
            if (!in.geometry.has_value()) { vfs->write(fulfilling.with_output("$out"), in); return; }
            Geometry geo = vfs->read<Geometry>(in.geometry.value());
            Mesh mesh = boolean::Engine::geometry_to_mesh_ik(geo);

            // 1. Build Spring Network before deformation
            auto constraints = build_spring_network(mesh);

            // 2. Initial "Dumb" Bend Mapping
            IK::Iso_cuboid_3 bbox = CGAL::Polygon_mesh_processing::bbox(mesh);
            double height = CGAL::to_double(bbox.ymax() - bbox.ymin());
            double r = (radius == 0.0) ? (height / (tau * 2.0 * M_PI)) : radius;
            if (r == 0) r = 1.0;

            for (auto v : mesh.vertices()) {
                IK::Point_3 p = mesh.point(v);
                double x = CGAL::to_double(p.x());
                double y = CGAL::to_double(p.y());
                double z = CGAL::to_double(p.z());

                double angle = x / r;
                double dist_from_center = r + y;

                double nx = dist_from_center * std::sin(angle);
                double ny = dist_from_center * std::cos(angle) - r;
                mesh.point(v) = IK::Point_3(nx, ny, z);
            }

            // 3. Relax to maintain volume/integrity
            execute_pbd(mesh, constraints, 20);

            // 4. Cleanup and Output
            CGAL::Polygon_mesh_processing::tangential_relaxation(mesh);
            Shape out = in;
            out.geometry = vfs->materialize<Geometry>(boolean::Engine::mesh_to_geometry_ik(mesh));
            vfs->write(fulfilling.with_output("$out"), out);
        }
        static std::vector<std::string> argument_keys() { return {"$in", "tau", "radius"}; }
        static typename P::json schema() {
            return {
                {"path", "jot/bend"},
                {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
                {"arguments", nlohmann::json::array({
                    {{"name", "tau"}, {"type", "jot:number"}, {"default", 0.25}},
                    {{"name", "radius"}, {"type", "jot:number"}, {"default", 0.0}}
                })},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    struct TwistOp {
        static constexpr const char* path = "jot/twist";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double turns = 1.0) {
            if (!in.geometry.has_value()) { vfs->write(fulfilling.with_output("$out"), in); return; }
            Geometry geo = vfs->read<Geometry>(in.geometry.value());
            Mesh mesh = boolean::Engine::geometry_to_mesh_ik(geo);

            auto constraints = build_spring_network(mesh);

            IK::Iso_cuboid_3 bbox = CGAL::Polygon_mesh_processing::bbox(mesh);
            double z_min = CGAL::to_double(bbox.zmin());
            double z_max = CGAL::to_double(bbox.zmax());
            double z_range = z_max - z_min;
            if (z_range == 0) z_range = 1.0;

            for (auto v : mesh.vertices()) {
                IK::Point_3 p = mesh.point(v);
                double x = CGAL::to_double(p.x());
                double y = CGAL::to_double(p.y());
                double z = CGAL::to_double(p.z());

                double angle = ((z - z_min) / z_range) * turns * 2.0 * M_PI;
                double nx = x * std::cos(angle) - y * std::sin(angle);
                double ny = x * std::sin(angle) + y * std::cos(angle);
                mesh.point(v) = IK::Point_3(nx, ny, z);
            }

            execute_pbd(mesh, constraints, 20);

            CGAL::Polygon_mesh_processing::tangential_relaxation(mesh);
            Shape out = in;
            out.geometry = vfs->materialize<Geometry>(boolean::Engine::mesh_to_geometry_ik(mesh));
            vfs->write(fulfilling.with_output("$out"), out);
        }
        static std::vector<std::string> argument_keys() { return {"$in", "turns"}; }
        static typename P::json schema() {
            return {
                {"path", "jot/twist"},
                {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
                {"arguments", nlohmann::json::array({ {{"name", "turns"}, {"type", "jot:number"}, {"default", 1.0}} })},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };
};

inline void deform_init(fs::VFSNode* vfs) {
    Processor::register_op<DeformOp<>::BendOp, Shape, double, double>(vfs, "jot/bend");
    Processor::register_op<DeformOp<>::TwistOp, Shape, double>(vfs, "jot/twist");
}

} // namespace geo
} // namespace jotcad
