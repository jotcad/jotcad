#ifndef HYDRODYNAMICS_H
#define HYDRODYNAMICS_H

#include "element.h"

// 2. Hydrodynamics Element (Shallow Water Solver)
class Hydrodynamics : public Element {
private:
    float gravity;
    float drag;
public:
    Hydrodynamics(float g, float d) : gravity(g), drag(d) {}
    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;

        // Calculate dynamic CFL stability limit based on maximum water depth
        float max_h = 0.0f;
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                max_h = std::max(max_h, g.h_surface[y][x]);
            }
        }
        if (max_h > 0.01f) {
            float c_max = std::sqrt(gravity * max_h);
            float dt_max = 0.95f * (1.0f / (c_max + 0.001f)); // 0.95 safety factor
            if (dt > dt_max) {
                throw std::runtime_error("Hydrodynamics CFL Stability Violation: dt (" + std::to_string(dt) + 
                                         ") exceeds maximum stable time step (" + std::to_string(dt_max) + 
                                         ") for maximum water depth (" + std::to_string(max_h) + ")");
            }
        }

        std::vector<std::vector<float>> next_vx(sz + 1, std::vector<float>(sz, 0.0f));
        std::vector<std::vector<float>> next_vy(sz, std::vector<float>(sz + 1, 0.0f));

        // Horizontal flux acceleration
        for (int y = 0; y < sz; ++y) {
            for (int x = 1; x < sz; ++x) {
                float h_L = g.h_surface[y][x-1];
                float h_R = g.h_surface[y][x];
                if (h_L < 0.001f && h_R < 0.001f) {
                    next_vx[x][y] = 0.0f;
                } else {
                    float z_L = g.H_soil[y][x-1] + h_L;
                    float z_R = g.H_soil[y][x] + h_R;
                    float accel_x = -gravity * (z_R - z_L);
                    float depth = std::max(h_L, h_R);
                    float speed = std::abs(g.vx[x][y]);
                    float local_drag = drag * (0.05f * (1.0f + speed)) / (depth + 0.001f);
                    float val = (g.vx[x][y] + accel_x * dt) / (1.0f + local_drag * dt);
                    next_vx[x][y] = std::max(-3.0f, std::min(3.0f, val));
                }
            }
        }

        // Vertical flux acceleration
        for (int y = 1; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float h_T = g.h_surface[y-1][x];
                float h_B = g.h_surface[y][x];
                if (h_T < 0.001f && h_B < 0.001f) {
                    next_vy[x][y] = 0.0f;
                } else {
                    float z_T = g.H_soil[y-1][x] + h_T;
                    float z_B = g.H_soil[y][x] + h_B;
                    float accel_y = -gravity * (z_B - z_T);
                    float depth = std::max(h_T, h_B);
                    float speed = std::abs(g.vy[x][y]);
                    float local_drag = drag * (0.05f * (1.0f + speed)) / (depth + 0.001f);
                    float val = (g.vy[x][y] + accel_y * dt) / (1.0f + local_drag * dt);
                    next_vy[x][y] = std::max(-3.0f, std::min(3.0f, val));
                }
            }
        }

        // Apply boundary velocity clamping (closed box boundary) with boundary adjustments
        for (int y = 0; y < sz; ++y) {
            next_vx[0][y] = 0.0f;
            if (next_vx[1][y] < 0.0f) next_vx[1][y] = 0.0f;
            next_vx[sz][y] = 0.0f;
            if (next_vx[sz-1][y] > 0.0f) next_vx[sz-1][y] = 0.0f;
        }
        for (int x = 0; x < sz; ++x) {
            next_vy[x][0] = 0.0f;
            if (next_vy[x][1] < 0.0f) next_vy[x][1] = 0.0f;
            next_vy[x][sz] = 0.0f;
            if (next_vy[x][sz-1] > 0.0f) next_vy[x][sz-1] = 0.0f;
        }

        // Apply velocity smoothing (numerical viscosity) to prevent checkerboard oscillations
        std::vector<std::vector<float>> smoothed_vx = next_vx;
        for (int y = 1; y < sz - 1; ++y) {
            for (int x = 1; x < sz; ++x) {
                float laplacian = next_vx[x-1][y] + next_vx[x+1][y] + next_vx[x][y-1] + next_vx[x][y+1] - 4.0f * next_vx[x][y];
                smoothed_vx[x][y] = next_vx[x][y] + 0.01f * laplacian;
            }
        }
        g.vx = smoothed_vx;

        std::vector<std::vector<float>> smoothed_vy = next_vy;
        for (int y = 1; y < sz; ++y) {
            for (int x = 1; x < sz - 1; ++x) {
                float laplacian = next_vy[x-1][y] + next_vy[x+1][y] + next_vy[x][y-1] + next_vy[x][y+1] - 4.0f * next_vy[x][y];
                smoothed_vy[x][y] = next_vy[x][y] + 0.01f * laplacian;
            }
        }
        g.vy = smoothed_vy;

        // Compute Upwind Fluxes (Water transport matches cell depth, not just velocity)
        std::vector<std::vector<float>> qx(sz + 1, std::vector<float>(sz, 0.0f));
        std::vector<std::vector<float>> qy(sz, std::vector<float>(sz + 1, 0.0f));

        for (int y = 0; y < sz; ++y) {
            for (int x = 1; x < sz; ++x) {
                float v = g.vx[x][y];
                if (v >= 0.0f) {
                    qx[x][y] = v * g.h_surface[y][x-1];
                } else {
                    qx[x][y] = v * g.h_surface[y][x];
                }
            }
        }
        for (int y = 1; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float v = g.vy[x][y];
                if (v >= 0.0f) {
                    qy[x][y] = v * g.h_surface[y-1][x];
                } else {
                    qy[x][y] = v * g.h_surface[y][x];
                }
            }
        }

        // Apply Edge Boundaries to Fluxes
        for (int y = 0; y < sz; ++y) {
            if (g.vx[0][y] < 0.0f) qx[0][y] = g.vx[0][y] * g.h_surface[y][0];
            if (g.vx[sz][y] > 0.0f) qx[sz][y] = g.vx[sz][y] * g.h_surface[y][sz-1];
        }
        for (int x = 0; x < sz; ++x) {
            if (g.vy[x][0] < 0.0f) qy[x][0] = g.vy[x][0] * g.h_surface[0][x];
            if (g.vy[x][sz] > 0.0f) qy[x][sz] = g.vy[x][sz] * g.h_surface[sz-1][x];
        }

        // Compute Total Outflow per cell to prevent negative depth (Flux Limiting)
        std::vector<std::vector<float>> cell_outflow(sz, std::vector<float>(sz, 0.0f));
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float out = 0.0f;
                if (qx[x+1][y] > 0.0f) out += qx[x+1][y];
                if (qx[x][y] < 0.0f) out -= qx[x][y];
                if (qy[x][y+1] > 0.0f) out += qy[x][y+1];
                if (qy[x][y] < 0.0f) out -= qy[x][y];
                cell_outflow[y][x] = out;
            }
        }

        // Scale Outflow Fluxes to avoid draining more than is present
        std::vector<std::vector<float>> cell_scale(sz, std::vector<float>(sz, 1.0f));
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float limit = cell_outflow[y][x] * dt;
                if (limit > g.h_surface[y][x] && limit > 0.0f) {
                    cell_scale[y][x] = g.h_surface[y][x] / limit;
                }
            }
        }

        for (int y = 0; y < sz; ++y) {
            for (int x = 1; x < sz; ++x) {
                if (qx[x][y] >= 0.0f) {
                    qx[x][y] *= cell_scale[y][x-1];
                } else {
                    qx[x][y] *= cell_scale[y][x];
                }
            }
        }
        for (int y = 1; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                if (qy[x][y] >= 0.0f) {
                    qy[x][y] *= cell_scale[y-1][x];
                } else {
                    qy[x][y] *= cell_scale[y][x];
                }
            }
        }
        for (int y = 0; y < sz; ++y) {
            if (g.vx[0][y] < 0.0f) qx[0][y] *= cell_scale[y][0];
            if (g.vx[sz][y] > 0.0f) qx[sz][y] *= cell_scale[y][sz-1];
        }
        for (int x = 0; x < sz; ++x) {
            if (g.vy[x][0] < 0.0f) qy[x][0] *= cell_scale[0][x];
            if (g.vy[x][sz] > 0.0f) qy[x][sz] *= cell_scale[sz-1][x];
        }

        // Update Surface Water Depth with Scaled Fluxes
        std::vector<std::vector<float>> next_h(sz, std::vector<float>(sz, 0.0f));
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float net_outflow = (qx[x+1][y] - qx[x][y]) + (qy[x][y+1] - qy[x][y]);
                next_h[y][x] = g.h_surface[y][x] - net_outflow * dt;
                if (next_h[y][x] < 0.0f) next_h[y][x] = 0.0f;
            }
        }
        g.h_surface = next_h;

        // Perfectly conservative pairwise smoothing of surface water to prevent numerical checkerboard sloshing
        std::vector<std::vector<float>> smoothed_h = g.h_surface;
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                int dx[] = {1, -1, 0, 0};
                int dy[] = {0, 0, 1, -1};
                for (int i = 0; i < 4; ++i) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];
                    if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
                        float amount = g.h_surface[y][x] * 0.0025f; 
                        smoothed_h[y][x] -= amount;
                        smoothed_h[ny][nx] += amount;
                    }
                }
            }
        }
        g.h_surface = smoothed_h;
    }
};

