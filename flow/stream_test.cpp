#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cassert>
#include "flow_solver.h"
#include "stream_exporter.h"

int main(int argc, char* argv[]) {
    bool run_regression_test = true;
    int total_sim_steps = 1000;
    int export_stride = 10;
    bool export_js = false;

    if (argc > 1 && std::string(argv[1]) == "--export") {
        run_regression_test = false;
        total_sim_steps = 20000;
        export_stride = 100;
        export_js = true;
    }

    const int GRID_SIZE = 80;
    
    // Initialize Unified Grid & Elements
    Grid g(GRID_SIZE);
    g.initialize_soil_perlin();

    Precipitation rain(14.2f);
    Hydrodynamics hydro(9.81f, 0.15f); // gravity, drag
    Erosion erosion(0.02f, 0.04f, 0.18f, 0.15f, 0.0f); // M, tau_c, C_f, settle, infiltration (0.0)
    Landslide landslide(1.00f); // 45 deg repose
    Sink sink(74, 49);

    struct Particle {
        int id;
        float x, y;
        bool active;
        int stuck_frames;
    };
    std::vector<Particle> particles;
    int next_particle_id = 0;

    double initial_soil_volume = 0.0;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            initial_soil_volume += g.H_soil[y][x] + g.sediment[y][x];
        }
    }
    double total_rain_volume = 0.0;
    float dt = 0.04f;

    std::vector<GridState> history;

    for (int step = 0; step <= total_sim_steps; ++step) {
        // isnan checks to locate numerical instability
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (std::isnan(g.H_soil[y][x]) || std::isnan(g.h_surface[y][x]) || std::isnan(g.sediment[y][x])) {
                    std::cerr << "CRITICAL: NaN detected at step " << step << " at cell (" << x << "," << y << ")" << std::endl;
                    std::cerr << "  Current: H_soil=" << g.H_soil[y][x] << ", h_surface=" << g.h_surface[y][x] << ", sediment=" << g.sediment[y][x] << std::endl;
                    if (y > 0) std::cerr << "  y-1: H_soil=" << g.H_soil[y-1][x] << ", h_surface=" << g.h_surface[y-1][x] << std::endl;
                    if (y < GRID_SIZE - 1) std::cerr << "  y+1: H_soil=" << g.H_soil[y+1][x] << ", h_surface=" << g.h_surface[y+1][x] << std::endl;
                    if (x > 0) std::cerr << "  x-1: H_soil=" << g.H_soil[y][x-1] << ", h_surface=" << g.h_surface[y][x-1] << std::endl;
                    if (x < GRID_SIZE - 1) std::cerr << "  x+1: H_soil=" << g.H_soil[y][x+1] << ", h_surface=" << g.h_surface[y][x+1] << std::endl;
                    return 1;
                }
            }
        }
        if (step % export_stride == 0) {
            GridState state;
            state.step = step;
            state.H_soil = g.H_soil;
            state.h_surface = g.h_surface;
            state.h_soil_water = g.h_soil_water;
            
            state.grid_vx.assign(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
            state.grid_vy.assign(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
            for (int y = 0; y < GRID_SIZE; ++y) {
                for (int x = 0; x < GRID_SIZE; ++x) {
                    state.grid_vx[y][x] = (g.vx[x][y] + g.vx[x+1][y]) * 0.5f;
                    state.grid_vy[y][x] = (g.vy[x][y] + g.vy[x][y+1]) * 0.5f;
                }
            }

            state.particles.clear();
            for (const auto& p : particles) {
                if (p.active) {
                    state.particles.push_back({p.id, p.x, p.y});
                }
            }
            history.push_back(state);
        }

        if (step == total_sim_steps) break;

        // Run elements in sequence
        rain.step(g, dt, step, total_sim_steps);
        total_rain_volume += 14.2f * dt;

        landslide.step(g, dt, step, total_sim_steps);
        sink.step(g, dt, step, total_sim_steps);
        hydro.step(g, dt, step, total_sim_steps);
        erosion.step(g, dt, step, total_sim_steps);

        // Spawn and advect tracer particles (visuals only)
        if (step % 1 == 0) {
            int active_count = 0;
            for (const auto& p : particles) {
                if (p.active) active_count++;
            }
            if (active_count < 100) {
                int to_spawn = std::min(5, 100 - active_count);
                for (int k = 0; k < to_spawn; ++k) {
                    Particle p;
                    p.id = next_particle_id++;
                    p.x = ((float)std::rand() / RAND_MAX) * (float)GRID_SIZE;
                    p.y = ((float)std::rand() / RAND_MAX) * (float)GRID_SIZE;
                    p.active = true;
                    p.stuck_frames = 0;
                    particles.push_back(p);
                }
            }
        }

        // Advect tracer particles
        std::vector<std::vector<float>> cur_grid_vx(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
        std::vector<std::vector<float>> cur_grid_vy(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                cur_grid_vx[y][x] = (g.vx[x][y] + g.vx[x+1][y]) * 0.5f;
                cur_grid_vy[y][x] = (g.vy[x][y] + g.vy[x][y+1]) * 0.5f;
            }
        }

        for (auto& p : particles) {
            if (!p.active) continue;

            float x_clamp = std::max(0.0f, std::min((float)GRID_SIZE - 1.01f, p.x));
            float y_clamp = std::max(0.0f, std::min((float)GRID_SIZE - 1.01f, p.y));
            int x0 = std::floor(x_clamp);
            int y0 = std::floor(y_clamp);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            float tx = x_clamp - x0;
            float ty = y_clamp - y0;

            float p_vx = (1.0f - tx) * (1.0f - ty) * cur_grid_vx[y0][x0] +
                         tx * (1.0f - ty) * cur_grid_vx[y0][x1] +
                         (1.0f - tx) * ty * cur_grid_vx[y1][x0] +
                         tx * ty * cur_grid_vx[y1][x1];

            float p_vy = (1.0f - tx) * (1.0f - ty) * cur_grid_vy[y0][x0] +
                         tx * (1.0f - ty) * cur_grid_vy[y0][x1] +
                         (1.0f - tx) * ty * cur_grid_vy[y1][x0] +
                         tx * ty * cur_grid_vy[y1][x1];

            p.x += p_vx * dt;
            p.y += p_vy * dt;

            bool out_of_bounds = p.x < 1.0f || p.x > (float)GRID_SIZE - 1.0f || p.y < 1.0f || p.y > (float)GRID_SIZE - 1.0f;
            bool is_dry = false;
            int cx = (int)p.x;
            int cy = (int)p.y;
            if (!out_of_bounds) {
                if (g.h_surface[cy][cx] <= 0.01f) {
                    is_dry = true;
                }
            }

            float speed = std::sqrt(p_vx * p_vx + p_vy * p_vy);
            if (is_dry || speed < 0.002f) {
                p.stuck_frames++;
            } else {
                p.stuck_frames = 0;
            }

            if (out_of_bounds || p.stuck_frames > 80 || (cx == 74 && cy == 49)) {
                p.active = false;
            }
        }

        std::vector<Particle> active_particles;
        for (const auto& p : particles) {
            if (p.active) active_particles.push_back(p);
        }
        particles = active_particles;
    }

    assert(history.size() == (total_sim_steps / export_stride + 1));

    if (export_js) {
        export_to_javascript("test_stream.js", history, GRID_SIZE);
    }

    save_bmp("final_state.bmp", g.H_soil, g.h_surface, history[0].H_soil);

    double final_soil_volume = 0.0;
    double final_water_volume = 0.0;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            final_soil_volume += g.H_soil[y][x] + g.sediment[y][x];
            final_water_volume += g.h_surface[y][x] + g.h_soil_water[y][x];
        }
    }
    double soil_discrepancy = (final_soil_volume + sink.total_sediment_removed) - initial_soil_volume;
    double water_discrepancy = (final_water_volume + sink.total_water_removed) - total_rain_volume;

    std::cout << "SUCCESS: Stream test completed successfully." << std::endl;
    std::cout << "Sink Telemetry: Total Water Removed: " << sink.total_water_removed << " m^3 | Total Sediment Removed: " << sink.total_sediment_removed << " m^3" << std::endl;
    std::cout << "Last Cycle Telemetry: Water Removed: " << sink.last_cycle_water_removed << " m^3 | Sediment Removed: " << sink.last_cycle_sediment_removed << " m^3" << std::endl;
    std::cout << "Mass Balance Diagnostics:" << std::endl;
    std::cout << "  Initial Soil Volume: " << initial_soil_volume << " m^3 | Final + Removed Soil: " << (final_soil_volume + sink.total_sediment_removed) << " m^3 | Soil Discrepancy: " << soil_discrepancy << " m^3" << std::endl;
    std::cout << "  Total Rain Added: " << total_rain_volume << " m^3 | Final + Removed Water: " << (final_water_volume + sink.total_water_removed) << " m^3 | Water Discrepancy: " << water_discrepancy << " m^3" << std::endl;

    if (run_regression_test) {
        std::cout << "\nRunning Regression Test Assertions..." << std::endl;
        
        if (std::abs(soil_discrepancy) > 0.05) {
            std::cerr << "FAIL: Soil discrepancy too large: " << soil_discrepancy << " m^3" << std::endl;
            return 1;
        }
        if (std::abs(water_discrepancy) > 0.05) {
            std::cerr << "FAIL: Water discrepancy too large: " << water_discrepancy << " m^3" << std::endl;
            return 1;
        }
        std::cout << "  [PASS] Mass Conservation verified." << std::endl;

        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (g.h_surface[y][x] < 0.0f) {
                    std::cerr << "FAIL: Negative water depth at (" << x << "," << y << "): " << g.h_surface[y][x] << std::endl;
                    return 1;
                }
                if (g.sediment[y][x] < 0.0f) {
                    std::cerr << "FAIL: Negative suspended sediment at (" << x << "," << y << "): " << g.sediment[y][x] << std::endl;
                    return 1;
                }
                if (g.H_soil[y][x] < -10.01f) {
                    std::cerr << "FAIL: Soil below bedrock limit at (" << x << "," << y << "): " << g.H_soil[y][x] << std::endl;
                    return 1;
                }
                if (g.H_soil[y][x] > 4.79f) {
                    std::cerr << "FAIL: Peak elevation exceeded limit at (" << x << "," << y << "): " << g.H_soil[y][x] << std::endl;
                    return 1;
                }
            }
        }
        std::cout << "  [PASS] Non-negativity and physical bounds verified." << std::endl;

        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x <= GRID_SIZE; ++x) {
                if (std::abs(g.vx[x][y]) > 15.0f) {
                    std::cerr << "FAIL: Velocity vx out of bounds at (" << x << "," << y << "): " << g.vx[x][y] << std::endl;
                    return 1;
                }
            }
        }
        for (int y = 0; y <= GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (std::abs(g.vy[x][y]) > 15.0f) {
                    std::cerr << "FAIL: Velocity vy out of bounds at (" << x << "," << y << "): " << g.vy[x][y] << std::endl;
                    return 1;
                }
            }
        }
        std::cout << "  [PASS] Velocity bounds verified." << std::endl;
        std::cout << "REGRESSION TEST PASSED SUCCESSFULLY!" << std::endl;
    }
    return 0;
}
