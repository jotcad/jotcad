#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <cassert>

struct GridState {
    int step;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface;
    std::vector<std::vector<float>> grid_vx;
    std::vector<std::vector<float>> grid_vy;
    struct ParticleExport {
        int id;
        float x, y;
    };
    std::vector<ParticleExport> particles;
    std::vector<std::vector<float>> h_soil_water;
};

void save_bmp(const std::string& filename, const std::vector<std::vector<float>>& H_soil, const std::vector<std::vector<float>>& h_surface, const std::vector<std::vector<float>>& H_initial) {
    int w = H_soil[0].size();
    int h = H_soil.size();
    int row_size = w * 3;
    int padding = (4 - (row_size % 4)) % 4;
    int filesize = 54 + (row_size + padding) * h;
    
    unsigned char bmpFileHeader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpInfoHeader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
    
    bmpFileHeader[ 2] = (unsigned char)(filesize      );
    bmpFileHeader[ 3] = (unsigned char)(filesize >>  8);
    bmpFileHeader[ 4] = (unsigned char)(filesize >> 16);
    bmpFileHeader[ 5] = (unsigned char)(filesize >> 24);
    
    bmpInfoHeader[ 4] = (unsigned char)(w      );
    bmpInfoHeader[ 5] = (unsigned char)(w >>  8);
    bmpInfoHeader[ 6] = (unsigned char)(w >> 16);
    bmpInfoHeader[ 7] = (unsigned char)(w >> 24);
    bmpInfoHeader[ 8] = (unsigned char)(h      );
    bmpInfoHeader[ 9] = (unsigned char)(h >>  8);
    bmpInfoHeader[10] = (unsigned char)(h >> 16);
    bmpInfoHeader[11] = (unsigned char)(h >> 24);

    std::ofstream f(filename, std::ios::out | std::ios::binary);
    f.write((char*)bmpFileHeader, 14);
    f.write((char*)bmpInfoHeader, 40);

    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            float H = H_soil[y][x];
            float H0 = H_initial[y][x];
            float water = h_surface[y][x];
            float erosion = std::max(0.0f, H0 - H);

            unsigned char r = 0, g = 0, b = 0;

            float h_L = x > 0 ? H_soil[y][x-1] : H;
            float h_R = x < w - 1 ? H_soil[y][x+1] : H;
            float h_T = y > 0 ? H_soil[y-1][x] : H;
            float h_B = y < h - 1 ? H_soil[y+1][x] : H;
            
            float dx = h_R - h_L;
            float dy = h_B - h_T;
            float shadow = 1.0f - 0.20f * dx - 0.20f * dy;
            float shade = std::max(0.55f, std::min(1.45f, shadow));

            float normH = std::max(0.0f, std::min(1.0f, (H - 23.5f) / 3.0f));
            
            float sr = (34.0f + (80.0f - 34.0f) * normH) * shade;
            float sg = (197.0f + (50.0f - 197.0f) * normH) * shade;
            float sb = (94.0f + (25.0f - 94.0f) * normH) * shade;

            if (erosion > 0.015f) {
                float t = std::min(1.0f, erosion * 6.0f);
                sr = (1.0f - t) * sr + t * 71.0f;
                sg = (1.0f - t) * sg + t * 85.0f;
                sb = (1.0f - t) * sb + t * 105.0f;
            }

            r = (unsigned char)std::max(0.0f, std::min(255.0f, sr));
            g = (unsigned char)std::max(0.0f, std::min(255.0f, sg));
            b = (unsigned char)std::max(0.0f, std::min(255.0f, sb));

            if (water > 0.01f) {
                float w_norm = std::min(1.0f, water / 0.50f);
                float wr = 56.0f + (3.0f - 56.0f) * w_norm;
                float wg = 189.0f + (105.0f - 189.0f) * w_norm;
                float wb = 248.0f + (161.0f - 248.0f) * w_norm;
                
                float w_opacity = std::min(0.90f, 0.40f + w_norm * 0.40f);
                r = (unsigned char)((1.0f - w_opacity) * r + w_opacity * wr);
                g = (unsigned char)((1.0f - w_opacity) * g + w_opacity * wg);
                b = (unsigned char)((1.0f - w_opacity) * b + w_opacity * wb);
            }

            // Mark the sink as red in BMP
            if (x == 12 && y == 12) {
                r = 239; g = 68; b = 68;
            }
            f.put(b); f.put(g); f.put(r);
        }
        for (int p = 0; p < padding; ++p) {
            f.put(0);
        }
    }
}