// 2.5. Sub-Stepped Hydrodynamics Element (for realistic flow speeds over large timesteps)
class SubSteppedHydrodynamics : public Element {
private:
    float gravity;
    float drag;
    int num_substeps;
public:
    SubSteppedHydrodynamics(float g, float d, int substeps = 50)
        : gravity(g), drag(d), num_substeps(substeps) {}
        
    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        float dt_sub = dt / (float)num_substeps;
        
        // Pre-allocate temporary grid arrays to avoid heap allocation overhead in sub-step loop
        std::vector<std::vector<float>> next_vx(sz + 1, std::vector<float>(sz, 0.0f));
        std::vector<std::vector<float>> next_vy(sz, std::vector<float>(sz + 1, 0.0f));
        std::vector<std::vector<float>> smoothed_vx(sz + 1, std::vector<float>(sz, 0.0f));
        std::vector<std::vector<float>> smoothed_vy(sz, std::vector<float>(sz + 1, 0.0f));
        std::vector<std::vector<float>> qx(sz + 1, std::vector<float>(sz, 0.0f));
        std::vector<std::vector<float>> qy(sz, std::vector<float>(sz + 1, 0.0f));
        std::vector<std::vector<float>> cell_outflow(sz, std::vector<float>(sz, 0.0f));
        std::vector<std::vector<float>> cell_scale(sz, std::vector<float>(sz, 1.0f));
        std::vector<std::vector<float>> next_h(sz, std::vector<float>(sz, 0.0f));
        
