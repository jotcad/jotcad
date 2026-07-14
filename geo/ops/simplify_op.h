#pragma once
#include "protocols.h"
#include "processor.h"
#include "../boolean/engine.h"
#include <CGAL/Surface_mesh_simplification/edge_collapse.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Edge_count_stop_predicate.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Edge_count_ratio_stop_predicate.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/GarlandHeckbert_policies.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Constrained_placement.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Bounded_normal_change_placement.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/repair.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SimplifyOp : P {
    static constexpr const char* path = "jot/simplify";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double ratio = 0.5, int count = 0, double threshold_tau = 60.0/360.0) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        
        typedef CGAL::Surface_mesh<IK::Point_3> InexactMesh;
        typedef boost::graph_traits<InexactMesh>::edge_descriptor edge_descriptor;
        typedef boost::graph_traits<InexactMesh>::vertex_descriptor vertex_descriptor;
        InexactMesh imesh = boolean::Engine::geometry_to_mesh_ik(geo);

        // 2. Mark Sharp Edges as Constrained
        InexactMesh::Property_map<edge_descriptor, bool> is_constrained = 
            imesh.add_property_map<edge_descriptor, bool>("e:is_constrained", false).first;
        
        int constrained_count = 0;
        for(edge_descriptor e : imesh.edges()) {
            if(CGAL::is_border(e, imesh)) {
                is_constrained[e] = true;
                constrained_count++;
            } else {
                auto h = imesh.halfedge(e);
                auto p1 = imesh.point(imesh.source(h));
                auto p2 = imesh.point(imesh.target(h));
                auto p3 = imesh.point(imesh.target(imesh.next(h)));
                auto p4 = imesh.point(imesh.target(imesh.next(imesh.opposite(h))));
                
                // CGAL::approximate_dihedral_angle returns degrees, where 180 is flat.
                double angle_deg = CGAL::to_double(CGAL::approximate_dihedral_angle(p1, p2, p3, p4));
                double deviation_deg = std::abs(180.0 - std::abs(angle_deg));
                
                if(deviation_deg > threshold_tau * 360.0) {
                    is_constrained[e] = true;
                    constrained_count++;
                }
            }
        }

        // 3. Set up Policies
        namespace SMS = CGAL::Surface_mesh_simplification;
        typedef SMS::GarlandHeckbert_policies<InexactMesh, IK> GHPolicies;
        GHPolicies gh_policies(imesh);
        
        typedef typename GHPolicies::Get_placement BasePlacement;
        typedef SMS::Constrained_placement<BasePlacement, InexactMesh::Property_map<edge_descriptor, bool>> ConstrainedPlacement;
        typedef SMS::Bounded_normal_change_placement<ConstrainedPlacement> SafePlacement;
        
        ConstrainedPlacement cp(is_constrained, gh_policies.get_placement());
        SafePlacement placement(cp);

        // 4. Execute Collapse
        if (count > 0) {
            SMS::Edge_count_stop_predicate<InexactMesh> stop(count);
            SMS::edge_collapse(imesh, stop,
                CGAL::parameters::get_cost(gh_policies.get_cost())
                                 .get_placement(placement)
                                 .edge_is_constrained_map(is_constrained));
        } else {
            SMS::Edge_count_ratio_stop_predicate<InexactMesh> stop(ratio);
            SMS::edge_collapse(imesh, stop,
                CGAL::parameters::get_cost(gh_policies.get_cost())
                                 .get_placement(placement)
                                 .edge_is_constrained_map(is_constrained));
        }

        // 5. Convert back to Geometry
        Geometry res;
        std::map<vertex_descriptor, int> v_map;
        for(auto v : imesh.vertices()) {
            v_map[v] = (int)res.vertices.size();
            auto p = imesh.point(v);
            res.vertices.push_back({p.x(), p.y(), p.z()});
        }
        for(auto f : imesh.faces()) {
            Geometry::Face face;
            std::vector<int> loop;
            for(auto v : imesh.vertices_around_face(imesh.halfedge(f))) {
                loop.push_back(v_map[v]);
            }
            if (loop.size() >= 3) {
                face.loops.push_back(loop);
                res.faces.push_back(face);
            }
        }
        
        Shape out = in;
        res.triangulate();
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "ratio", "count", "threshold"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/simplify"},
            {"description", "Simplifies a mesh using edge-collapse reduction while preserving sharp features."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "ratio"}, {"type", "jot:number"}, {"default", 0.5}, {"description", "Target face reduction ratio (0.1 = 10% of original faces)"}},
                {{"name", "count"}, {"type", "jot:number"}, {"default", 0}, {"description", "Explicit target face count (if > 0, ratio is ignored)"}},
                {{"name", "threshold"}, {"type", "jot:number"}, {"default", 60.0/360.0}, {"description", "Dihedral angle threshold for sharp feature preservation in turns (tau)."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void simplify_init(fs::VFSNode* vfs) {
    Processor::register_op<SimplifyOp<>, Shape, double, int, double>(vfs, "jot/simplify");
}

} // namespace geo
} // namespace jotcad
