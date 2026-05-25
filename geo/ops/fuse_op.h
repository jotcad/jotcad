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

    struct GeometryNode {
        Geometry geo;
        Matrix tf;
        std::string type;
        bool is_gap;
    };

    static void collect_geometries(fs::VFSNode* vfs, const Shape& s, std::vector<GeometryNode>& nodes) {
        std::string type = s.tags.value("type", "");
        bool is_gap = s.is_gap();
        if (s.geometry.has_value()) {
            nodes.push_back({vfs->read<Geometry>(s.geometry.value()), s.tf, type, is_gap});
        }
        for (const auto& child : s.components) {
            collect_geometries(vfs, child, nodes);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools) {
        std::vector<GeometryNode> all_nodes;
        
        // 1. Collect all geometry from subject and tools recursively (Independent Matrix Mandate compliant)
        collect_geometries(vfs, in, all_nodes);
        for (const auto& tool : tools) {
            collect_geometries(vfs, tool, all_nodes);
        }

        if (all_nodes.empty()) {
            vfs->write(fulfilling.with_output("$out"), Shape());
            return;
        }

        // 2. Group components by type (Closed Solids vs Coplanar Surfaces)
        boolean::Surface_mesh combined_solids;
        std::vector<std::pair<EK::Plane_3, boolean::General_polygon_set_2>> plane_groups;

        std::vector<GeometryNode> regular_nodes, gap_nodes;
        for (const auto& node : all_nodes) {
            if (node.is_gap) gap_nodes.push_back(node);
            else regular_nodes.push_back(node);
        }

        // Process Regular Nodes
        for (const auto& node : regular_nodes) {
            if (node.geo.vertices.empty()) continue;

            if (node.type == "closed") {
                // A. Watertight 3D Solids
                boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(node.geo);
                boolean::Engine::transform_mesh(mesh, node.tf);
                
                if (combined_solids.number_of_vertices() == 0) {
                    combined_solids = mesh;
                } else {
                    boolean::Engine::join_mesh_by_mesh(combined_solids, mesh);
                }
            } else if (node.type == "surface") {
                // B. 2D Planar Surfaces
                // World plane is defined robustly by transforming local Z=0 plane using node.tf
                EK::Plane_3 world_plane = node.tf.transform(EK::Plane_3(0, 0, 1, 0));

                bool found_group = false;
                for (auto& [existing_plane, gps] : plane_groups) {
                    // Check if they represent the same 3D plane
                    if (std::abs(CGAL::to_double(CGAL::squared_distance(existing_plane, world_plane.point()))) < 1e-6) {
                        Vector_3 n1 = existing_plane.orthogonal_vector();
                        Vector_3 n2 = world_plane.orthogonal_vector();
                        if (CGAL::cross_product(n1, n2).squared_length() < 1e-6) {
                            Matrix project_tf = Matrix::lookAt(existing_plane.point(), existing_plane.orthogonal_vector());
                            boolean::Engine::add_geometry_to_gps(node.geo, project_tf * node.tf, gps);
                            found_group = true;
                            break;
                        }
                    }
                }

                if (!found_group) {
                    Matrix project_tf = Matrix::lookAt(world_plane.point(), world_plane.orthogonal_vector());
                    boolean::General_polygon_set_2 gps;
                    boolean::Engine::add_geometry_to_gps(node.geo, project_tf * node.tf, gps);
                    plane_groups.push_back({world_plane, gps});
                }
            }
        }

        // Process Gap Nodes
        for (const auto& gap : gap_nodes) {
            if (gap.geo.vertices.empty()) continue;

            if (gap.type == "closed") {
                // Closed volume gaps cut solids
                boolean::Surface_mesh gap_mesh = boolean::Engine::geometry_to_mesh(gap.geo);
                boolean::Engine::transform_mesh(gap_mesh, gap.tf);
                if (combined_solids.number_of_vertices() > 0) {
                    boolean::Engine::cut_mesh_by_mesh(combined_solids, gap_mesh);
                }
            } else if (gap.type == "surface") {
                // Planar/2D surface gaps cut coplanar groups and solids
                EK::Plane_3 gap_world_plane = gap.tf.transform(EK::Plane_3(0, 0, 1, 0));

                if (combined_solids.number_of_vertices() > 0) {
                    boolean::Engine::cut_mesh_by_plane(combined_solids, gap_world_plane);
                }
                for (auto& [existing_plane, gps] : plane_groups) {
                    if (std::abs(CGAL::to_double(CGAL::squared_distance(existing_plane, gap_world_plane.point()))) < 1e-6) {
                        Vector_3 n1 = existing_plane.orthogonal_vector();
                        Vector_3 n2 = gap_world_plane.orthogonal_vector();
                        if (CGAL::cross_product(n1, n2).squared_length() < 1e-6) {
                            Matrix project_tf = Matrix::lookAt(existing_plane.point(), existing_plane.orthogonal_vector());
                            boolean::General_polygon_set_2 gap_gps;
                            boolean::Engine::add_geometry_to_gps(gap.geo, project_tf * gap.tf, gap_gps);
                            gps.difference(gap_gps);
                        }
                    }
                }
            }
        }

        // 3. Rehydrate each non-empty joined class into discrete Shapes
        std::vector<Shape> output_shapes;

        // A. Closed Solids
        if (combined_solids.number_of_vertices() > 0) {
            Shape s_solid;
            s_solid.geometry = vfs->materialize<Geometry>(boolean::Engine::mesh_to_geometry(combined_solids));
            s_solid.tf = Matrix::identity();
            s_solid.add_tag("type", "closed");
            output_shapes.push_back(s_solid);
        }

        // B. Coplanar Surfaces
        for (auto& [world_plane, gps] : plane_groups) {
            if (gps.is_empty()) continue;
            Matrix rehydrate_tf = Matrix::lookAt(world_plane.point(), world_plane.orthogonal_vector()).inverse();
            Geometry plane_geo = boolean::Engine::gps_to_geometry(gps);
            
            Shape s_plane;
            s_plane.geometry = vfs->materialize<Geometry>(plane_geo);
            s_plane.tf = rehydrate_tf;
            s_plane.add_tag("type", "surface");
            output_shapes.push_back(s_plane);
        }

        // 4. Final output routing
        if (output_shapes.empty()) {
            vfs->write(fulfilling.with_output("$out"), Shape());
        } else if (output_shapes.size() == 1) {
            vfs->write(fulfilling.with_output("$out"), output_shapes[0]);
        } else {
            Shape out_group;
            out_group.components = output_shapes;
            out_group.add_tag("type", "group");
            vfs->write(fulfilling.with_output("$out"), out_group);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/fuse"}, 
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to fuse."}}}}},
            {"arguments", nlohmann::json::array({ 
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}
            })}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void fuse_init(fs::VFSNode* vfs) {
    Processor::register_op<FuseOp<>, Shape, std::vector<Shape>>(vfs, "jot/fuse");
}

} // namespace geo
} // namespace jotcad
