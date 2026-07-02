#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>

struct GridState {
    int step;
    float H_soil[10][10];
    float h[10][10];
    float sediment[10][10];
    float grid_vx[10][10];
    float grid_vy[10][10];
};

int main() {
    const int GRID_SIZE = 10;
    
    float H_soil[GRID_SIZE][GRID_SIZE] = {0.0f};
    float h[GRID_SIZE][GRID_SIZE] = {0.0f};
    float sediment[GRID_SIZE][GRID_SIZE] = {0.0f};
    
    float vx[GRID_SIZE + 1][GRID_SIZE] = {0.0f};
    float vy[GRID_SIZE][GRID_SIZE + 1] = {0.0f};

    // Initialize Slope Terrain sloping downhill with a central dam ridge and center notch
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            H_soil[y][x] = 10.0f - (float)x * 0.8f;
            if (x == 5) {
                if (y == 5) {
                    H_soil[y][x] += 1.0f; // Lower notch
                } else {
                    H_soil[y][x] += 2.2f; // Dam wall
                }
            }
        }
    }

    std::vector<GridState> history;
    
    float dt = 0.2f;
    float g = 2.5f;
    float drag = 0.25f;
    float M_erosion = 0.45f;
    float tau_c = 0.15f;
    float C_friction = 0.15f;
    float settle_rate = 0.12f;

    for (int step = 0; step <= 15; ++step) {
        GridState state;
        state.step = step;
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                state.H_soil[y][x] = H_soil[y][x];
                state.h[y][x] = h[y][x];
                state.sediment[y][x] = sediment[y][x];
                state.grid_vx[y][x] = (vx[x][y] + vx[x+1][y]) * 0.5f;
                state.grid_vy[y][x] = (vy[x][y] + vy[x][y+1]) * 0.5f;
            }
        }
        history.push_back(state);

        if (step == 15) break;

        // Continuous water head inlet
        h[4][0] = 3.5f;
        h[5][0] = 3.5f;

        float next_vx[GRID_SIZE + 1][GRID_SIZE] = {0.0f};
        float next_vy[GRID_SIZE][GRID_SIZE + 1] = {0.0f};

        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 1; x < GRID_SIZE; ++x) {
                float z_L = H_soil[y][x-1] + h[y][x-1];
                float z_R = H_soil[y][x] + h[y][x];
                float accel_x = -g * (z_R - z_L);
                next_vx[x][y] = (vx[x][y] + accel_x * dt) * (1.0f - drag * dt);
            }
        }

        for (int y = 1; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float z_T = H_soil[y-1][x] + h[y-1][x];
                float z_B = H_soil[y][x] + h[y][x];
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

        float next_h[GRID_SIZE][GRID_SIZE] = {0.0f};
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float net_outflow = (vx[x+1][y] - vx[x][y]) + (vy[x][y+1] - vy[x][y]);
                next_h[y][x] = h[y][x] - net_outflow * dt;
                if (next_h[y][x] < 0.0f) next_h[y][x] = 0.0f;
            }
        }
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (x == 0 && (y == 4 || y == 5)) {
                    h[y][x] = 3.5f;
                } else {
                    h[y][x] = next_h[y][x];
                }
            }
        }

        // Bed Erosion
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (h[y][x] > 0.01f) {
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

    assert(history[0].H_soil[5][5] > 6.5f && history[0].H_soil[5][5] < 7.5f);
    assert(history[1].h[5][0] > 0.1f);
    assert(history[15].H_soil[5][5] < history[0].H_soil[5][5]);

    std::ofstream out("test_erosion.js");
    out << "export const erosionData = {\n";
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
        out << "      grid_h: [\n";
        for (int y = 0; y < 10; ++y) {
            out << "        [";
            for (int x = 0; x < 10; ++x) {
                out << history[s].h[y][x];
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

    std::cout << "SUCCESS: Erosion test completed successfully." << std::endl;
    return 0;
}
