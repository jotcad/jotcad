#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"
#include <map>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct FuseOp : P {
    static constexpr const char* path = "jot/fuse";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools) {
        std::vector<boolean::Engine::ToolNode> all_nodes;
        
        // 1. Collect all geometry from subject and tools into world space
        boolean::Engine::collect_tool_geometry(vfs, in, Matrix::identity(), all_nodes);
        for (const auto& tool : tools) {
            boolean::Engine::collect_tool_geometry(vfs, tool, Matrix::identity(), all_nodes);
        }

        if (all_nodes.empty()) {
            vfs->write(fulfilling.with_output("$out"), Shape());
            return;
        }

        // 2. Cumulative results for flattening
        boolean::Surface_mesh combined_solids;
        std::map<std::string, boolean::General_polygon_set_2> plane_groups;
        Geometry remainder;

        for (const auto& node : all_nodes) {
            if (node.geo.vertices.empty()) continue;

            if (node.is_plane) {
                auto plane_opt = node.geo.find_plane();
                EK::Plane_3 world_plane = node.world_tf.transform(*plane_opt);
                
                std::stringstream ss;
                ss << world_plane.a() << "," << world_plane.b() << "," << world_plane.c() << "," << world_plane.d();
                std::string plane_key = ss.str();

                Matrix project_tf = Matrix::lookAt(world_plane.point(), world_plane.orthogonal_vector());
                boolean::Engine::add_geometry_to_gps(node.geo, project_tf * node.world_tf, plane_groups[plane_key]);
            } else {
                boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(node.geo);
                boolean::Engine::transform_mesh(mesh, node.world_tf);
                
                if (CGAL::is_closed(mesh)) {
                    if (combined_solids.number_of_vertices() == 0) {
                        combined_solids = mesh;
                    } else {
                        boolean::Engine::join_mesh_by_mesh(combined_solids, mesh);
                    }
                } else {
                    int base = (int)remainder.vertices.size();
                    for (const auto& v : node.geo.vertices) {
                        Point_3 p = node.world_tf.transform(Point_3(v.x, v.y, v.z));
                        remainder.vertices.push_back({p.x(), p.y(), p.z()});
                    }
                    for (const auto& f : node.geo.faces) {
                        Geometry::Face nf = f;
                        for (auto& loop : nf.loops) for (auto& idx : loop) idx += base;
                        remainder.faces.push_back(nf);
                    }
                    for (auto s : node.geo.segments) {
                        s[0] += base; s[1] += base;
                        remainder.segments.push_back(s);
                    }
                }
            }
        }

        // 3. Merge everything into one Geometry
        Geometry final_geo = remainder;
        
        if (combined_solids.number_of_vertices() > 0) {
            Geometry solid_geo = boolean::Engine::mesh_to_geometry(combined_solids);
            int base = (int)final_geo.vertices.size();
            for (const auto& v : solid_geo.vertices) final_geo.vertices.push_back(v);
            for (auto f : solid_geo.faces) {
                for (auto& loop : f.loops) for (auto& idx : loop) idx += base;
                final_geo.faces.push_back(f);
            }
        }

        for (auto& [key, gps] : plane_groups) {
            std::vector<FT> p_coeffs;
            std::stringstream ss_key(key);
            std::string val_str;
            while(std::getline(ss_key, val_str, ',')) {
                std::stringstream ss_val(val_str);
                FT val; ss_val >> val;
                p_coeffs.push_back(val);
            }
            
            EK::Plane_3 world_plane(p_coeffs[0], p_coeffs[1], p_coeffs[2], p_coeffs[3]);
            Matrix rehydrate_tf = Matrix::lookAt(world_plane.point(), world_plane.orthogonal_vector()).inverse();
            
            Geometry plane_geo = boolean::Engine::gps_to_geometry(gps);
            plane_geo.apply_tf(rehydrate_tf);

            int base = (int)final_geo.vertices.size();
            for (const auto& v : plane_geo.vertices) final_geo.vertices.push_back(v);
            for (auto f : plane_geo.faces) {
                for (auto& loop : f.loops) for (auto& idx : loop) idx += base;
                final_geo.faces.push_back(f);
            }
        }

        Shape out;
        out.geometry = vfs->materialize<Geometry>(final_geo);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/fuse"}, 
            {"arguments", { 
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}}, 
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}
            }}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void fuse_init(fs::VFSNode* vfs) {
    Processor::register_op<FuseOp<>, Shape, std::vector<Shape>>(vfs, "jot/fuse");
}

} // namespace geo
} // namespace jotcad
