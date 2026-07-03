#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <vector>
#include <memory>
#include <iostream>
#include <typeindex>
#include "grid.h"
#include "phase.h"

// Orchestrator class running multi-phase simulations and managing elements
class Orchestrator {
private:
    Grid grid;
    std::vector<std::unique_ptr<Phase>> phases;
public:
    Orchestrator(int sz) : grid(sz) {}
    
    Grid& get_grid() { return grid; }

    Phase* add_phase(const std::string& name, float dt, int steps) {
        phases.push_back(std::make_unique<Phase>(name, dt, steps));
        return phases.back().get();
    }

    const std::vector<std::unique_ptr<Phase>>& get_phases() const {
        return phases;
    }

    void run() {
        for (auto& phase : phases) {
            std::cout << "--- Starting Phase: " << phase->name << " (" << phase->steps << " steps, dt = " << phase->dt << ") ---" << std::endl;
            
            // Allocate all sparse fields required by the elements in this phase
            for (auto& element : phase->elements) {
                for (std::type_index type : element->get_required_fields()) {
                    grid.request_field_by_type_index(type);
                }
            }

            for (int step = 0; step < phase->steps; ++step) {
                for (auto& element : phase->elements) {
                    element->step(grid, phase->dt, step, phase->steps);
                }
            }
        }
    }
};

#endif // ORCHESTRATOR_H
