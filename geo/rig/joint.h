#pragma once
#include <string>
#include <vector>
#include "../math/matrix.h"

namespace jotcad {
namespace geo {
namespace rig {

struct Joint {
    std::string name;
    int id;
    int parent_id;
    
    // Bind-pose transforms (rest state)
    Matrix local_bind_tf;
    Matrix world_bind_tf;
    Matrix inv_bind_tf;
    
    // Current animated/posed transforms
    Matrix local_tf;
    Matrix world_tf;

    Joint() : id(-1), parent_id(-1) {}
    Joint(const std::string& n, int i, int p) : name(n), id(i), parent_id(p) {}
};

struct Skeleton {
    std::vector<Joint> joints;

    int find_joint_index(int id) const {
        for (size_t i = 0; i < joints.size(); ++i) {
            if (joints[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }

    void update_world_transforms() {
        for (auto& joint : joints) {
            if (joint.parent_id == -1) {
                joint.world_tf = joint.local_tf;
            } else {
                int parent_idx = find_joint_index(joint.parent_id);
                if (parent_idx != -1) {
                    joint.world_tf = joints[parent_idx].world_tf * joint.local_tf;
                } else {
                    joint.world_tf = joint.local_tf;
                }
            }
        }
    }

    void compute_inverse_bind_matrices() {
        for (auto& joint : joints) {
            if (joint.parent_id == -1) {
                joint.world_bind_tf = joint.local_bind_tf;
            } else {
                int parent_idx = find_joint_index(joint.parent_id);
                if (parent_idx != -1) {
                    joint.world_bind_tf = joints[parent_idx].world_bind_tf * joint.local_bind_tf;
                } else {
                    joint.world_bind_tf = joint.local_bind_tf;
                }
            }
            joint.inv_bind_tf = joint.world_bind_tf.inverse();
        }
    }
};

} // namespace rig
} // namespace geo
} // namespace jotcad
