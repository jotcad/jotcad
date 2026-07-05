#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include "ops/deform_op.h"
#include "joint.h"
#include "weights.h"
#include "skinning.h"

namespace jotcad {
namespace geo {
namespace rig {

template <typename P = JotVfsProtocol>
struct RigOp : P {
    
    static void extract_skeleton(const Shape& s, int parent_id, Skeleton& skeleton) {
        std::string type = s.tags.value("type", "");
        if (type == "joint") {
            int joint_id = s.tags.value("joint_id", -1);
            if (joint_id != -1) {
                Joint j(s.tags.value("name", ""), joint_id, parent_id);
                
                std::string bind_tf_str = s.tags.value("bind_tf", "1 0 0 0 0 1 0 0 0 0 1 0");
                j.world_bind_tf = Matrix::from_vec(bind_tf_str);
                j.inv_bind_tf = j.world_bind_tf.inverse();
                j.world_tf = s.tf;
                
                skeleton.joints.push_back(j);
                
                for (const auto& child : s.components) {
                    extract_skeleton(child, joint_id, skeleton);
                }
                return;
            }
        }
        
        for (const auto& child : s.components) {
            extract_skeleton(child, parent_id, skeleton);
        }
    }

    static void find_solid_components(Shape& s, std::vector<Shape*>& solids) {
        if (s.geometry.has_value() && s.tags.value("type", "") == "solid") {
            solids.push_back(&s);
        }
        for (auto& child : s.components) {
            find_solid_components(child, solids);
        }
    }

    static void compute_proximity_weights(const Geometry& geo, const Skeleton& skeleton, SkinWeights& weights) {
        weights.vertex_weights_list.resize(geo.vertices.size());
        
        struct Bone {
            int joint_id;
            EK::Point_3 p0, p1;
        };
        std::vector<Bone> bones;
        
        for (const auto& joint : skeleton.joints) {
            if (joint.parent_id == -1) {
                bones.push_back({joint.id, joint.world_bind_tf.transform(EK::Point_3(0,0,0)), joint.world_bind_tf.transform(EK::Point_3(0,0,0))});
            } else {
                int parent_idx = skeleton.find_joint_index(joint.parent_id);
                if (parent_idx != -1) {
                    EK::Point_3 parent_pos = skeleton.joints[parent_idx].world_bind_tf.transform(EK::Point_3(0,0,0));
                    EK::Point_3 child_pos = joint.world_bind_tf.transform(EK::Point_3(0,0,0));
                    bones.push_back({joint.id, parent_pos, child_pos});
                }
            }
        }

        for (size_t i = 0; i < geo.vertices.size(); ++i) {
            const auto& v = geo.vertices[i];
            EK::Point_3 pt(v.x, v.y, v.z);
            
            std::vector<std::pair<int, double>> distances;
            for (const auto& bone : bones) {
                double dist = 0.0;
                if (bone.p0 == bone.p1) {
                    dist = CGAL::to_double(CGAL::squared_distance(pt, bone.p0));
                } else {
                    CGAL::Segment_3<EK> seg(bone.p0, bone.p1);
                    dist = CGAL::to_double(CGAL::squared_distance(pt, seg));
                }
                distances.push_back({bone.joint_id, dist});
            }
            
            std::sort(distances.begin(), distances.end(), [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
            
            int limit = std::min((int)distances.size(), 4);
            VertexWeights& vw = weights.vertex_weights_list[i];
            for (int k = 0; k < limit; ++k) {
                double dist = std::sqrt(distances[k].second);
                double affinity = 1.0 / (dist * dist + 1e-6);
                vw.influences.push_back({distances[k].first, affinity});
            }
            vw.normalize();
        }
    }

    struct RigOperation {
        static constexpr const char* path = "jot/rig";
        
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, int pbd_iterations = 40) {
            Skeleton skeleton;
            extract_skeleton(in, -1, skeleton);
            
            if (skeleton.joints.empty()) {
                vfs->write(fulfilling.with_output("$out"), in);
                return;
            }

            Shape out = in;
            out.tags["type"] = "rig";
            
            std::vector<Shape*> solids;
            find_solid_components(out, solids);
            
            for (auto* solid : solids) {
                if (!solid->geometry.has_value()) continue;
                
                Geometry rest_geo = vfs->read<Geometry>(solid->geometry.value());
                SkinWeights weights;
                
                compute_proximity_weights(rest_geo, skeleton, weights);
                
                Geometry posed_geo = rest_geo;
                for (size_t i = 0; i < posed_geo.vertices.size(); ++i) {
                    EK::Point_3 rest_pt(rest_geo.vertices[i].x, rest_geo.vertices[i].y, rest_geo.vertices[i].z);
                    EK::Point_3 posed_pt = Skinning::apply_lbs(rest_pt, skeleton, weights.vertex_weights_list[i]);
                    posed_geo.vertices[i] = {posed_pt.x(), posed_pt.y(), posed_pt.z()};
                }
                
                if (pbd_iterations > 0) {
                    typename DeformOp<P>::Mesh rest_mesh = boolean::Engine::geometry_to_mesh_ik(rest_geo);
                    typename DeformOp<P>::Mesh posed_mesh = boolean::Engine::geometry_to_mesh_ik(posed_geo);
                    
                    auto constraints = DeformOp<P>::build_spring_network(rest_mesh);
                    DeformOp<P>::execute_pbd(posed_mesh, constraints, pbd_iterations);
                    CGAL::Polygon_mesh_processing::tangential_relaxation(posed_mesh);
                    
                    posed_geo = boolean::Engine::mesh_to_geometry_ik(posed_mesh);
                }
                
                solid->geometry = vfs->materialize<Geometry>(posed_geo);
            }
            
            vfs->write(fulfilling.with_output("$out"), out);
        }

        static std::vector<std::string> argument_keys() { return {"$in", "pbd_iterations"}; }
        
        static typename P::json schema() {
            return {
                {"path", "jot/rig"},
                {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
                {"arguments", nlohmann::json::array({
                    {{"name", "pbd_iterations"}, {"type", "jot:number"}, {"default", 40}}
                })},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };
};

inline void rig_init(fs::VFSNode* vfs) {
    Processor::register_op<RigOp<>::RigOperation, Shape, int>(vfs, "jot/rig");
}

} // namespace rig
} // namespace geo
} // namespace jotcad
