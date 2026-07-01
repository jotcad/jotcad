#include "simulator.h"
#include <iostream>

int main() {
    jotcad::sim_rivers::Simulator sim(256, 256);
    sim.initialize(1337);
    
    // Check initial water frequency
    sim.step(0.5f);
    
    std::cout << "Successfully ran a single step!" << std::endl;
    return 0;
}