struct PerlinNoise2D {
    int p[512];
    PerlinNoise2D() {
        static const int permutation[256] = {
            151,160,137,91,90,15,
            131,13,201,95,96,53,194,233, 7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
            190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
            88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
            77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
            102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
            135,130,116,188,189,128,1,119,248,73,188,206,120,244,214,107,252,24,178,160,
            182,235,180,186,13,250,134,124,142,29,166,224,198,129,73,81,244,61,63,67,112,
            81,60,183,190,137,254,121,78,184,182,220,25,97,252,90,121,39,182,182,245,63,
            11,186,198,186,112,112,85,124,190,132,60,183,191,159,22,219,94,223,254,233,
            10,23,22,252,232,154,254, 52,59,228,174,178,174,20,204,188,110,60,190,184,
            186,197,186,120,116,117,144,22,120,120,150,160,143,159,34,53,60,182,182,204,
            68,171,200,172,144,144,149,150,149,150,228,174,188,110,60,184
        };
        for (int i = 0; i < 256; ++i) {
            p[i] = permutation[i];
            p[256 + i] = permutation[i];
        }
    }

    float fade(float t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    float lerp(float t, float a, float b) {
        return a + t * (b - a);
    }

    float grad(int hash, float x, float y) {
        int h = hash & 7;
        float u = h < 4 ? x : y;
        float v = h < 4 ? y : x;
        return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
    }

    float noise(float x, float y) {
        int X = (int)std::floor(x) & 255;
        int Y = (int)std::floor(y) & 255;

        x -= std::floor(x);
        y -= std::floor(y);

        float u = fade(x);
        float v = fade(y);

        int aa = p[p[X] + Y];
        int ab = p[p[X] + Y + 1];
        int ba = p[p[X + 1] + Y];
        int bb = p[p[X + 1] + Y + 1];

        float x1 = lerp(u, grad(aa, x, y), grad(ba, x - 1.0f, y));
        float x2 = lerp(u, grad(ab, x, y - 1.0f), grad(bb, x - 1.0f, y - 1.0f));

        return lerp(v, x1, x2);
    }
};

int main() {
    const int GRID_SIZE = 80; // Double the grid size again
    
    std::vector<std::vector<float>> H_soil(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
    std::vector<std::vector<float>> h_surface(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
    std::vector<std::vector<float>> sediment(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
    
    std::vector<std::vector<float>> vx(GRID_SIZE + 1, std::vector<float>(GRID_SIZE, 0.0f));
    std::vector<std::vector<float>> vy(GRID_SIZE, std::vector<float>(GRID_SIZE + 1, 0.0f));

    std::srand(42);

    // Generate terrain elevations using Perlin Noise only (no regional tilt, no funnel slope)
    PerlinNoise2D perlin;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            float pn = perlin.noise(x * 0.25f, y * 0.25f);
            H_soil[y][x] = 25.0f + pn * 1.20f;
        }
    }

    // Start with water filled to a flat, uniform surface elevation (lake initialization)
    float initial_water_z = 26.0f;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            h_surface[y][x] = std::max(0.0f, initial_water_z - H_soil[y][x]);
        }
    }

    std::vector<GridState> history;
    