        for (int substep = 0; substep < num_substeps; ++substep) {
            // Horizontal flux acceleration
            for (int y = 0; y < sz; ++y) {
                for (int x = 1; x < sz; ++x) {
                    float h_L = g.h_surface[y][x-1];
                    float h_R = g.h_surface[y][x];
                    if (h_L < 0.001f && h_R < 0.001f) {
                        next_vx[x][y] = 0.0f;
                    } else {
                        float z_L = g.H_soil[y][x-1] + h_L;
                        float z_R = g.H_soil[y][x] + h_R;
                        float accel_x = -gravity * 100000.0f * (z_R - z_L);
                        float depth = std::max(h_L, h_R);
                        float speed = std::abs(g.vx[x][y]);
                        float local_drag = drag * (0.05f * (1.0f + speed)) / (depth * g.scale.height_scale_m + 0.001f);
                        float val = (g.vx[x][y] + accel_x * dt_sub) / (1.0f + local_drag * dt_sub);
                        next_vx[x][y] = std::max(-10000.0f, std::min(10000.0f, val));
                    }
                }
            }

            // Vertical flux acceleration
            for (int y = 1; y < sz; ++y) {
                for (int x = 0; x < sz; ++x) {
                    float h_T = g.h_surface[y-1][x];
                    float h_B = g.h_surface[y][x];
                    if (h_T < 0.001f && h_B < 0.001f) {
                        next_vy[x][y] = 0.0f;
                    } else {
                        float z_T = g.H_soil[y-1][x] + h_T;
                        float z_B = g.H_soil[y][x] + h_B;
                        float accel_y = -gravity * 100000.0f * (z_B - z_T);
                        float depth = std::max(h_T, h_B);
                        float speed = std::abs(g.vy[x][y]);
                        float local_drag = drag * (0.05f * (1.0f + speed)) / (depth * g.scale.height_scale_m + 0.001f);
                        float val = (g.vy[x][y] + accel_y * dt_sub) / (1.0f + local_drag * dt_sub);
                        next_vy[x][y] = std::max(-10000.0f, std::min(10000.0f, val));
                    }
                }
            }

            // Apply boundary velocity clamping (closed box boundary)
            for (int y = 0; y < sz; ++y) {
                next_vx[0][y] = 0.0f;
                if (next_vx[1][y] < 0.0f) next_vx[1][y] = 0.0f;
                next_vx[sz][y] = 0.0f;
                if (next_vx[sz-1][y] > 0.0f) next_vx[sz-1][y] = 0.0f;
            }
            for (int x = 0; x < sz; ++x) {
                next_vy[x][0] = 0.0f;
                if (next_vy[x][1] < 0.0f) next_vy[x][1] = 0.0f;
                next_vy[x][sz] = 0.0f;
                if (next_vy[x][sz-1] > 0.0f) next_vy[x][sz-1] = 0.0f;
            }

            // Apply velocity smoothing (numerical viscosity)
            smoothed_vx = next_vx;
            float visc = 0.01f / (float)num_substeps;
            for (int y = 1; y < sz - 1; ++y) {
                for (int x = 1; x < sz; ++x) {
                    float laplacian = next_vx[x-1][y] + next_vx[x+1][y] + next_vx[x][y-1] + next_vx[x][y+1] - 4.0f * next_vx[x][y];
                    smoothed_vx[x][y] = next_vx[x][y] + visc * laplacian;
                }
            }
            g.vx = smoothed_vx;

            smoothed_vy = next_vy;
            for (int y = 1; y < sz; ++y) {
                for (int x = 1; x < sz - 1; ++x) {
                    float laplacian = next_vy[x-1][y] + next_vy[x+1][y] + next_vy[x][y-1] + next_vy[x][y+1] - 4.0f * next_vy[x][y];
                    smoothed_vy[x][y] = next_vy[x][y] + visc * laplacian;
                }
            }
            g.vy = smoothed_vy;

            // Compute Upwind Fluxes
            for (int y = 0; y < sz; ++y) {
                for (int x = 1; x < sz; ++x) {
                    float v = g.vx[x][y];
                    if (v >= 0.0f) {
                        qx[x][y] = v * g.h_surface[y][x-1];
                    } else {
                        qx[x][y] = v * g.h_surface[y][x];
                    }
                }
            }
            for (int y = 1; y < sz; ++y) {
                for (int x = 0; x < sz; ++x) {
                    float v = g.vy[x][y];
                    if (v >= 0.0f) {
                        qy[x][y] = v * g.h_surface[y-1][x];
                    } else {
                        qy[x][y] = v * g.h_surface[y][x];
                    }
                }
            }

            // Apply Edge Boundaries to Fluxes
            for (int y = 0; y < sz; ++y) {
                if (g.vx[0][y] < 0.0f) qx[0][y] = g.vx[0][y] * g.h_surface[y][0];
                if (g.vx[sz][y] > 0.0f) qx[sz][y] = g.vx[sz][y] * g.h_surface[y][sz-1];
            }
            for (int x = 0; x < sz; ++x) {
                if (g.vy[x][0] < 0.0f) qy[x][0] = g.vy[x][0] * g.h_surface[0][x];
                if (g.vy[x][sz] > 0.0f) qy[x][sz] = g.vy[x][sz] * g.h_surface[sz-1][x];
            }

            // Compute Total Outflow per cell (Flux Limiting)
            for (int y = 0; y < sz; ++y) {
                for (int x = 0; x < sz; ++x) {
                    float out = 0.0f;
                    if (qx[x+1][y] > 0.0f) out += qx[x+1][y];
                    if (qx[x][y] < 0.0f) out -= qx[x][y];
                    if (qy[x][y+1] > 0.0f) out += qy[x][y+1];
                    if (qy[x][y] < 0.0f) out -= qy[x][y];
                    cell_outflow[y][x] = out;
                }
            }

            // Scale Outflow Fluxes to avoid draining more than is present
            for (int y = 0; y < sz; ++y) {
                std::fill(cell_scale[y].begin(), cell_scale[y].end(), 1.0f);
            }
            for (int y = 0; y < sz; ++y) {
                for (int x = 0; x < sz; ++x) {
                    float limit = cell_outflow[y][x] * dt_sub;
                    if (limit > g.h_surface[y][x] && limit > 0.0f) {
                        cell_scale[y][x] = g.h_surface[y][x] / limit;
                    }
                }
            }

              for (int y = 0; y < sz; ++y) {
                  for (int x = 1; x < sz; ++x) {
                      if (qx[x][y] >= 0.0f) {
                          qx[x][y] *= cell_scale[y][x-1];
                      } else {
                          qx[x][y] *= cell_scale[y][x];
                      }
                  }
              }
              for (int y = 1; y < sz; ++y) {
                  for (int x = 0; x < sz; ++x) {
                      if (qy[x][y] >= 0.0f) {
                          qy[x][y] *= cell_scale[y-1][x];
                      } else {
                          qy[x][y] *= cell_scale[y][x];
                      }
                  }
              }
              for (int y = 0; y < sz; ++y) {
                  if (g.vx[0][y] < 0.0f) qx[0][y] *= cell_scale[y][0];
                  if (g.vx[sz][y] > 0.0f) qx[sz][y] *= cell_scale[y][sz-1];
              }
              for (int x = 0; x < sz; ++x) {
                  if (g.vy[x][0] < 0.0f) qy[x][0] *= cell_scale[0][x];
                  if (g.vy[x][sz] > 0.0f) qy[x][sz] *= cell_scale[sz-1][x];
              }

              // Update Surface Water Depth with Scaled Fluxes
              for (int y = 0; y < sz; ++y) {
                  for (int x = 0; x < sz; ++x) {
                      float net_outflow = (qx[x+1][y] - qx[x][y]) + (qy[x][y+1] - qy[x][y]);
                      next_h[y][x] = g.h_surface[y][x] - net_outflow * dt_sub;
                      if (next_h[y][x] < 0.0f) next_h[y][x] = 0.0f;
                  }
              }
              g.h_surface = next_h;

              // Maintain constant ocean level (z = 0.0) as Dirichlet boundary sink inside the sub-step loop
              for (int y = 0; y < sz; ++y) {
                  for (int x = 0; x < sz; ++x) {
                      if (g.H_soil[y][x] < 0.0f) {
                          g.h_surface[y][x] = 0.0f - g.H_soil[y][x];
                      }
                  }
              }
          }
      
      // Perfectly conservative pairwise smoothing of surface water (run once per main step)
      std::vector<std::vector<float>> smoothed_h = g.h_surface;
      for (int y = 0; y < sz; ++y) {
          for (int x = 0; x < sz; ++x) {
              int dx[] = {1, -1, 0, 0};
              int dy[] = {0, 0, 1, -1};
              for (int i = 0; i < 4; ++i) {
                  int nx = x + dx[i];
                  int ny = y + dy[i];
                  if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
                      float amount = g.h_surface[y][x] * 0.0025f; 
                      smoothed_h[y][x] -= amount;
                      smoothed_h[ny][nx] += amount;
                  }
              }
          }
      }
      g.h_surface = smoothed_h;
  }
};

  #endif // HYDRODYNAMICS_H
