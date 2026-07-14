#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/Convex_hull_3/dual/halfspace_intersection_3.h>
#include <CGAL/convexity_check_3.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <vector>
#include <string>
#include <iostream>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct DilateOp : P {
    static constexpr const char* path = "jot/dilate";

    static void apply_dilation_recursive(fs::VFSNode* vfs, Shape& s, double diameter) {
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            std::string type = s.tags.value("type", "");
            if (type == "closed" && !geo.vertices.empty()) {
                boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
                if (CGAL::is_strongly_convex_3(mesh)) {
                    // Compute centroid as an interior point
                    EK::Point_3 interior_point(0, 0, 0);
                    for (auto v : mesh.vertices()) {
                        auto p = mesh.point(v);
                        interior_point = EK::Point_3(interior_point.x() + p.x(), interior_point.y() + p.y(), interior_point.z() + p.z());
                    }
                    double num_v = (double)mesh.number_of_vertices();
                    interior_point = EK::Point_3(interior_point.x() / num_v, interior_point.y() / num_v, interior_point.z() / num_v);

                    // Shift face planes
                    std::vector<EK::Plane_3> shifted_planes;
                    FT shift_dist = FT(diameter) / FT(2.0);
                    
                    for (auto f : mesh.faces()) {
                        // Compute face normal using PMP
                        EK::Vector_3 normal = CGAL::Polygon_mesh_processing::compute_face_normal(f, mesh);
                        // Get any point on the face
                        auto h = halfedge(f, mesh);
                        auto v = source(h, mesh);
                        EK::Point_3 p = mesh.point(v);

                        // Outward shift along normal
                        EK::Plane_3 plane(p + normal * shift_dist, normal);
                        shifted_planes.push_back(plane);
                    }

                    // Compute halfspace intersection
                    boolean::Surface_mesh dilated_mesh;
                    try {
                        CGAL::halfspace_intersection_3(
                            shifted_planes.begin(),
                            shifted_planes.end(),
                            dilated_mesh,
                            interior_point
                        );
                        
                        // Triangulate faces to satisfy the triangle mesh requirement
                        CGAL::Polygon_mesh_processing::triangulate_faces(dilated_mesh);
                        
                        // Convert back to geometry
                        Geometry res = boolean::Engine::mesh_to_geometry(dilated_mesh);
                        if (!res.vertices.empty()) {
                            res.triangulate();
                            s.geometry = vfs->materialize<Geometry>(res);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[DilateOp] Halfspace intersection failed: " << e.what() << std::endl;
                    }
                } else {
                    std::cerr << "[DilateOp] Shape is not strongly convex; dilate is restricted to convex solids." << std::endl;
                }
            }
        }
        for (auto& child : s.components) {
            apply_dilation_recursive(vfs, child, diameter);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double diameter) {
        Shape out = in;
        apply_dilation_recursive(vfs, out, diameter);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "diameter"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/dilate"},
            {"description", "Dilates a convex closed solid shape by shifting its face planes outwards and resolving their intersection."},
            {"inputs", nlohmann::json::object({{"$in", {{"type", "jot:shape"}}}})},
            {"arguments", nlohmann::json::array({
                {{"name", "diameter"}, {"type", "jot:number"}, {"default", 1.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void dilate_init(fs::VFSNode* vfs) {
    Processor::register_op<DilateOp<>, Shape, double>(vfs, "jot/dilate");
}

} // namespace geo
} // namespace jotcad
