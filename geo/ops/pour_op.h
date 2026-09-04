#pragma once
#include "protocols.h"
#include "processor.h"
#include "pour/types.h"
#include "pour/orientation.h"
#include "pour/traps.h"
#include "pour/vents.h"
#include "boolean/engine.h"
#include "mold/types.h"

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
        bool auto_orient,
        bool vents
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
        params.auto_orient = auto_orient;

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

        // 3. Compute unvented model bounding limits in pure FT
        pour::FT xmin = pour::FT(1e9), xmax = pour::FT(-1e9);
        pour::FT ymin = pour::FT(1e9), ymax = pour::FT(-1e9);
        pour::FT zmin = pour::FT(1e9), zmax = pour::FT(-1e9);
        for (auto v : oriented_mesh.vertices()) {
            auto p = oriented_mesh.point(v);
            if (p.x() < xmin) xmin = p.x();
            if (p.x() > xmax) xmax = p.x();
            if (p.y() < ymin) ymin = p.y();
            if (p.y() > ymax) ymax = p.y();
            if (p.z() < zmin) zmin = p.z();
            if (p.z() > zmax) zmax = p.z();
        }

        // 4. Construct the physical mold stock box shape using padding
        pour::FT padding = pour::FT(15.0);
        pour::FT box_xmin = xmin - padding;
        pour::FT box_xmax = xmax + padding;
        pour::FT box_ymin = ymin - padding;
        pour::FT box_ymax = ymax + padding;
        pour::FT box_zmin = zmin - padding;
        pour::FT box_zmax = zmax + padding;
        Geometry stock_box_geo = mold::build_box_geo(box_xmin, box_xmax, box_ymin, box_ymax, box_zmin, box_zmax);
        Shape stock_box = P::make_shape(vfs, stock_box_geo, {
            {"mold/role", "box"},
            {"color", "#ffffff33"},
            {"name", "mold_stock_box"}
        });

        std::vector<pour::ToolComponentMesh> tool_components;
        if (vents) {
            // 5. Detect local peak summits & air traps
            auto peaks = pour::detect_peaks_and_air_traps(oriented_mesh);
            std::cout << "  [Pour Prep] Optimal up vector: (" 
                      << CGAL::to_double(up_dir.x()) << ", " << CGAL::to_double(up_dir.y()) << ", " << CGAL::to_double(up_dir.z())
                      << ") with " << peaks.size() << " peak summits." << std::endl << std::flush;

            // 6. Extend sprue & vents past the top of the mold stock box (box_zmax + 15mm) so they protrude through
            pour::FT sprue_top_z = box_zmax + pour::FT(15.0);
            tool_components = pour::generate_sprue_and_vents(peaks, params, sprue_top_z);
        } else {
            std::cout << "  [Pour Prep] Optimal up vector: (" 
                      << CGAL::to_double(up_dir.x()) << ", " << CGAL::to_double(up_dir.y()) << ", " << CGAL::to_double(up_dir.z())
                      << ") [vents disabled]" << std::endl << std::flush;
        }

        // 7. Create the oriented core casting shape
        Geometry oriented_geo = boolean::Engine::mesh_to_geometry(oriented_mesh);
        nlohmann::json result_tags = nlohmann::json::object();
        result_tags["type"] = "closed";
        result_tags["pour/up_vector"] = std::to_string(CGAL::to_double(up_dir.x())) + " " +
                                       std::to_string(CGAL::to_double(up_dir.y())) + " " +
                                       std::to_string(CGAL::to_double(up_dir.z()));
        Shape result = P::make_shape(vfs, oriented_geo, result_tags);

        // Attach discrete sprue and vent tool components as gap roles
        int vent_idx = 1;
        for (const auto& tc : tool_components) {
            Geometry tc_geo = boolean::Engine::mesh_to_geometry(tc.mesh);
            nlohmann::json tc_tags = nlohmann::json::object();
            tc_tags["type"] = "closed";
            tc_tags["role"] = "gap";
            if (tc.is_primary) {
                tc_tags["mold/role"] = "sprue";
                tc_tags["name"] = "pour_sprue";
                tc_tags["color"] = "#ee802baa";
            } else {
                tc_tags["mold/role"] = "vent";
                tc_tags["name"] = "air_vent_" + std::to_string(vent_idx++);
                tc_tags["color"] = "#2bee80aa";
            }
            Shape tc_shape = P::make_shape(vfs, tc_geo, tc_tags);
            result.components.push_back(tc_shape);
        }

        // Attach the mold stock box as a component with role: "box"
        result.components.push_back(stock_box);

        // Naturally preserve any existing components from input
        for (const auto& child : in.components) {
            result.components.push_back(child);
        }

        vfs->write(fulfilling.with_output("$out"), result);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "sprue_base", "sprue_top", "vent_dia", "auto_orient", "vents"}; }

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
                {{"name", "auto_orient"}, {"type", "jot:boolean"}, {"default", true}, {"description", "Whether to auto-orient along gravity."}},
                {{"name", "vents"}, {"type", "jot:boolean"}, {"default", true}, {"description", "Whether to synthesize and attach pour sprues and vents."}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The oriented shape with attached pour sprue and vents as gap components."}}}
            }}
        };
    }
};

inline void pour_init(fs::VFSNode* vfs) {
    Processor::register_op<PourOp<>, Shape, double, double, double, bool, bool>(vfs, "jot/pourPrep");
    Processor::register_op<PourOp<>, Shape, double, double, double, bool, bool>(vfs, "jot/orientPour");
}

} // namespace geo
} // namespace jotcad