    struct Particle {
        int id;
        float x, y;
        bool active;
        int stuck_frames;
    };
    std::vector<Particle> particles;
    int next_particle_id = 0;

    std::vector<std::vector<float>> h_soil_water(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
    float max_soil_capacity = 0.40f;
    float K_sub = 0.08f;

    int total_sim_steps = 6000;
    int export_stride = 30;    // Export 201 steps total to keep file size lightweight
    
    float dt = 0.04f;           
    float g = 9.81f;             
    float drag = 0.15f;         
    
    // Erosion parameters
    float M_erosion = 0.02f;    
    float tau_c = 0.04f;        
    float C_friction = 0.18f;   
    float settle_rate = 0.15f;   
    float infiltration_rate = 0.005f;   

    for (int step = 0; step <= total_sim_steps; ++step) {
        if (step % export_stride == 0) {
            GridState state;
            state.step = step;
            state.H_soil = H_soil;
            state.h_surface = h_surface;
            state.h_soil_water = h_soil_water;
            state.grid_vx.assign(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
            state.grid_vy.assign(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
            for (int y = 0; y < GRID_SIZE; ++y) {
                for (int x = 0; x < GRID_SIZE; ++x) {
                    state.grid_vx[y][x] = (vx[x][y] + vx[x+1][y]) * 0.5f;
                    state.grid_vy[y][x] = (vy[x][y] + vy[x][y+1]) * 0.5f;
                }
            }
            for (const auto& p : particles) {
                if (p.active) {
                    GridState::ParticleExport pe;
                    pe.id = p.id;
                    pe.x = p.x;
                    pe.y = p.y;
                    state.particles.push_back(pe);
                }
            }
            history.push_back(state);
        }

        if (step == total_sim_steps) break;

        // --- Uniform Precipitation (Rainfall) ---
        float rain_rate = 0.0040f;
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                h_surface[y][x] += rain_rate * dt;
            }
        }

        // Update Surface Velocities with Dry Cell Gate protection
        std::vector<std::vector<float>> next_vx(GRID_SIZE + 1, std::vector<float>(GRID_SIZE, 0.0f));
        std::vector<std::vector<float>> next_vy(GRID_SIZE, std::vector<float>(GRID_SIZE + 1, 0.0f));

        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 1; x < GRID_SIZE; ++x) {
                float h_L = h_surface[y][x-1];
                float h_R = h_surface[y][x];
                if (h_L < 0.001f && h_R < 0.001f) {
                    next_vx[x][y] = 0.0f;
                } else {
                    float z_L = H_soil[y][x-1] + h_L;
                    float z_R = H_soil[y][x] + h_R;
                    float accel_x = -g * (z_R - z_L);
                    float depth = std::max(h_L, h_R);
                    float speed = std::abs(vx[x][y]);
                    float local_drag = drag * (0.05f * (1.0f + speed)) / (depth + 0.001f);
                    float val = (vx[x][y] + accel_x * dt) / (1.0f + local_drag * dt);
                    next_vx[x][y] = std::max(-3.0f, std::min(3.0f, val));
                }
            }
        }

