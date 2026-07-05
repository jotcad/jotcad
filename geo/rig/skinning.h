#pragma once
#include "joint.h"
#include "weights.h"

namespace jotcad {
namespace geo {
namespace rig {

class Skinning {
public:
    static EK::Point_3 apply_lbs(const EK::Point_3& rest_pos, const Skeleton& skeleton, const VertexWeights& weights) {
        if (weights.influences.empty()) {
            return rest_pos;
        }

        FT x(0), y(0), z(0);
        double total_weight = 0.0;

        for (const auto& inf : weights.influences) {
            int idx = skeleton.find_joint_index(inf.joint_id);
            if (idx == -1) continue;
            const auto& joint = skeleton.joints[idx];
            
            // Combined matrix = world_tf * inv_bind_tf
            Matrix m = joint.world_tf * joint.inv_bind_tf;
            EK::Point_3 transformed = m.transform(rest_pos);
            
            FT w(inf.weight);
            x += transformed.x() * w;
            y += transformed.y() * w;
            z += transformed.z() * w;
            total_weight += inf.weight;
        }

        if (total_weight < 1e-9) {
            return rest_pos;
        }

        // Normalize by total weight in case weights did not sum to 1.0
        FT norm(total_weight);
        return EK::Point_3(x / norm, y / norm, z / norm);
    }
};

} // namespace rig
} // namespace geo
} // namespace jotcad
