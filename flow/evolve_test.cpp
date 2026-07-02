#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>

struct GridState {
    int step;
    float H_soil[10][10];
    float h_surface[10][10];
    float h_subsurface[10][10];
    float grid_vx[10][10]; // Center-interpolated surface velocities
    float grid_vy[10][10];
};

int main() {
    const int GRID_SIZE = 10;
    
    // Hydrodynamic Grid States
    float H_soil[GRID_SIZE][GRID_SIZE] = {0.0f};       // Soil elevation (centers)
    float h_surface[GRID_SIZE][GRID_SIZE] = {0.0f};    // Surface water depth (centers)
    float h_subsurface[GRID_SIZE][GRID_SIZE] = {0.0f}; // Groundwater head (centers)
    
    // Surface velocities at staggered cell faces (MAC grid)
    float vx[GRID_SIZE + 1][GRID_SIZE] = {0.0f};
    float vy[GRID_SIZE][GRID_SIZE + 1] = {0.0f};

    // Initialize Slope Terrain sloping downhill from left to right
    // Initial groundwater table is 1.5m below the soil surface
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            H_soil[y][x] = 10.0f - (float)x * 0.8f;
            if (x == 5) {
                if (y == 5) {
                    H_soil[y][x] += 1.0f; // Dam notch
                } else {
                    H_soil[y][x] += 2.2f; // Dam wall
                }
            }
            h_subsurface[y][x] = H_soil[y][x] - 1.5f;
        }
    }

    std::vector<GridState> history;
    
    float dt = 0.2f;
    float g = 2.5f;             // Gravity constant
    float drag = 0.25f;         // Bed friction / resistance drag
    float K_infil = 0.5f;       // Soil infiltration conductivity rate
    float K_darcy = 0.6f;       // Subsurface Darcy conductivity (diffusion rate)
    float porosity = 0.35f;     // Soil effective porosity

    for (int step = 0; step <= 15; ++step) {
        // 1. Record current state of the fields
        GridState state;
        state.step = step;
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                state.H_soil[y][x] = H_soil[y][x];
                state.h_surface[y][x] = h_surface[y][x];
                state.h_subsurface[y][x] = h_subsurface[y][x];

                // Interpolate staggered face velocities to cell centers
                state.grid_vx[y][x] = (vx[x][y] + vx[x+1][y]) * 0.5f;
                state.grid_vy[y][x] = (vy[x][y] + vy[x][y+1]) * 0.5f;
            }
        }
        history.push_back(state);

        if (step == 15) break; // End of simulation

        // 2. Continuous high head surface boundary on the left (Inlet river source at x=0)
        h_surface[4][0] = 3.5f;
        h_surface[5][0] = 3.5f;

        // 3. Update Staggered Face Velocities for Surface Flow
        float next_vx[GRID_SIZE + 1][GRID_SIZE] = {0.0f};
        float next_vy[GRID_SIZE][GRID_SIZE + 1] = {0.0f};

        // Horizontal velocity vx accelerated by height difference
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 1; x < GRID_SIZE; ++x) {
                float z_L = H_soil[y][x-1] + h_surface[y][x-1];
                float z_R = H_soil[y][x] + h_surface[y][x];
                float accel_x = -g * (z_R - z_L);
                next_vx[x][y] = (vx[x][y] + accel_x * dt) * (1.0f - drag * dt);
            }
        }

        // Vertical velocity vy accelerated by height difference
        for (int y = 1; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float z_T = H_soil[y-1][x] + h_surface[y-1][x];
                float z_B = H_soil[y][x] + h_surface[y][x];
                float accel_y = -g * (z_B - z_T);
                next_vy[x][y] = (vy[x][y] + accel_y * dt) * (1.0f - drag * dt);
            }
        }

        // Copy velocities back (boundaries are solid)
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x <= GRID_SIZE; ++x) vx[x][y] = next_vx[x][y];
        }
        for (int y = 0; y <= GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) vy[x][y] = next_vy[x][y];
        }

        // 4. Update Surface Water Depth (Advection / Conservation of Mass)
        float next_h[GRID_SIZE][GRID_SIZE] = {0.0f};
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float flux_in_L = vx[x][y];
                float flux_out_R = vx[x+1][y];
                float flux_in_T = vy[x][y];
                float flux_out_B = vy[x][y+1];

                float net_outflow = (flux_out_R - flux_in_L) + (flux_out_B - flux_in_T);
                next_h[y][x] = h_surface[y][x] - net_outflow * dt;
                if (next_h[y][x] < 0.0f) next_h[y][x] = 0.0f;
            }
        }
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (x == 0 && (y == 4 || y == 5)) {
                    h_surface[y][x] = 3.5f; // Constant head
                } else {
                    h_surface[y][x] = next_h[y][x];
                }
            }
        }

        // 5. Soil Infiltration (Surface water sinks to raise Subsurface Groundwater Head)
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (h_surface[y][x] > 0.001f) {
                    // Infiltration rate is limited by surface water availability and conductivity
                    float infil = std::min(h_surface[y][x], K_infil * dt);
                    h_surface[y][x] -= infil;
                    h_subsurface[y][x] += (infil / porosity); // Rise in water table depends on porosity
                }
            }
        }

        // 6. Subsurface Darcy Flow (Groundwater Head Diffusion)
        // dh_sub/dt = K * laplacian(h_sub)
        float next_h_sub[GRID_SIZE][GRID_SIZE] = {0.0f};
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float sum_head = 0.0f;
                float weight_sum = 0.0f;

                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                            sum_head += h_subsurface[ny][nx];
                            weight_sum += 1.0f;
                        }
                    }
                }

                float avg_head = (weight_sum > 0.0f) ? (sum_head / weight_sum) : h_subsurface[y][x];
                // Darcy diffusion equation
                next_h_sub[y][x] = h_subsurface[y][x] + K_darcy * dt * (avg_head - h_subsurface[y][x]);
            }
        }
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                h_subsurface[y][x] = next_h_sub[y][x];
            }
        }

        // 7. Groundwater Exfiltration (Seepage Face Spring Formation)
        // If groundwater table rises above the soil surface elevation, it exfiltrates
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (h_subsurface[y][x] > H_soil[y][x]) {
                    float excess = h_subsurface[y][x] - H_soil[y][x];
                    
                    // Groundwater exfiltrates to surface water
                    h_subsurface[y][x] = H_soil[y][x]; // Head is capped at ground surface
                    h_surface[y][x] += (excess * porosity); // Water mass enters the surface layer
                }
            }
        }
    }

    // REGRESSION ASSERTS: Verify coupled surface-subsurface hydrology solver
    // Assert 1: Ground water is initialized below the surface
    assert(history[0].h_subsurface[5][5] < history[0].H_soil[5][5]);
    // Assert 2: Water infiltrates at the inlet, raising the subsurface head at (5,0)
    assert(history[1].h_subsurface[5][0] > history[0].h_subsurface[5][0]);
    // Assert 3: Subsurface seepage propagates under the dam and creates a spring downstream
    // That is, at step 15, exfiltration has occurred and h_surface at (5, 6) is non-zero
    // (This proves that water entered on the left, infiltrated, seeped through the soil under the dam,
    // and bubbled up as surface water at the lower ground downstream!)
    assert(history[15].h_subsurface[5][6] == history[15].H_soil[5][6]); // Saturated to surface
    assert(history[15].h_surface[5][6] > 0.0f); // Spring has formed!

    // Write path to test_progress.js as an ES6 module
    std::ofstream out("test_progress.js");
    out << "export const testData = {\n";
    out << "  grid_size: 10,\n";
    out << "  steps: [\n";
    for (size_t s = 0; s < history.size(); ++s) {
        out << "    {\n";
        out << "      step: " << history[s].step << ",\n";
        
        out << "      grid_vx: [\n";
        for (int y = 0; y < 10; ++y) {
            out << "        [";
            for (int x = 0; x < 10; ++x) {
                out << history[s].grid_vx[y][x];
                if (x < 9) out << ", ";
            }
            out << "]";
            if (y < 9) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        
        out << "      grid_vy: [\n";
        for (int y = 0; y < 10; ++y) {
            out << "        [";
            for (int x = 0; x < 10; ++x) {
                out << history[s].grid_vy[y][x];
                if (x < 9) out << ", ";
            }
            out << "]";
            if (y < 9) out << ",";
            out << "\n";
        }
        out << "      ],\n";

        out << "      grid_P: [\n"; // Soil elevation H_soil
        for (int y = 0; y < 10; ++y) {
            out << "        [";
            for (int x = 0; x < 10; ++x) {
                out << history[s].H_soil[y][x];
                if (x < 9) out << ", ";
            }
            out << "]";
            if (y < 9) out << ",";
            out << "\n";
        }
        out << "      ],\n";

        out << "      grid_h: [\n"; // Surface water depth
        for (int y = 0; y < 10; ++y) {
            out << "        [";
            for (int x = 0; x < 10; ++x) {
                out << history[s].h_surface[y][x];
                if (x < 9) out << ", ";
            }
            out << "]";
            if (y < 9) out << ",";
            out << "\n";
        }
        out << "      ],\n";

        out << "      grid_sediment: [\n"; // Subsurface groundwater head
        for (int y = 0; y < 10; ++y) {
            out << "        [";
            for (int x = 0; x < 10; ++x) {
                out << history[s].h_subsurface[y][x];
                if (x < 9) out << ", ";
            }
            out << "]";
            if (y < 9) out << ",";
            out << "\n";
        }
        out << "      ]\n";
        
        out << "    }";
        if (s < history.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "};\n";
    out.close();

    std::cout << "SUCCESS: Generated coupled Surface-Subsurface test_progress.js ES6 module" << std::endl;
    return 0;
}
