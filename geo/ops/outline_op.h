#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OutlineOp : P {
    static constexpr const char* path = "jot/outline";

    static Shape outline_shape(fs::VFSNode* vfs, const Shape& in) {
        Shape out = in;
        if (in.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(in.geometry.value());
            
            // Convert to CGAL ExactMesh
            boolean::ExactMesh mesh = boolean::Engine::geometry_to_mesh(geo);
            
            std::cout << "[DEBUG OutlineOp] geometry_to_mesh: vertices = " << mesh.number_of_vertices() 
                      << ", faces = " << mesh.number_of_faces() 
                      << ", edges = " << mesh.number_of_edges() << std::endl;

            // Build temporary face normals
            std::map<boolean::ExactMesh::Face_index, Vector_3> face_normals;
            for (auto f : mesh.faces()) {
                auto he = mesh.halfedge(f);
                Point_3 p0 = mesh.point(mesh.source(he));
                Point_3 p1 = mesh.point(mesh.target(he));
                Point_3 p2 = mesh.point(mesh.target(mesh.next(he)));
                Vector_3 n = CGAL::cross_product(p1 - p0, p2 - p0);
                face_normals[f] = n;
            }

            Geometry res;
            // Iterate over all edges in the mesh
            for (auto e : mesh.edges()) {
                auto h1 = mesh.halfedge(e, 0);
                auto h2 = mesh.halfedge(e, 1);
                
                bool is_feature = false;
                if (mesh.is_border(e)) {
                    is_feature = true;
                } else {
                    auto f1 = mesh.face(h1);
                    auto f2 = mesh.face(h2);
                    if (f1 == boolean::ExactMesh::null_face() || f2 == boolean::ExactMesh::null_face()) {
                        is_feature = true;
                    } else {
                        const auto& n1 = face_normals[f1];
                        const auto& n2 = face_normals[f2];
                        if (n1.squared_length() == 0 || n2.squared_length() == 0) {
                            is_feature = true;
                        } else {
                            bool parallel = CGAL::cross_product(n1, n2).squared_length() == 0;
                            bool same_dir = (n1 * n2) > 0;
                            if (!parallel || !same_dir) {
                                is_feature = true;
                            }
                        }
                    }
                }

                if (is_feature) {
                    auto p_a = mesh.point(mesh.source(h1));
                    auto p_b = mesh.point(mesh.target(h1));
                    
                    int base = (int)res.vertices.size();
                    res.vertices.push_back({p_a.x(), p_a.y(), p_a.z()});
                    res.vertices.push_back({p_b.x(), p_b.y(), p_b.z()});
                    res.segments.push_back({base, base + 1});
                }
            }

            std::cout << "[DEBUG OutlineOp] res geometry: vertices = " << res.vertices.size() 
                      << ", segments = " << res.segments.size() << std::endl;

            out.geometry = vfs->materialize<Geometry>(res);
        }

        out.components.clear();
        for (const auto& child : in.components) {
            out.components.push_back(outline_shape(vfs, child));
        }
        return out;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = outline_shape(vfs, in);
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/outline"},
            {"description", "Extracts the boundary edges of the input shape as line segments."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to outline."}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The resulting wireframe/outline shape."}}}}}
        };
    }
};

static void outline_init(fs::VFSNode* vfs) {
    Processor::register_op<OutlineOp<>, Shape>(vfs, "jot/outline");
}

} // namespace geo
} // namespace jotcad
