#pragma once
#include <vector>

namespace jotcad {
namespace geo {
namespace rig {

struct VertexInfluence {
    int joint_id;
    double weight;
};

struct VertexWeights {
    std::vector<VertexInfluence> influences;

    void normalize() {
        double sum = 0.0;
        for (const auto& inf : influences) {
            sum += inf.weight;
        }
        if (sum > 1e-9) {
            for (auto& inf : influences) {
                inf.weight /= sum;
            }
        } else if (!influences.empty()) {
            // Fallback: assign equal weight if sum is zero
            double val = 1.0 / influences.size();
            for (auto& inf : influences) {
                inf.weight = val;
            }
        }
    }
};

struct SkinWeights {
    std::vector<VertexWeights> vertex_weights_list;

    void normalize_all() {
        for (auto& vw : vertex_weights_list) {
            vw.normalize();
        }
    }
};

} // namespace rig
} // namespace geo
} // namespace jotcad
