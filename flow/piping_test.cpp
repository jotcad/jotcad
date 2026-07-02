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
    float sediment[10][10];
    float grid_vx[10][10];
    float grid_vy[10][10];
};

int main() {
    const int GRID_SIZE = 10;
    
    // Fully Coupled Hydrodynamic and Soil State
    float H_soil[GRID_SIZE][GRID_SIZE] = {0.0f};       // Soil elevation (centers)
    float h_surface[GRID_SIZE][GRID_SIZE] = {0.0f};    // Surface water depth (centers)
    float h_subsurface[GRID_SIZE][GRID_SIZE] = {0.0f}; // Groundwater table head (centers)
    float sediment[GRID_SIZE][GRID_SIZE] = {0.0f};     // Suspended sediment (centers)
    
    // Surface velocities at staggered cell faces (MAC grid)
    float vx[GRID_SIZE + 1][GRID_SIZE] = {0.0f};
    float vy[GRID_SIZE][GRID_SIZE + 1] = {0.0f};

    // Initialize Slope Terrain: sloping downhill from left to right with a dam ridge
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            H_soil[y][x] = 10.0f - (float)x * 0.8f;
            if (x == 5) {
                if (y == 5) {
                    H_soil[y][x] += 1.0f; // Notch
                } else {
                    H_soil[y][x] += 2.2f; // Dam wall
                }
            }
            h_subsurface[y][x] = H_soil[y][x] - 1.5f; // Water table 1.5m below surface
        }
    }

    std::vector<GridState> history;
    
    float dt = 0.2f;
    float g = 2.5f;             // Gravity constant
    float drag = 0.25f;         // Bed friction / resistance drag
    float K_infil = 0.5f;       // Soil infiltration conductivity rate
    float K_darcy = 0.6f;       // Subsurface Darcy conductivity (diffusion rate)
    float porosity = 0.35f;     // Soil effective porosity
    
    // Erosion parameters
    float M_erosion = 0.40f;    // Soil detachment rate coefficient
    float tau_c = 0.15f;        // Critical shear stress threshold
    float C_friction = 0.15f;   // Bed drag coefficient for shear stress
    float settle_rate = 0.12f;   // Rate of sediment settling

    for (int step = 0; step <= 15; ++step) {
        // Record current state
        GridState state;
        state.step = step;
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                state.H_soil[y][x] = H_soil[y][x];
                state.h_surface[y][x] = h_surface[y][x];
                state.h_subsurface[y][x] = h_subsurface[y][x];
                state.sediment[y][x] = sediment[y][x];
                state.grid_vx[y][x] = (vx[x][y] + vx[x+1][y]) * 0.5f;
                state.grid_vy[y][x] = (vy[x][y] + vy[x][y+1]) * 0.5f;
            }
        }
        history.push_back(state);

        if (step == 15) break;

        // Constant head boundary on the left (Inflow river)
        h_surface[4][0] = 3.5f;
        h_surface[5][0] = 3.5f;

        // Update Surface Velocities on Staggered faces
        float next_vx[GRID_SIZE + 1][GRID_SIZE] = {0.0f};
        float next_vy[GRID_SIZE][GRID_SIZE + 1] = {0.0f};

        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 1; x < GRID_SIZE; ++x) {
                float z_L = H_soil[y][x-1] + h_surface[y][x-1];
                float z_R = H_soil[y][x] + h_surface[y][x];
                float accel_x = -g * (z_R - z_L);
                next_vx[x][y] = (vx[x][y] + accel_x * dt) * (1.0f - drag * dt);
            }
        }

        for (int y = 1; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float z_T = H_soil[y-1][x] + h_surface[y-1][x];
                float z_B = H_soil[y][x] + h_surface[y][x];
                float accel_y = -g * (z_B - z_T);
                next_vy[x][y] = (vy[x][y] + accel_y * dt) * (1.0f - drag * dt);
            }
        }

        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x <= GRID_SIZE; ++x) vx[x][y] = next_vx[x][y];
        }
        for (int y = 0; y <= GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) vy[x][y] = next_vy[x][y];
        }

        // Update Surface Water Depth
        float next_h[GRID_SIZE][GRID_SIZE] = {0.0f};
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float net_outflow = (vx[x+1][y] - vx[x][y]) + (vy[x][y+1] - vy[x][y]);
                next_h[y][x] = h_surface[y][x] - net_outflow * dt;
                if (next_h[y][x] < 0.0f) next_h[y][x] = 0.0f;
            }
        }
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (x == 0 && (y == 4 || y == 5)) {
                    h_surface[y][x] = 3.5f;
                } else {
                    h_surface[y][x] = next_h[y][x];
                }
            }
        }

        // Soil Infiltration
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (h_surface[y][x] > 0.001f) {
                    float infil = std::min(h_surface[y][x], K_infil * dt);
                    h_surface[y][x] -= infil;
                    h_subsurface[y][x] += (infil / porosity);
                }
            }
        }

        // Subsurface Darcy Seepage Diffusion
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
                next_h_sub[y][x] = h_subsurface[y][x] + K_darcy * dt * (avg_head - h_subsurface[y][x]);
            }
        }
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                h_subsurface[y][x] = next_h_sub[y][x];
            }
        }

        // Groundwater Exfiltration (Seepage face)
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (h_subsurface[y][x] > H_soil[y][x]) {
                    float excess = h_subsurface[y][x] - H_soil[y][x];
                    h_subsurface[y][x] = H_soil[y][x];
                    h_surface[y][x] += (excess * porosity);
                }
            }
        }

        // Coupled Bed Erosion
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (h_surface[y][x] > 0.01f) {
                    float vx_avg = (vx[x][y] + vx[x+1][y]) * 0.5f;
                    float vy_avg = (vy[x][y] + vy[x][y+1]) * 0.5f;
                    float u_mag = std::sqrt(vx_avg * vx_avg + vy_avg * vy_avg);
                    
                    float tau = C_friction * u_mag * u_mag;
                    if (tau > tau_c) {
                        float E = M_erosion * (tau - tau_c) * dt;
                        E = std::min(E, H_soil[y][x]);
                        H_soil[y][x] -= E;
                        sediment[y][x] += E;
                    } else {
                        float D = settle_rate * sediment[y][x] * dt;
                        sediment[y][x] -= D;
                        H_soil[y][x] += D;
                    }
                }
            }
        }

        // Sediment Advection
        float next_sediment[GRID_SIZE][GRID_SIZE] = {0.0f};
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) next_sediment[y][x] = sediment[y][x];
        }
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float vx_avg = (vx[x][y] + vx[x+1][y]) * 0.5f;
                float vy_avg = (vy[x][y] + vy[x][y+1]) * 0.5f;
                int nx = x + (vx_avg > 0.05f ? 1 : (vx_avg < -0.05f ? -1 : 0));
                int ny = y + (vy_avg > 0.05f ? 1 : (vy_avg < -0.05f ? -1 : 0));
                if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE && (nx != x || ny != y)) {
                    float amount = sediment[y][x] * 0.4f * dt;
                    next_sediment[y][x] -= amount;
                    next_sediment[ny][nx] += amount;
                }
            }
        }
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) sediment[y][x] = std::max(0.0f, next_sediment[y][x]);
        }
    }

    // REGRESSION ASSERTS
    assert(history[0].H_soil[5][5] > 6.5f && history[0].H_soil[5][5] < 7.5f);
    assert(history[5].h_subsurface[5][4] > history[0].h_subsurface[5][4]);
    assert(history[15].H_soil[5][6] < history[0].H_soil[5][6]); // piping erosion has occurred!

    std::ofstream out("test_piping.js");
    out << "export const pipingData = {\n";
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
        out << "      grid_H_soil: [\n";
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
        out << "      grid_h_surface: [\n";
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
        out << "      grid_h_subsurface: [\n";
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
        out << "      ],\n";
        out << "      grid_sediment: [\n";
        for (int y = 0; y < 10; ++y) {
            out << "        [";
            for (int x = 0; x < 10; ++x) {
                out << history[s].sediment[y][x];
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

    std::cout << "SUCCESS: Piping test completed successfully." << std::endl;
    return 0;
}
