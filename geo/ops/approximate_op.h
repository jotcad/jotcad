#pragma once
#include "protocols.h"
#include "processor.h"
#include "../boolean/engine.h"
#include <CGAL/Surface_mesh_approximation/approximate_triangle_mesh.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ApproximateOp : P {
    static constexpr const char* path = "jot/approximate";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, int max_proxies = 40) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        
        typedef CGAL::Surface_mesh<IK::Point_3> InexactMesh;
        InexactMesh imesh = boolean::Engine::geometry_to_mesh_ik(geo);

        // 2. Prepare containers for output
        std::vector<IK::Point_3> anchors;
        std::vector<std::array<std::size_t, 3>> triangles;

        // 3. Run Variational Shape Approximation
        namespace VSA = CGAL::Surface_mesh_approximation;
        VSA::approximate_triangle_mesh(imesh,
            CGAL::parameters::verbose_level(VSA::SILENT)
                             .max_number_of_proxies(max_proxies)
                             .anchors(std::back_inserter(anchors))
                             .triangles(std::back_inserter(triangles)));

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

    static std::vector<std::string> argument_keys() { return {"$in", "max_proxies"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/approximate"},
            {"description", "Approximates a triangle mesh using Variational Shape Approximation (VSA) to create a simplified, flat-proxy representation."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "max_proxies"}, {"type", "jot:number"}, {"default", 40}, {"description", "Target number of planar proxies (clusters) to approximate the shape."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void approximate_init(fs::VFSNode* vfs) {
    Processor::register_op<ApproximateOp<>, Shape, int>(vfs, "jot/approximate");
}

} // namespace geo
} // namespace jotcad
