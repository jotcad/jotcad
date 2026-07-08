#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <vector>
#include <memory>
#include <iostream>
#include <typeindex>
#include <functional>
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

    Phase* add_phase(const std::string& name, float dt, int steps, int target_sz = -1) {
        phases.push_back(std::make_unique<Phase>(name, dt, steps, target_sz));
        return phases.back().get();
    }

    const std::vector<std::unique_ptr<Phase>>& get_phases() const {
        return phases;
    }

    void run_phase(Phase* phase, std::function<void(int step)> on_step = nullptr) {
        // Apply scale coherence dynamically at phase boundary
        if (phase->target_grid_size > 0 && grid.size != phase->target_grid_size) {
            std::cout << ">>> Phase Transition: Rescaling Grid from " << grid.size 
                      << " to " << phase->target_grid_size << " for phase '" 
                      << phase->name << "' <<<" << std::endl;
            grid.upscale_in_place(phase->target_grid_size);
        }

        // Allocate all sparse fields required by the elements in this phase
        for (auto& element : phase->elements) {
            for (std::type_index type : element->get_required_fields()) {
                grid.request_field_by_type_index(type);
            }
        }

        // Step elements
        for (int step = 0; step < phase->steps; ++step) {
            for (auto& element : phase->elements) {
                element->step(grid, phase->dt, step, phase->steps);
            }
            if (on_step) {
                on_step(step);
            }
        }
    }

    void run() {
        for (auto& phase : phases) {
            std::cout << "--- Starting Phase: " << phase->name << " (" << phase->steps << " steps, dt = " << phase->dt << ") ---" << std::endl;
            run_phase(phase.get());
        }
    }
};

#endif // ORCHESTRATOR_H
