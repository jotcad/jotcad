#ifndef HEX_PHASE_H
#define HEX_PHASE_H

#include <vector>
#include <memory>
#include <string>
#include "hex_element.h"

// Phase abstraction representing a distinct simulation era on a hexagonal grid
class HexPhase {
public:
    std::string name;
    float dt;
    int steps;
    int target_size_q; // Target resolution size_q for this phase (-1 for no scaling)
    int target_size_r; // Target resolution size_r for this phase (-1 for no scaling)
    std::vector<std::shared_ptr<HexElement>> elements;

    HexPhase(std::string n, float delta_t, int s, int target_sq = -1, int target_sr = -1) 
        : name(n), dt(delta_t), steps(s), target_size_q(target_sq), target_size_r(target_sr) {}

    template<typename T, typename... Args>
    std::shared_ptr<T> add(Args&&... args) {
        auto el = std::make_shared<T>(std::forward<Args>(args)...);
        elements.push_back(el);
        return el;
    }
};

#endif // HEX_PHASE_H
