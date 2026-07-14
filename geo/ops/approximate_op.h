#pragma once
#include "protocols.h"
#include "processor.h"
#include "../boolean/engine.h"
#include <CGAL/Surface_mesh_approximation/approximate_triangle_mesh.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <cmath>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ApproximateOp : P {
    static constexpr const char* path = "jot/approximate";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, int max_proxies = 40, double tolerance = -1.0) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        
        typedef CGAL::Surface_mesh<IK::Point_3> InexactMesh;
        InexactMesh imesh = boolean::Engine::geometry_to_mesh_ik(geo);

        int current_proxies = max_proxies;
        bool satisfied = false;

        std::vector<IK::Point_3> anchors;
        std::vector<std::array<std::size_t, 3>> triangles;

        while (!satisfied) {
            anchors.clear();
            triangles.clear();

            // 3. Run Variational Shape Approximation
            namespace VSA = CGAL::Surface_mesh_approximation;
            VSA::approximate_triangle_mesh(imesh,
                CGAL::parameters::verbose_level(VSA::SILENT)
                                 .max_number_of_proxies(current_proxies)
                                 .anchors(std::back_inserter(anchors))
                                 .triangles(std::back_inserter(triangles)));

            if (tolerance <= 0.0 || current_proxies >= 400) {
                satisfied = true;
                break;
            }

            // Reconstruct a temporary Surface_mesh to query distance
            CGAL::Surface_mesh<IK::Point_3> approx_mesh;
            std::vector<CGAL::Surface_mesh<IK::Point_3>::Vertex_index> v_indices;
            for (const auto& p : anchors) {
                v_indices.push_back(approx_mesh.add_vertex(p));
            }
            for (const auto& t : triangles) {
                approx_mesh.add_face(v_indices[t[0]], v_indices[t[1]], v_indices[t[2]]);
            }

            if (approx_mesh.number_of_faces() == 0) {
                current_proxies += 10;
                continue;
            }

            // Build AABB tree for distance check
            typedef CGAL::AABB_face_graph_triangle_primitive<CGAL::Surface_mesh<IK::Point_3>> Primitive;
            typedef CGAL::AABB_traits<IK, Primitive> Traits;
            typedef CGAL::AABB_tree<Traits> Tree;
            Tree tree(faces(approx_mesh).first, faces(approx_mesh).second, approx_mesh);
            tree.build();

            double max_sq_dist = 0.0;
            for (auto v : imesh.vertices()) {
                auto p = imesh.point(v);
                double sq_dist = CGAL::to_double(tree.squared_distance(p));
                if (sq_dist > max_sq_dist) {
                    max_sq_dist = sq_dist;
                }
            }

            double max_dist = std::sqrt(max_sq_dist);
            std::cout << "[Approximate] proxies: " << current_proxies << ", error: " << max_dist << " (target: " << tolerance << ")" << std::endl;

            if (max_dist <= tolerance) {
                satisfied = true;
            } else {
                current_proxies += 10;
            }
        }

        // 3.5. Orient the output triangle soup to guarantee closed manifold boundaries
        CGAL::Polygon_mesh_processing::orient_polygon_soup(anchors, triangles);

        // 4. Convert back to Geometry
        Geometry res;
        for(const auto& p : anchors) {
            res.vertices.push_back({p.x(), p.y(), p.z()});
        }
        for(const auto& t : triangles) {
            Geometry::Face face;
            face.loops.push_back({(int)t[0], (int)t[1], (int)t[2]});
            res.faces.push_back(face);
        }
        
        Shape out = in;
        res.triangulate();
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "max_proxies", "tolerance"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/approximate"},
            {"description", "Approximates a triangle mesh using Variational Shape Approximation (VSA) to create a simplified, flat-proxy representation."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "max_proxies"}, {"type", "jot:number"}, {"default", 40}, {"description", "Target number of planar proxies (clusters) to approximate the shape."}},
                {{"name", "tolerance"}, {"type", "jot:number"}, {"default", -1.0}, {"description", "Maximum allowed geometric error tolerance. If > 0, proxy count is dynamically increased to satisfy it."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void approximate_init(fs::VFSNode* vfs) {
    Processor::register_op<ApproximateOp<>, Shape, int, double>(vfs, "jot/approximate");
}

} // namespace geo
} // namespace jotcad
