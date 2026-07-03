#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <vector>
#include <memory>
#include "element.h"

// Orchestrator class running simulations and managing elements
class Orchestrator {
private:
    Grid grid;
    std::vector<std::shared_ptr<Element>> elements;
public:
    Orchestrator(int sz) : grid(sz) {}
    
    Grid& get_grid() { return grid; }

    template<typename T, typename... Args>
    std::shared_ptr<T> add(Args&&... args) {
        auto el = std::make_shared<T>(std::forward<Args>(args)...);
        elements.push_back(el);
        return el;
    }

    template<typename T>
    std::shared_ptr<T> get() {
        for (auto& el : elements) {
            auto casted = std::dynamic_pointer_cast<T>(el);
            if (casted) return casted;
        }
        return nullptr;
    }

    void run(int steps, float dt) {
        for (int step = 0; step < steps; ++step) {
            for (auto& element : elements) {
                element->step(grid, dt, step, steps);
            }
        }
    }
};

#endif // ORCHESTRATOR_H