        for (int y = 1; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float h_T = h_surface[y-1][x];
                float h_B = h_surface[y][x];
                if (h_T < 0.001f && h_B < 0.001f) {
                    next_vy[x][y] = 0.0f;
                } else {
                    float z_T = H_soil[y-1][x] + h_T;
                    float z_B = H_soil[y][x] + h_B;
                    float accel_y = -g * (z_B - z_T);
                    float depth = std::max(h_T, h_B);
                    float speed = std::abs(vy[x][y]);
                    float local_drag = drag * (0.05f * (1.0f + speed)) / (depth + 0.001f);
                    float val = (vy[x][y] + accel_y * dt) / (1.0f + local_drag * dt);
                    next_vy[x][y] = std::max(-3.0f, std::min(3.0f, val));
                }
            }
        }

        vx = next_vx;
        vy = next_vy;

        // Apply velocity smoothing (numerical viscosity) to prevent checkerboard oscillations
        std::vector<std::vector<float>> smoothed_vx = vx;
        for (int y = 1; y < GRID_SIZE - 1; ++y) {
            for (int x = 1; x < GRID_SIZE; ++x) {
                float laplacian = vx[x-1][y] + vx[x+1][y] + vx[x][y-1] + vx[x][y+1] - 4.0f * vx[x][y];
                smoothed_vx[x][y] = vx[x][y] + 0.01f * laplacian;
            }
        }
        vx = smoothed_vx;

        std::vector<std::vector<float>> smoothed_vy = vy;
        for (int y = 1; y < GRID_SIZE; ++y) {
            for (int x = 1; x < GRID_SIZE - 1; ++x) {
                float laplacian = vy[x-1][y] + vy[x+1][y] + vy[x][y-1] + vy[x][y+1] - 4.0f * vy[x][y];
                smoothed_vy[x][y] = vy[x][y] + 0.01f * laplacian;
            }
        }
        vy = smoothed_vy;

        // Completely Closed Outer Boundaries
        for (int y = 0; y < GRID_SIZE; ++y) {
            vx[0][y] = 0.0f;
            if (vx[1][y] < 0.0f) vx[1][y] = 0.0f;
            vx[GRID_SIZE][y] = 0.0f;
            if (vx[GRID_SIZE - 1][y] > 0.0f) vx[GRID_SIZE - 1][y] = 0.0f;
        }
        for (int x = 0; x < GRID_SIZE; ++x) {
            vy[x][0] = 0.0f;
            if (vy[x][1] < 0.0f) vy[x][1] = 0.0f;
            vy[x][GRID_SIZE] = 0.0f;
            if (vy[x][GRID_SIZE - 1] > 0.0f) vy[x][GRID_SIZE - 1] = 0.0f;
        }

        // Compute Upwind Fluxes (Water transport matches cell depth, not just velocity)
        std::vector<std::vector<float>> qx(GRID_SIZE + 1, std::vector<float>(GRID_SIZE, 0.0f));
        std::vector<std::vector<float>> qy(GRID_SIZE, std::vector<float>(GRID_SIZE + 1, 0.0f));

        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 1; x < GRID_SIZE; ++x) {
                float v = vx[x][y];
                if (v >= 0.0f) {
                    qx[x][y] = v * h_surface[y][x-1];
                } else {
                    qx[x][y] = v * h_surface[y][x];
                }
            }
        }
        for (int y = 1; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float v = vy[x][y];
                if (v >= 0.0f) {
                    qy[x][y] = v * h_surface[y-1][x];
                } else {
                    qy[x][y] = v * h_surface[y][x];
                }
            }
        }

        // Apply Edge Boundaries to Fluxes
        for (int y = 0; y < GRID_SIZE; ++y) {
            if (vx[0][y] < 0.0f) qx[0][y] = vx[0][y] * h_surface[y][0];
            if (vx[GRID_SIZE][y] > 0.0f) qx[GRID_SIZE][y] = vx[GRID_SIZE][y] * h_surface[y][GRID_SIZE-1];
        }
        for (int x = 0; x < GRID_SIZE; ++x) {
            if (vy[x][0] < 0.0f) qy[x][0] = vy[x][0] * h_surface[0][x];
            if (vy[x][GRID_SIZE] > 0.0f) qy[x][GRID_SIZE] = vy[x][GRID_SIZE] * h_surface[GRID_SIZE-1][x];
        }

        // Compute Total Outflow per cell to prevent negative depth (Flux Limiting)
        std::vector<std::vector<float>> cell_outflow(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float out = 0.0f;
                if (qx[x+1][y] > 0.0f) out += qx[x+1][y];
                if (qx[x][y] < 0.0f) out -= qx[x][y];
                if (qy[x][y+1] > 0.0f) out += qy[x][y+1];
                if (qy[x][y] < 0.0f) out -= qy[x][y];
                cell_outflow[y][x] = out;
            }
        }

        // Scale Outflow Fluxes to avoid draining more than is present
        std::vector<std::vector<float>> cell_scale(GRID_SIZE, std::vector<float>(GRID_SIZE, 1.0f));
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float limit = cell_outflow[y][x] * dt;
                if (limit > h_surface[y][x] && limit > 0.0f) {
                    cell_scale[y][x] = h_surface[y][x] / limit;
                }
            }
        }

        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 1; x < GRID_SIZE; ++x) {
                if (qx[x][y] >= 0.0f) {
                    qx[x][y] *= cell_scale[y][x-1];
                } else {
                    qx[x][y] *= cell_scale[y][x];
                }
            }
        }
        for (int y = 1; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (qy[x][y] >= 0.0f) {
                    qy[x][y] *= cell_scale[y-1][x];
                } else {
                    qy[x][y] *= cell_scale[y][x];
                }
            }
        }
        for (int y = 0; y < GRID_SIZE; ++y) {
            if (qx[0][y] < 0.0f) qx[0][y] *= cell_scale[y][0];
            if (qx[GRID_SIZE][y] > 0.0f) qx[GRID_SIZE][y] *= cell_scale[y][GRID_SIZE-1];
        }
        for (int x = 0; x < GRID_SIZE; ++x) {
            if (qy[x][0] < 0.0f) qy[x][0] *= cell_scale[0][x];
            if (qy[x][GRID_SIZE] > 0.0f) qy[x][GRID_SIZE] *= cell_scale[GRID_SIZE-1][x];
        }

        // Update Surface Water Depth with Scaled Fluxes
        std::vector<std::vector<float>> next_h(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float net_outflow = (qx[x+1][y] - qx[x][y]) + (qy[x][y+1] - qy[x][y]);
                next_h[y][x] = h_surface[y][x] - net_outflow * dt;
                if (next_h[y][x] < 0.0f) next_h[y][x] = 0.0f;
            }
        }
        h_surface = next_h;

        // Laplacian smoothing of surface water to prevent numerical checkerboard sloshing
        std::vector<std::vector<float>> smoothed_h = h_surface;
        for (int y = 1; y < GRID_SIZE - 1; ++y) {
            for (int x = 1; x < GRID_SIZE - 1; ++x) {
                float laplacian = h_surface[y][x-1] + h_surface[y][x+1] + h_surface[y-1][x] + h_surface[y+1][x] - 4.0f * h_surface[y][x];
                smoothed_h[y][x] = h_surface[y][x] + 0.01f * laplacian;
            }
        }
        h_surface = smoothed_h;

        // --- Coupled Surface-Subsurface Hydrology (Infiltration, Darcy Flow, Seepage) ---
        // 1. Infiltration capped by remaining soil water capacity
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float capacity_left = std::max(0.0f, max_soil_capacity - h_soil_water[y][x]);
                float potential_infilt = infiltration_rate * dt;
                float actual_infilt = std::min(potential_infilt, h_surface[y][x]);
                actual_infilt = std::min(actual_infilt, capacity_left);
                
                h_surface[y][x] -= actual_infilt;
                h_soil_water[y][x] += actual_infilt;
            }
        }

        // 2. Darcy Subsurface Flow (gravity-driven water table diffusion)
        std::vector<std::vector<float>> z_water(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                z_water[y][x] = H_soil[y][x] + h_soil_water[y][x];
            }
        }

        std::vector<std::vector<float>> next_soil_water = h_soil_water;
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                int dx[] = {1, -1, 0, 0};
                int dy[] = {0, 0, 1, -1};
                for (int i = 0; i < 4; ++i) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];
                    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                        float head_diff = z_water[y][x] - z_water[ny][nx];
                        if (head_diff > 0.0f) {
                            float flow = K_sub * head_diff * (h_soil_water[y][x] / max_soil_capacity) * dt;
                            flow = std::min(flow, h_soil_water[y][x] * 0.23f);
                            next_soil_water[y][x] -= flow;
                            next_soil_water[ny][nx] += flow;
                        }
                    }
                }
            }
        }
        h_soil_water = next_soil_water;

        // 3. Return Flow / Seepage (Exfiltration of excess water to surface)
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (h_soil_water[y][x] > max_soil_capacity) {
                    float seepage = h_soil_water[y][x] - max_soil_capacity;
                    h_surface[y][x] += seepage;
                    h_soil_water[y][x] = max_soil_capacity;
                }
            }
        }

        // 4. Internal Drain Sink at (12, 12)
        int sink_x = 12;
        int sink_y = 12;
        h_surface[sink_y][sink_x] = 0.0f;
        h_soil_water[sink_y][sink_x] = 0.0f;
        sediment[sink_y][sink_x] = 0.0f;
        // Keep the sink's soil elevation as the lowest local point
        float min_neighbor = std::min({H_soil[sink_y-1][sink_x], H_soil[sink_y+1][sink_x], H_soil[sink_y][sink_x-1], H_soil[sink_y][sink_x+1]});
        H_soil[sink_y][sink_x] = min_neighbor - 0.01f;

        // --- Diagnostic Water Progress Tracking ---
        static bool reached_mid = false;
        if (!reached_mid) {
            for (int y = 0; y < GRID_SIZE; ++y) {
                if (h_surface[y][GRID_SIZE / 2] > 0.01f) {
                    std::cout << ">>> WATER OVERFLOWED SADDLE (REACHED X=" << (GRID_SIZE / 2) << ") AT STEP " << step << " <<<" << std::endl;
                    reached_mid = true;
                    break;
                }
            }
        }
        
        static bool reached_end = false;
        if (!reached_end) {
            for (int y = 0; y < GRID_SIZE; ++y) {
                if (h_surface[y][GRID_SIZE - 5] > 0.01f) {
                    std::cout << ">>> WATER REACHED EAST EDGE (REACHED X=" << (GRID_SIZE - 5) << ") AT STEP " << step << " <<<" << std::endl;
                    reached_end = true;
                    break;
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
        std::vector<std::vector<float>> next_sediment = sediment;
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                float vx_avg = (vx[x][y] + vx[x+1][y]) * 0.5f;
                float vy_avg = (vy[x][y] + vy[x][y+1]) * 0.5f;
                int nx = x + (vx_avg > 0.05f ? 1 : (vx_avg < -0.05f ? -1 : 0));
                int ny = y + (vy_avg > 0.05f ? 1 : (vy_avg < -0.05f ? -1 : 0));
                if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE && (nx != x || ny != y)) {
                    float amount = sediment[y][x] * 0.45f * dt;
                    next_sediment[y][x] -= amount;
                    next_sediment[ny][nx] += amount;
                }
            }
        }
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) sediment[y][x] = std::max(0.0f, next_sediment[y][x]);
        }

        // Advect C++ Tracer Particles
        // Spawn tracer particles uniformly across the entire grid up to a maximum of 100 active particles
        if (step % 1 == 0) {
            int active_count = 0;
            for (const auto& p : particles) {
                if (p.active) active_count++;
            }
            if (active_count < 100) {
                int to_spawn = std::min(5, 100 - active_count);
                for (int k = 0; k < to_spawn; ++k) {
                    float px = ((float)std::rand() / RAND_MAX) * (float)GRID_SIZE;
                    float py = ((float)std::rand() / RAND_MAX) * (float)GRID_SIZE;

                    Particle p;
                    p.id = next_particle_id++;
                    p.x = px;
                    p.y = py;
                    p.active = true;
                    p.stuck_frames = 0;
                    particles.push_back(p);
                }
            }
        }



        // Calculate average cell-centered velocities for interpolation
        std::vector<std::vector<float>> cur_grid_vx(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
        std::vector<std::vector<float>> cur_grid_vy(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                cur_grid_vx[y][x] = (vx[x][y] + vx[x+1][y]) * 0.5f;
                cur_grid_vy[y][x] = (vy[x][y] + vy[x][y+1]) * 0.5f;
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
                if (h_surface[cy][cx] <= 0.01f) {
                    is_dry = true;
                }
            }

            float speed = std::sqrt(p_vx * p_vx + p_vy * p_vy);
            if (is_dry || speed < 0.002f) {
                p.stuck_frames++;
            } else {
                p.stuck_frames = 0;
            }

            if (out_of_bounds || p.stuck_frames > 80 || (cx == 3 && cy == 3)) {
                p.active = false;
            }
        }

        // Prune inactive particles to keep the vector size strictly <= 100
        std::vector<Particle> active_particles;
        for (const auto& p : particles) {
            if (p.active) active_particles.push_back(p);
        }
        particles = active_particles;
    }

    // REGRESSION ASSERTS
    assert(history.size() == 201);


    // Save as test_stream.js using fixed float representation to keep file size lightweight
    std::ofstream out("test_stream.js");
    out << std::fixed << std::setprecision(2); // Cut ASCII decimal footprint
    out << "export const streamData = {\n";
    out << "  grid_size: " << GRID_SIZE << ",\n";
    out << "  steps: [\n";
    for (size_t s = 0; s < history.size(); ++s) {
        out << "    {\n";
        out << "      step: " << history[s].step << ",\n";
        out << "      grid_vx: [\n";
        for (int y = 0; y < GRID_SIZE; ++y) {
            out << "        [";
            for (int x = 0; x < GRID_SIZE; ++x) {
                out << history[s].grid_vx[y][x];
                if (x < GRID_SIZE - 1) out << ", ";
            }
            out << "]";
            if (y < GRID_SIZE - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_vy: [\n";
        for (int y = 0; y < GRID_SIZE; ++y) {
            out << "        [";
            for (int x = 0; x < GRID_SIZE; ++x) {
                out << history[s].grid_vy[y][x];
                if (x < GRID_SIZE - 1) out << ", ";
            }
            out << "]";
            if (y < GRID_SIZE - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_H_soil: [\n";
        for (int y = 0; y < GRID_SIZE; ++y) {
            out << "        [";
            for (int x = 0; x < GRID_SIZE; ++x) {
                out << history[s].H_soil[y][x];
                if (x < GRID_SIZE - 1) out << ", ";
            }
            out << "]";
            if (y < GRID_SIZE - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_h_surface: [\n";
        for (int y = 0; y < GRID_SIZE; ++y) {
            out << "        [";
            for (int x = 0; x < GRID_SIZE; ++x) {
                out << history[s].h_surface[y][x];
                if (x < GRID_SIZE - 1) out << ", ";
            }
            out << "]";
            if (y < GRID_SIZE - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_h_soil_water: [\n";
        for (int y = 0; y < GRID_SIZE; ++y) {
            out << "        [";
            for (int x = 0; x < GRID_SIZE; ++x) {
                out << history[s].h_soil_water[y][x];
                if (x < GRID_SIZE - 1) out << ", ";
            }
            out << "]";
            if (y < GRID_SIZE - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      particles: [\n";
        for (size_t i = 0; i < history[s].particles.size(); ++i) {
            out << "        { id: " << history[s].particles[i].id
                << ", x: " << history[s].particles[i].x
                << ", y: " << history[s].particles[i].y << " }";
            if (i < history[s].particles.size() - 1) out << ",";
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

    save_bmp("final_state.bmp", H_soil, h_surface, history[0].H_soil);

    std::cout << "SUCCESS: Stream test completed successfully." << std::endl;
    return 0;
}
