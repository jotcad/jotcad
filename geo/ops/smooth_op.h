#pragma once
#include <vector>
#include <set>
#include <chrono>
#include <cmath>
#include <iostream>
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/smooth_shape.h>
#include <CGAL/Polygon_mesh_processing/remesh.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/boost/graph/properties.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SmoothOp : P {
    static constexpr const char* path = "jot/smooth";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, 
                        double limit_tau = 1.0/24.0, int iterations = 10, double resolution = 1.0, 
                        const Shape& region = Shape(), double radius = 0.0) {
        auto t_start = std::chrono::high_resolution_clock::now();
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        if (geo.faces.empty() && geo.triangles.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        typedef boolean::InexactMesh InexactMesh;
        typedef boost::graph_traits<InexactMesh>::vertex_descriptor vertex_descriptor;
        
        InexactMesh mesh = boolean::Engine::geometry_to_mesh_ik(geo);

        double target_len = 0.0;
        int eff_iterations = iterations;
        double time_step = 0.005;

        if (radius > 0.0) {
            // Automatically derive physical grain and iterations from radius
            target_len = 0.2 * radius;
            eff_iterations = std::max(1, (int)std::round(radius * 10.0));
        } else if (resolution > 1.0) {
            target_len = 2.0 / resolution;
        }

        // 1. Isotropic Remeshing
        if (target_len > 0.0) {
            CGAL::Polygon_mesh_processing::isotropic_remeshing(faces(mesh), target_len, mesh,
                CGAL::parameters::number_of_iterations(4).protect_constraints(false));
        }
        
        // 2. Intermittent Mean Curvature Flow execution
        for (int step = 0; step < eff_iterations; ++step) {
            CGAL::Polygon_mesh_processing::smooth_shape(mesh, time_step,
                CGAL::parameters::number_of_iterations(1)
            );
        }

        // 3. Post-smooth CGAL planar patch remeshing (decimate interior flat-face micro-triangles)
        std::vector<InexactMesh> meshes = { mesh };
        CGAL::Polygon_mesh_processing::decimate_meshes_with_common_interfaces(meshes, -0.999);
        mesh = meshes[0];

        auto t_end = std::chrono::high_resolution_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
        std::cout << "[CGAL smooth_shape] Execution time: " << total_ms << "ms (Vertices: " << mesh.number_of_vertices() << ", Radius: " << radius << "mm)" << std::endl;

        Geometry res = boolean::Engine::mesh_to_geometry_ik(mesh);
        
        Shape out = in;
        res.triangulate();
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "limit", "iterations", "resolution", "region", "radius"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/smooth"},
            {"description", "Smooths geometry using CGAL native Mean Curvature Flow (smooth_shape) with optional remeshing or target curve radius."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "limit"}, {"type", "jot:number"}, {"default", 1.0/24.0}, {"description", "Target dihedral angle limit in turns (tau)."}},
                {{"name", "iterations"}, {"type", "jot:number"}, {"default", 10}, {"description", "Number of mean curvature flow iterations."}},
                {{"name", "resolution"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Subdivision factor for edge refinement."}},
                {{"name", "region"}, {"type", "jot:shape"}, {"optional", true}, {"description", "Optional bounding shape region."}},
                {{"name", "radius"}, {"type", "jot:number"}, {"optional", true}, {"default", 0.0}, {"description", "Physical curve radius in millimeters."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void smooth_init(fs::VFSNode* vfs) {
    Processor::register_op<SmoothOp<>, Shape, double, int, double, Shape, double>(vfs, "jot/smooth");
}

} // namespace geo
} // namespace jotcad
