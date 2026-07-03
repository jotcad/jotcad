#ifndef PHASE_H
#define PHASE_H

#include <vector>
#include <memory>
#include <string>
#include "element.h"

// Phase abstraction representing a distinct simulation era (e.g. Tectonic vs. Hydrological)
class Phase {
public:
    std::string name;
    float dt;
    int steps;
    int target_grid_size; // Resolution scale for this phase
    std::vector<std::shared_ptr<Element>> elements;

    Phase(std::string n, float delta_t, int s, int target_sz = -1) 
        : name(n), dt(delta_t), steps(s), target_grid_size(target_sz) {}

    template<typename T, typename... Args>
    std::shared_ptr<T> add(Args&&... args) {
        auto el = std::make_shared<T>(std::forward<Args>(args)...);
        elements.push_back(el);
        return el;
    }
};

#endif // PHASE_H
