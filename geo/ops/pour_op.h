#pragma once
#include "protocols.h"
#include "processor.h"
#include "pour/types.h"
#include "pour/orientation.h"
#include "pour/traps.h"
#include "pour/vents.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PourOp : P {
    static constexpr const char* path = "jot/pourPrep";

    static void execute(
        fs::VFSNode* vfs,
        const fs::Selector& fulfilling,
        const Shape& in,
        double sprue_base,
        double sprue_top,
        double vent_dia,
        double auto_orient
    ) {
        pour::ExactMesh mesh_part;
        if (!boolean::Engine::shape_to_fused_mesh(vfs, in, mesh_part)) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        pour::PourParams params;
        params.sprue_base_dia = pour::FT(sprue_base);
        params.sprue_top_dia = pour::FT(sprue_top);
        params.vent_dia = pour::FT(vent_dia);
        params.auto_orient = (auto_orient > 0.5);

        // 1. Compute face normals and areas
        std::vector<pour::EK::Vector_3> face_normals;
        std::vector<pour::FT> face_areas;
        for (auto f : mesh_part.faces()) {
            auto h = mesh_part.halfedge(f);
            auto p0 = mesh_part.point(mesh_part.source(h));
            auto p1 = mesh_part.point(mesh_part.target(h));
            auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
            face_normals.push_back(CGAL::normal(p0, p1, p2));
            face_areas.push_back(CGAL::to_double(CGAL::Polygon_mesh_processing::face_area(f, mesh_part)));
        }

        // 2. Find optimal pour orientation
        pour::EK::Vector_3 up_dir(0, 0, 1);
        pour::ExactMesh oriented_mesh = mesh_part;
        if (params.auto_orient) {
            up_dir = pour::find_optimal_pour_orientation(mesh_part, face_normals, face_areas);
            oriented_mesh = pour::rotate_mesh_to_gravity(mesh_part, up_dir);
        }

        // 3. Detect local peak summits & air traps
        auto peaks = pour::detect_peaks_and_air_traps(oriented_mesh);
        std::cout << "  [Pour Prep] Optimal up vector: (" 
                  << CGAL::to_double(up_dir.x()) << ", " << CGAL::to_double(up_dir.y()) << ", " << CGAL::to_double(up_dir.z())
                  << ") with " << peaks.size() << " peak summits." << std::endl << std::flush;

        // 4. Create the oriented model shape
        Geometry oriented_geo = boolean::Engine::mesh_to_geometry(oriented_mesh);
        Shape result = P::make_shape(vfs, oriented_geo, {
            {"type", "closed"},
            {"pour/peaks", (int)peaks.size()},
            {"pour/up_vector", std::to_string(CGAL::to_double(up_dir.x())) + " " +
                               std::to_string(CGAL::to_double(up_dir.y())) + " " +
                               std::to_string(CGAL::to_double(up_dir.z()))}
        });

        // Naturally preserve any existing components from input
        for (const auto& child : in.components) {
            result.components.push_back(child);
        }

        // 5. Synthesize sprue and vents as distinct child shapes with role: "gap"
        pour::FT max_model_z = pour::FT(-1e9);
        for (auto v : oriented_mesh.vertices()) {
            pour::FT z = oriented_mesh.point(v).z();
            if (z > max_model_z) max_model_z = z;
        }
        pour::FT sprue_top_z = max_model_z + params.sprue_height;

        for (const auto& cluster : peaks) {
            pour::FT h = sprue_top_z - cluster.apex.z();
            if (h <= pour::FT(0)) h = pour::FT(10.0);

            pour::ExactMesh tool_mesh;
            std::string tool_name;
            if (cluster.is_primary) {
                tool_mesh = pour::build_sprue_mesh(
                    cluster.apex,
                    params.sprue_base_dia / pour::FT(2),
                    params.sprue_top_dia / pour::FT(2),
                    h,
                    6,
                    16
                );
                tool_name = "pour_sprue";
            } else {
                tool_mesh = pour::build_sprue_mesh(
                    cluster.apex,
                    params.vent_dia / pour::FT(2),
                    params.vent_dia / pour::FT(2),
                    h,
                    6,
                    12
                );
                tool_name = "riser_vent";
            }

            Geometry tool_geo = boolean::Engine::mesh_to_geometry(tool_mesh);
            Shape tool_shape = P::make_shape(vfs, tool_geo, {
                {"name", tool_name},
                {"color", "#ff222288"}
            });
            result.components.push_back(Shape::make_gap(tool_shape));
        }

        vfs->write(fulfilling.with_output("$out"), result);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "sprue_base", "sprue_top", "vent_dia", "auto_orient"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/pourPrep"},
            {"dsl_name", "pourPrep"},
            {"role", "method"},
            {"description", "Optimizes model pour orientation along gravity and attaches a primary pour funnel and air bleed risers as gap components."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"binding", "implicit"}, {"description", "The solid model to orient and vent."}}}
            }},
            {"arguments", {
                {{"name", "sprue_base"}, {"type", "jot:number"}, {"default", 16.0}, {"description", "Pour funnel base diameter in mm."}},
                {{"name", "sprue_top"}, {"type", "jot:number"}, {"default", 36.0}, {"description", "Pour funnel top opening diameter in mm."}},
                {{"name", "vent_dia"}, {"type", "jot:number"}, {"default", 2.5}, {"description", "Air bleed riser diameter in mm."}},
                {{"name", "auto_orient"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Whether to auto-orient along gravity (1.0 = yes)."}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The oriented shape with attached pour sprue and vents as gap components."}}}
            }}
        };
    }
};

inline void pour_init(fs::VFSNode* vfs) {
    Processor::register_op<PourOp<>, Shape, double, double, double, double>(vfs, "jot/pourPrep");
    Processor::register_op<PourOp<>, Shape, double, double, double, double>(vfs, "jot/orientPour");
}

} // namespace geo
} // namespace jotcad
