#ifndef HEX_ORCHESTRATOR_H
#define HEX_ORCHESTRATOR_H

#include <vector>
#include <memory>
#include <iostream>
#include <typeindex>
#include <functional>
#include "hex_grid.h"
#include "hex_phase.h"

// Orchestrator class running multi-phase simulations and managing elements on HexGrid
class HexOrchestrator {
private:
    HexGrid grid;
    std::vector<std::unique_ptr<HexPhase>> phases;
public:
    HexOrchestrator(int sq, int sr) : grid(sq, sr) {}
    
    HexGrid& get_grid() { return grid; }

    HexPhase* add_phase(const std::string& name, float dt, int steps, int target_sq = -1, int target_sr = -1) {
        phases.push_back(std::make_unique<HexPhase>(name, dt, steps, target_sq, target_sr));
        return phases.back().get();
    }

    const std::vector<std::unique_ptr<HexPhase>>& get_phases() const {
        return phases;
    }

    void run_phase(HexPhase* phase, std::function<void(int step)> on_step = nullptr) {
        // Apply scale coherence dynamically at phase boundary
        if (phase->target_size_q > 0 && phase->target_size_r > 0 && 
            (grid.size_q != phase->target_size_q || grid.size_r != phase->target_size_r)) {
            std::cout << ">>> Hex Phase Transition: Rescaling Grid from " << grid.size_q << "x" << grid.size_r 
                      << " to " << phase->target_size_q << "x" << phase->target_size_r << " for phase '" 
                      << phase->name << "' <<<" << std::endl;
            grid.upscale_in_place(phase->target_size_q, phase->target_size_r);
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
            std::cout << "--- Starting Hex Phase: " << phase->name << " (" << phase->steps << " steps, dt = " << phase->dt << ") ---" << std::endl;
            run_phase(phase.get());
        }
    }
};

#endif // HEX_ORCHESTRATOR_H
