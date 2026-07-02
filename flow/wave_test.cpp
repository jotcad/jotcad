#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>

struct GridState {
    int step;
    float grid_vx[10][10];
    float grid_vy[10][10];
    float grid_P[10][10];
};

int main() {
    const int GRID_SIZE = 10;
    float cell_vx[GRID_SIZE][GRID_SIZE] = {0.0f};
    float cell_vy[GRID_SIZE][GRID_SIZE] = {0.0f};
    float P[GRID_SIZE][GRID_SIZE] = {0.0f}; // Pressure field

    // Set INITIAL velocity pulse for central cell (5,5) pointing toward the right
    cell_vx[5][5] = 5.0f;
    cell_vy[5][5] = 0.0f;

    // REGRESSION CHECK 1: Verify initial conditions
    assert(cell_vx[5][5] == 5.0f);
    assert(cell_vy[5][5] == 0.0f);
    assert(P[5][5] == 0.0f);

    std::vector<GridState> history;
    
    float dt = 0.25f;          // Stable timestep for coupled wave equations
    float c_sq = 2.0f;        // Speed of sound squared
    float viscosity = 0.15f;   // Numerical smoothing to prevent checkerboard oscillations

    for (int step = 0; step <= 10; ++step) {
        GridState state;
        state.step = step;
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                state.grid_vx[y][x] = cell_vx[y][x];
                state.grid_vy[y][x] = cell_vy[y][x];
                state.grid_P[y][x] = P[y][x];
            }
        }
        history.push_back(state);

        if (step == 10) break;

        // Compute Divergence and update Pressure
        float next_P[GRID_SIZE][GRID_SIZE] = {0.0f};
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                int left = std::max(0, x - 1);
                int right = std::min(9, x + 1);
                int up = std::max(0, y - 1);
                int down = std::min(9, y + 1);

                float div = (cell_vx[y][right] - cell_vx[y][left]) * 0.5f +
                            (cell_vy[down][x] - cell_vy[up][x]) * 0.5f;

                next_P[y][x] = P[y][x] - c_sq * div * dt;
            }
        }

        // Apply pressure diffusion
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float sum_P = 0.0f;
                float weight_sum = 0.0f;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                            float w = (dx == 0 && dy == 0) ? 0.0f : 1.0f;
                            sum_P += next_P[ny][nx] * w;
                            weight_sum += w;
                        }
                    }
                }
                float avg_P = (weight_sum > 0.0f) ? (sum_P / weight_sum) : 0.0f;
                P[y][x] = next_P[y][x] * (1.0f - viscosity) + avg_P * viscosity;
            }
        }

        if (step == 0) {
            assert(P[5][4] < -0.1f);
            assert(P[5][6] > 0.1f);
        }

        // Compute Pressure Gradient and update Velocity
        float next_vx[GRID_SIZE][GRID_SIZE] = {0.0f};
        float next_vy[GRID_SIZE][GRID_SIZE] = {0.0f};
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                int left = std::max(0, x - 1);
                int right = std::min(9, x + 1);
                int up = std::max(0, y - 1);
                int down = std::min(9, y + 1);

                float grad_x = (P[y][right] - P[y][left]) * 0.5f;
                float grad_y = (P[down][x] - P[up][x]) * 0.5f;

                next_vx[y][x] = cell_vx[y][x] - grad_x * dt;
                next_vy[y][x] = cell_vy[y][x] - grad_y * dt;
            }
        }

        // Apply velocity diffusion
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float sum_vx = 0.0f;
                float sum_vy = 0.0f;
                float weight_sum = 0.0f;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                            float w = (dx == 0 && dy == 0) ? 0.0f : 1.0f;
                            sum_vx += next_vx[ny][nx] * w;
                            sum_vy += next_vy[ny][nx] * w;
                            weight_sum += w;
                        }
                    }
                }
                float avg_vx = (weight_sum > 0.0f) ? (sum_vx / weight_sum) : 0.0f;
                float avg_vy = (weight_sum > 0.0f) ? (sum_vy / weight_sum) : 0.0f;

                cell_vx[y][x] = next_vx[y][x] * (1.0f - viscosity) + avg_vx * viscosity;
                cell_vy[y][x] = next_vy[y][x] * (1.0f - viscosity) + avg_vy * viscosity;
            }
        }

        cell_vx[5][5] = 5.0f;
        cell_vy[5][5] = 0.0f;
    }

    assert(history[10].grid_vx[5][6] > 0.1f);
    assert(history[10].grid_vx[5][7] > 0.05f);
    assert(std::abs(history[10].grid_P[5][6]) > 0.05f);

    std::ofstream out("test_wave.js");
    out << "export const waveData = {\n";
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
        out << "      grid_P: [\n";
        for (int y = 0; y < 10; ++y) {
            out << "        [";
            for (int x = 0; x < 10; ++x) {
                out << history[s].grid_P[y][x];
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

    std::cout << "SUCCESS: Wave test completed successfully." << std::endl;
    return 0;
}
