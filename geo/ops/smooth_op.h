#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/tangential_relaxation.h>
#include <CGAL/Polygon_mesh_processing/remesh.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Side_of_triangle_mesh.h>
#include <CGAL/boost/graph/iterator.h>
#include <CGAL/boost/graph/iterator.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SmoothOp : P {
    static constexpr const char* path = "jot/smooth";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, 
                        double limit_tau = 1.0/24.0, int iterations = 20, double resolution = 1.0, 
                        const Shape& region = Shape()) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        if (geo.faces.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        typedef boolean::Surface_mesh SurfaceMesh;
        typedef boost::graph_traits<SurfaceMesh>::vertex_descriptor vertex_descriptor;
        typedef boost::graph_traits<SurfaceMesh>::face_descriptor face_descriptor;
        typedef boost::graph_traits<SurfaceMesh>::edge_descriptor edge_descriptor;
        
        SurfaceMesh mesh = boolean::Engine::geometry_to_mesh(geo);
        
        // 1. ROI Selection
        std::vector<vertex_descriptor> roi_vertices;
        if (region.geometry.has_value()) {
            Geometry region_geo_data = vfs->read<Geometry>(region.geometry.value());
            SurfaceMesh region_mesh = boolean::Engine::geometry_to_mesh(region_geo_data);
            
            // Transform region mesh by region.tf (EK)
            for(auto v : region_mesh.vertices()) {
                region_mesh.point(v) = region.tf.transform(region_mesh.point(v));
            }
            
            CGAL::Side_of_triangle_mesh<SurfaceMesh, EK> inside_check(region_mesh);
            for (auto v : mesh.vertices()) {
                if (inside_check(mesh.point(v)) != CGAL::ON_UNBOUNDED_SIDE) {
                    roi_vertices.push_back(v);
                }
            }
        } else {
            for (auto v : mesh.vertices()) roi_vertices.push_back(v);
        }

        if (roi_vertices.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // 2. Refinement (Density for curves)
        if (resolution > 1.0) {
            std::vector<face_descriptor> roi_faces;
            std::set<face_descriptor> face_set;
            for (auto v : roi_vertices) {
                for (auto f : CGAL::faces_around_target(mesh.halfedge(v), mesh)) {
                    if (f != SurfaceMesh::null_face()) face_set.insert(f);
                }
            }
            roi_faces.assign(face_set.begin(), face_set.end());
            
            // Calculate avg edge length
            double avg_len = 0;
            int count = 0;
            for (auto e : mesh.edges()) {
                auto h = mesh.halfedge(e);
                avg_len += std::sqrt(CGAL::to_double(CGAL::squared_distance(mesh.point(mesh.source(h)), mesh.point(mesh.target(h)))));
                count++;
            }
            if (count > 0) avg_len /= count;
            
            double target_len = avg_len / resolution;
            CGAL::Polygon_mesh_processing::isotropic_remeshing(roi_faces, target_len, mesh,
                CGAL::parameters::number_of_iterations(2).protect_constraints(true));
            
            // Re-collect ROI vertices after remeshing
            roi_vertices.clear();
            if (region.geometry.has_value()) {
                Geometry region_geo_data = vfs->read<Geometry>(region.geometry.value());
                SurfaceMesh region_mesh = boolean::Engine::geometry_to_mesh(region_geo_data);
                for(auto v : region_mesh.vertices()) {
                    region_mesh.point(v) = region.tf.transform(region_mesh.point(v));
                }
                CGAL::Side_of_triangle_mesh<SurfaceMesh, EK> inside_check(region_mesh);
                for (auto v : mesh.vertices()) {
                    if (inside_check(mesh.point(v)) != CGAL::ON_UNBOUNDED_SIDE) roi_vertices.push_back(v);
                }
            } else {
                for (auto v : mesh.vertices()) roi_vertices.push_back(v);
            }
        }

        // 3. Angle-Limited Smoothing Loop
        double limit_rad = limit_tau * 2.0 * M_PI;
        
        for (int iter = 0; iter < iterations; ++iter) {
            std::map<vertex_descriptor, EK::Point_3> updates;
            
            for (auto v : roi_vertices) {
                if (CGAL::is_border(v, mesh)) continue;

                double max_angle = 0;
                for (auto h : CGAL::halfedges_around_target(v, mesh)) {
                    if (CGAL::is_border(mesh.edge(h), mesh)) continue;
                    
                    auto h_opp = mesh.opposite(h);
                    auto f1 = mesh.face(h);
                    auto f2 = mesh.face(h_opp);
                    if (f1 == SurfaceMesh::null_face() || f2 == SurfaceMesh::null_face()) continue;
                    
                    auto n1 = CGAL::Polygon_mesh_processing::compute_face_normal(f1, mesh);
                    auto n2 = CGAL::Polygon_mesh_processing::compute_face_normal(f2, mesh);
                    
                    double dot = CGAL::to_double(n1 * n2);
                    if (dot > 1.0) dot = 1.0;
                    if (dot < -1.0) dot = -1.0;
                    double angle = std::acos(dot);
                    if (angle > max_angle) max_angle = angle;
                }

                if (max_angle > limit_rad) {
                    EK::Vector_3 laplacian(0, 0, 0);
                    int neighbors = 0;
                    for (auto nb : CGAL::vertices_around_target(v, mesh)) {
                        laplacian = laplacian + (mesh.point(nb) - mesh.point(v));
                        neighbors++;
                    }
                    if (neighbors > 0) {
                        laplacian = laplacian / (double)neighbors;
                        double factor = (max_angle - limit_rad) / M_PI;
                        factor = std::pow(factor, 0.5);
                        updates[v] = mesh.point(v) + laplacian * factor * 0.5;
                    }
                }
            }
            
            for (auto const& [v, p] : updates) {
                mesh.point(v) = p;
            }
            
            CGAL::Polygon_mesh_processing::tangential_relaxation(mesh, 
                CGAL::parameters::number_of_iterations(1));
        }

        Geometry res = boolean::Engine::mesh_to_geometry(mesh);
        
        Shape out = in;
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "limit", "iterations", "resolution", "region"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/smooth"},
            {"description", "Smooths the geometry until dihedral angles approach the specified limit (Angle-Limited Smoothing)."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "limit"}, {"type", "jot:number"}, {"default", 1.0/24.0}, {"description", "Target dihedral angle limit in turns (tau). Smoothing stops when angles hit this threshold."}},
                {{"name", "iterations"}, {"type", "jot:number"}, {"default", 20}, {"description", "Maximum relaxation steps."}},
                {{"name", "resolution"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Subdivision factor. Higher values create denser meshes to support tighter curves."}},
                {{"name", "region"}, {"type", "jot:shape"}, {"optional", true}, {"description", "Optional bounding shape to limit smoothing to a specific region."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void smooth_init(fs::VFSNode* vfs) {
    Processor::register_op<SmoothOp<>, Shape, double, int, double, Shape>(vfs, "jot/smooth");
}

} // namespace geo
} // namespace jotcad
