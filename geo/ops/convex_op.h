#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/convex_decomposition_3.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/boost/graph/copy_face_graph.h>
#include <CGAL/Nef_3/SNC_indexed_items.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/convex_hull_3.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ConvexOp : P {
    static constexpr const char* path = "jot/convex";

    static Shape decompose_shape(fs::VFSNode* vfs, const Shape& s) {
        Shape out = s;
        
        // 1. Process child components recursively first
        std::vector<Shape> processed_components;
        for (const auto& child : s.components) {
            processed_components.push_back(decompose_shape(vfs, child));
        }

        // 2. Decompose the main geometry of this shape
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            std::string type = s.tags.value("type", "");
            if (type == "closed" && !geo.vertices.empty()) {
                boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
                if (!CGAL::is_strongly_convex_3(mesh)) {
                    try {
                        CGAL::Nef_polyhedron_3<EK, CGAL::SNC_indexed_items> nef(mesh);
                        CGAL::convex_decomposition_3(nef);
                        
                        std::vector<Shape> parts;
                        auto ci = ++nef.volumes_begin();
                        for (; ci != nef.volumes_end(); ++ci) {
                            if (ci->mark()) {
                                CGAL::Polyhedron_3<EK> poly;
                                nef.convert_inner_shell_to_polyhedron(ci->shells_begin(), poly);
                                boolean::Surface_mesh part_mesh;
                                CGAL::copy_face_graph(poly, part_mesh);
                                CGAL::Polygon_mesh_processing::triangulate_faces(part_mesh);
                                
                                Geometry part_geo = boolean::Engine::mesh_to_geometry(part_mesh);
                                if (!part_geo.vertices.empty()) {
                                    Shape part_s;
                                    part_s.geometry = vfs->materialize(part_geo);
                                    part_s.tf = Matrix::identity();
                                    part_s.add_tag("type", "closed");
                                    parts.push_back(part_s);
                                }
                            }
                        }
                        
                        if (parts.size() > 1) {
                            Shape group_s;
                            group_s.components = parts;
                            // Preserve child components (like ghost cutter components) by appending them to the group
                            group_s.components.insert(group_s.components.end(), processed_components.begin(), processed_components.end());
                            group_s.tf = s.tf;
                            group_s.tags = s.tags;
                            group_s.add_tag("type", "group");
                            return group_s;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[ConvexOp] Convex decomposition failed: " << e.what() << ", returning original shape" << std::endl;
                    }
                }
            }
        }
        
        out.components = processed_components;
        return out;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = decompose_shape(vfs, in);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/convex"},
            {"description", "Decomposes a concave solid geometry into a group of convex sub-polyhedra."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to decompose."}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void convex_init(fs::VFSNode* vfs) {
    Processor::register_op<ConvexOp<>, Shape>(vfs, "jot/convex");
}

} // namespace geo
} // namespace jotcad
