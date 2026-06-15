#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/distance.h>
#include <CGAL/Cartesian_converter.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ConformOp : P {
    static constexpr const char* path = "jot/conform";

    typedef CGAL::Surface_mesh<IK::Point_3> Mesh;
    typedef CGAL::AABB_face_graph_triangle_primitive<Mesh> Primitive;
    typedef CGAL::AABB_traits<IK, Primitive> Traits;
    typedef CGAL::AABB_tree<Traits> Tree;

    static void project_recursive(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const Tree& tree, const Mesh& target_mesh, const IK::Vector_3& dir, double offset) {
        Matrix current_tf = parent_tf * s.tf;
        CGAL::Cartesian_converter<EK, IK> ek_to_ik;
        CGAL::Cartesian_converter<IK, EK> ik_to_ek;

        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            bool changed = false;

            for (auto& v : geo.vertices) {
                EK::Point_3 p_world_ek = current_tf.transform(EK::Point_3(v.x, v.y, v.z));
                IK::Point_3 p_world = ek_to_ik(p_world_ek);
                IK::Point_3 p_new = p_world;
                bool hit = false;
                IK::Vector_3 normal(0,0,1);

                if (dir == IK::Vector_3(0,0,0)) {
                    // Closest point mode
                    auto cp = tree.closest_point_and_primitive(p_world);
                    p_new = cp.first;
                    normal = CGAL::Polygon_mesh_processing::compute_face_normal(cp.second, target_mesh);
                    hit = true;
                } else {
                    // Raycast mode
                    IK::Ray_3 ray(p_world, dir);
                    std::vector<typename Tree::Intersection_and_primitive_id<IK::Ray_3>::Type> intersections;
                    tree.all_intersections(ray, std::back_inserter(intersections));
                    
                    if (!intersections.empty()) {
                        double min_dist = -1;
                        for (const auto& inter : intersections) {
                            if (const IK::Point_3* pi = std::get_if<IK::Point_3>(&inter.first)) {
                                double d = CGAL::to_double(CGAL::squared_distance(p_world, *pi));
                                if (min_dist < 0 || d < min_dist) {
                                    min_dist = d;
                                    p_new = *pi;
                                    normal = CGAL::Polygon_mesh_processing::compute_face_normal(inter.second, target_mesh);
                                    hit = true;
                                }
                            }
                        }
                    }
                }

                if (hit) {
                    p_new = p_new + normal * offset;
                    EK::Point_3 p_local_ek = current_tf.inverse().transform(ik_to_ek(p_new));
                    v.x = p_local_ek.x();
                    v.y = p_local_ek.y();
                    v.z = p_local_ek.z();
                    changed = true;
                }
            }

            if (changed) {
                s.geometry = vfs->materialize<Geometry>(geo);
            }
        }

        for (auto& child : s.components) {
            project_recursive(vfs, child, current_tf, tree, target_mesh, dir, offset);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& target, std::vector<double> direction = {0,0,0}, double offset = 0.0) {
        if (!target.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        CGAL::Cartesian_converter<EK, IK> ek_to_ik;
        CGAL::Cartesian_converter<IK, EK> ik_to_ek;

        // Build AABB Tree for Target
        Geometry target_geo = vfs->read<Geometry>(target.geometry.value());
        Mesh target_mesh = boolean::Engine::geometry_to_mesh_ik(target_geo);
        
        // Apply target transform to mesh points
        for (auto v : target_mesh.vertices()) {
            EK::Point_3 p_ek = ik_to_ek(target_mesh.point(v));
            target_mesh.point(v) = ek_to_ik(target.tf.transform(p_ek));
        }
        
        Tree tree(faces(target_mesh).first, faces(target_mesh).second, target_mesh);
        tree.build();

        IK::Vector_3 dir(0,0,0);
        if (direction.size() >= 3 && (direction[0] != 0 || direction[1] != 0 || direction[2] != 0)) {
            dir = IK::Vector_3(direction[0], direction[1], direction[2]);
        }

        Shape out = in;
        project_recursive(vfs, out, Matrix::identity(), tree, target_mesh, dir, offset);
        
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "target", "direction", "offset"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/conform"},
            {"description", "Conformally projects/wraps the subject onto a target surface."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}}},
                {"target", {{"type", "jot:shape"}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "direction"}, {"type", "jot:vector"}, {"default", {0,0,0}}, {"description", "Projection direction. [0,0,0] for closest point (shrink-wrap)."}},
                {{"name", "offset"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Distance from target surface."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void conform_init(fs::VFSNode* vfs) {
    Processor::register_op<ConformOp<>, Shape, Shape, std::vector<double>, double>(vfs, "jot/conform");
}

} // namespace geo
} // namespace jotcad
