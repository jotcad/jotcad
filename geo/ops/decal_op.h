#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include "decal_pipeline.h"
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <vector>
#include <iostream>
#include <chrono>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct DecalOp : P {
    static constexpr const char* path = "jot/decal";
    typedef boolean::ExactMesh ExactMesh;

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& relief, std::string seam = "skirting", double fade_radius = 1.0) {
        auto t_start = std::chrono::high_resolution_clock::now();
        if (!in.geometry.has_value() || !relief.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in); return;
        }

        try {
            // 1. Read Subject and Relief
            Geometry in_geo = vfs->read<Geometry>(in.geometry.value());
            ExactMesh subject_local = boolean::Engine::geometry_to_mesh(in_geo);
            
            Geometry relief_geo = vfs->read<Geometry>(relief.geometry.value());
            ExactMesh relief_local = boolean::Engine::geometry_to_mesh(relief_geo);

            if (CGAL::is_closed(relief_local)) {
                std::cerr << "[DecalOp] Warning: Relief is closed. Decal expected an open surface." << std::endl;
            }

            // 2. Apply high-level pipeline
            ExactMesh decal_mesh_subject_space = decal::apply_decal(subject_local, relief_local, in.tf, relief.tf);

            // 3. Assemble: Combine decal chunks
            ExactMesh result_mesh = decal_mesh_subject_space;
            
            // Clean up result
            CGAL::Polygon_mesh_processing::triangulate_faces(result_mesh);
            CGAL::Polygon_mesh_processing::remove_isolated_vertices(result_mesh);

            // 4. Write back to VFS
            Shape out = in; 
            out.geometry = vfs->materialize<Geometry>(boolean::Engine::mesh_to_geometry(result_mesh));
            vfs->write(fulfilling.with_output("$out"), out);
            
            auto t_end = std::chrono::high_resolution_clock::now();
            std::cout << "[DecalOp] Finished in " << std::chrono::duration<double>(t_end - t_start).count() << "s" << std::endl;

        } catch (const std::exception& e) { 
            std::cerr << "[DecalOp] Error: " << e.what() << std::endl; 
            throw; 
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "relief", "seam", "fade_radius"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/decal"},
            {"description", "Physical Clip and Re-place decal operation."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}}},
                {"relief", {{"type", "jot:shape"}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "seam"}, {"type", "jot:string"}, {"default", "skirting"}},
                {{"name", "fade_radius"}, {"type", "jot:number"}, {"default", 1.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void decal_init(fs::VFSNode* vfs) {
    Processor::register_op<DecalOp<>, Shape, Shape, std::string, double>(vfs, "jot/decal");
}

} // namespace geo
} // namespace jotcad
