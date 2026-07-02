#include <iostream>
#include <algorithm>
#include <cmath>

struct Particle {
    float x, y;
    float vx, vy;
};

int main() {
    // 10x10 Flat elevation grid (height = 0.0 everywhere)
    float grid[10][10] = {0.0f};

    // Initialize particle in the center at rest
    Particle p = {5.0f, 5.0f, 0.0f, 0.0f};
    
    std::cout << "Starting Simple Lagrangian Model (10x10 flat grid)" << std::endl;
    std::cout << "Initial Particle State: pos=(" << p.x << ", " << p.y 
              << ") vel=(" << p.vx << ", " << p.vy << ")" << std::endl;

    float dt = 1.0f;
    float gravity = 9.81f;
    float inertia = 0.5f;

    for (int step = 1; step <= 5; ++step) {
        // Calculate gradient using finite differences of local neighbors
        int ix = std::clamp((int)std::round(p.x), 0, 9);
        int iy = std::clamp((int)std::round(p.y), 0, 9);
        
        int left = std::max(0, ix - 1);
        int right = std::min(9, ix + 1);
        int up = std::max(0, iy - 1);
        int down = std::min(9, iy + 1);
        
        float gx = (grid[iy][right] - grid[iy][left]) * 0.5f;
        float gy = (grid[down][ix] - grid[up][ix]) * 0.5f;

        // Update velocity with inertia and gravity force along the gradient
        p.vx = p.vx * inertia - gravity * gx * dt;
        p.vy = p.vy * inertia - gravity * gy * dt;

        // Update position
        p.x += p.vx * dt;
        p.y += p.vy * dt;

        std::cout << "Step " << step << ": pos=(" << p.x << ", " << p.y 
                  << ") vel=(" << p.vx << ", " << p.vy << ")" << std::endl;
    }

    if (p.x == 5.0f && p.y == 5.0f && p.vx == 0.0f && p.vy == 0.0f) {
        std::cout << "SUCCESS: Particle stayed perfectly at rest!" << std::endl;
    } else {
        std::cout << "FAILURE: Particle moved!" << std::endl;
    }
    return 0;
}
