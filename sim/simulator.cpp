#include "simulator.h"
#include "noise.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>
#include <queue>
#include <numeric>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../fs/cpp/vendor/stb_image_write.h"

namespace jotcad {
namespace sim {

// Constants
constexpr float GRAVITY = 9.81f;
constexpr float INFILTRATION_BASE = 0.02f;     // m/s
constexpr float SAT_CONDUCTIVITY = 0.005f;     // m/s (Darcy flow)
constexpr float BEDROCK_WEATHER_BASE = 0.0001f;// m/yr baseline weathering
constexpr float SPLINTER_BASE = 0.0002f;       // m/yr mechanical weathering
constexpr float K_LITHIFICATION = 0.0005f;     // compaction rate
constexpr float DRY_ANGLE_REPOSE = 0.6f;        // tangent of stable slope (~30 deg)
constexpr float LITH_OVERBURDEN_BASE = 1800.0f;// density of soil (kg/m3)
constexpr float SEA_LEVEL = 15.0f;              // base sea level height (m)

Simulator::Simulator(int w, int h)
    : width(w), height(h),
      global_temp_sea_level(25.0f),
      global_rain_rate(0.001f),
      wind_x(5.0f), wind_y(2.0f),
      step_count(0) {
    grid.resize(width * height);
}

void Simulator::initialize(unsigned int seed) {
    PerlinNoise noiseGen(seed);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            Cell& cell = grid[idx];
            
            // Base height for bedrock: hybrid ridged/fBm heterogeneous model
            float nx = (float)x / (float)width * 4.0f;
            float ny = (float)y / (float)height * 4.0f;
            
            // 1. Continental macro-structure (low-frequency noise)
            float continent_base = static_cast<float>(noiseGen.noise(nx * 0.3f, ny * 0.3f) * 22.0f + 16.0f);
            
            // 2. Mountain mask: mountains only form in the high continental shield highlands
            float mountain_mask = std::clamp((continent_base - 17.0f) / 12.0f, 0.0f, 1.0f);
            
            // 3. High-frequency details
            float m_elev = noiseGen.ridged(nx * 1.2f, ny * 1.2f, 6, 2.0, 0.5) * 55.0f;
            float p_elev = noiseGen.fBm(nx * 0.8f, ny * 0.8f, 3, 2.0, 0.4) * 10.0f;
            
            // Combine: macro continental base + plains + mountains modulated by the shield mask
            float elev = continent_base + p_elev + mountain_mask * m_elev;
            
            cell.bedrock = elev;
            cell.soil_mineral = 1.0f;  // 1 meter starting mineral soil
            cell.soil_organic = 0.1f;  // 10 cm starting organic soil
            cell.sediment = 0.0f;
            cell.groundwater = 0.2f;   // 20 cm starting groundwater depth
            
            // Initialize ocean floor below sea level
            float total_h = cell.terrain_height();
            if (total_h < SEA_LEVEL) {
                cell.water = SEA_LEVEL - total_h;
            } else {
                cell.water = 0.0f;
            }
            
            cell.grass = 0.1f;
            cell.tree = 0.02f;
            cell.moisture = 0.2f;
            cell.arability = 0.0f;
            cell.has_road = false;
            cell.settlement_type = 0;
            cell.temperature = 25.0f;
            cell.insolation = 1.0f;
            
            cell.flow_target_idx = -1;
            cell.flow_accumulation = 1.0f;
        }
    }
}

void Simulator::run_weathering(float dt) {
    for (int i = 0; i < width * height; ++i) {
        Cell& cell = grid[i];
        float total_soil = cell.soil_mineral + cell.soil_organic;
        
        // 1. Biological/Chemical Weathering: P = P0 * (1 + beta_g * grass + beta_t * tree) * exp(-alpha * h_soil)
        float bio_accel = 1.0f + 2.0f * cell.grass + 5.0f * cell.tree;
        float buffering = std::exp(-2.0f * total_soil);
        float p_weather = BEDROCK_WEATHER_BASE * bio_accel * buffering * dt;
        
        // Check bedrock availability
        p_weather = std::min(p_weather, cell.bedrock);
        cell.bedrock -= p_weather;
        cell.soil_mineral += p_weather;

        // 2. Mechanical Weathering (Frost/Thermal Splintering)
        // Highly active on bare rock peaks, buffered by soil
        float temp_cycles = std::max(0.0f, 15.0f - std::abs(cell.temperature - 0.0f)); // close to freezing drives wedging
        float w_splinter = SPLINTER_BASE * temp_cycles * std::exp(-4.0f * total_soil) * dt;
        w_splinter = std::min(w_splinter, cell.bedrock);
        cell.bedrock -= w_splinter;
        cell.soil_mineral += w_splinter;
    }
}

void Simulator::run_hydrology_and_infiltration(float dt) {
    // Periodic storms: rain rate cycles with high-intensity storm events
    // A storm peaks every 30 steps (with dt = 0.2, this is 6.0 units of time)
    float t_time = static_cast<float>(step_count) * dt;
    float storm_mod = 0.1f; // base dry season rainfall
    float cycle = std::sin(t_time * 0.5f); // wave period ~ 12.5 seconds (about 60 steps with dt=0.2)
    if (cycle > 0.6f) {
        // High intensity storm event!
        storm_mod = 3.0f + 5.0f * (cycle - 0.6f); // up to 5.0x rain rate
    } else if (cycle > 0.0f) {
        // Light rain season
        storm_mod = 0.5f;
    }
    float current_rain_rate = global_rain_rate * storm_mod;

    for (int i = 0; i < width * height; ++i) {
        Cell& cell = grid[i];
        
        // Dynamic infiltration capacity: f = f_base * (1 + eta_g * grass + eta_t * tree)
        float f = INFILTRATION_BASE * (1.0f + 1.5f * cell.grass + 3.0f * cell.tree);
        
        // Rainfall volume added
        float rain_vol = current_rain_rate * dt;
        float infiltrated = std::min(rain_vol, f * dt);
        
        // Infiltrate into groundwater table
        cell.groundwater += infiltrated;
        
        // Remaining becomes surface runoff water
        cell.water += (rain_vol - infiltrated);
        
        // Evaporation from surface water (aridity effect)
        float evap = 0.005f * dt; // 5 mm per unit time
        cell.water = std::max(0.0f, cell.water - evap);
        
        // Enforce ocean water level
        float ocean_depth = std::max(0.0f, SEA_LEVEL - cell.terrain_height());
        cell.water = std::max(cell.water, ocean_depth);
    }
}

void Simulator::run_groundwater_routing(float dt) {
    // Saturated groundwater routing using Darcy's Law virtual flows to neighbors
    std::vector<float> gw_outflow(width * height, 0.0f);
    
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            const Cell& cell = grid[idx];
            
            float head = cell.hydraulic_head();
            float max_flow = cell.groundwater; // cannot flow more than available groundwater
            if (max_flow <= 0.0f) continue;
            
            float total_out_grad = 0.0f;
            std::vector<float> grads(4, 0.0f);
            
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    int nidx = get_index(nx, ny);
                    float n_head = grid[nidx].hydraulic_head();
                    float grad = head - n_head;
                    if (grad > 0.0f) {
                        grads[d] = grad;
                        total_out_grad += grad;
                    }
                }
            }
            
            if (total_out_grad > 0.0f) {
                // Darcy's Law flux: Q = K * grad * dt
                float flow_vol = SAT_CONDUCTIVITY * total_out_grad * dt;
                flow_vol = std::min(flow_vol, max_flow);
                
                gw_outflow[idx] += flow_vol;
                
                // Distribute to neighbors proportionally to hydraulic head gradient
                for (int d = 0; d < 4; ++d) {
                    if (grads[d] > 0.0f) {
                        int nx = x + dx[d];
                        int ny = y + dy[d];
                        int nidx = get_index(nx, ny);
                        grid[nidx].groundwater += flow_vol * (grads[d] / total_out_grad);
                    }
                }
            }
        }
    }
    
    // Apply outflows and compute exfiltration (spring discharge)
    for (int i = 0; i < width * height; ++i) {
        Cell& cell = grid[i];
        cell.groundwater = std::max(0.0f, cell.groundwater - gw_outflow[i]);
        
        // Exfiltration: if water table rises above terrain surface
        float soil_depth = cell.soil_mineral + cell.soil_organic;
        if (cell.groundwater > soil_depth) {
            float seep = cell.groundwater - soil_depth;
            cell.water += seep;
            cell.groundwater = soil_depth; // groundwater clamped to surface
        }
        
        // Update soil moisture index based on groundwater saturation ratio
        cell.moisture = (soil_depth > 1e-5f) ? std::clamp(cell.groundwater / soil_depth, 0.0f, 1.0f) : 0.0f;
    }
}

void Simulator::run_wind_erosion(float dt) {
    // Simple Aeolian wind transport along the wind vector
    float wind_speed = std::sqrt(wind_x * wind_x + wind_y * wind_y);
    if (wind_speed < 0.1f) return;
    
    std::vector<float> sand_lifted(width * height, 0.0f);
    
    for (int i = 0; i < width * height; ++i) {
        Cell& cell = grid[i];
        
        // Wind transport capacity Q = K * wind_speed^3 * exp(-moisture) * exp(-vegetation)
        float moisture_damping = std::exp(-5.0f * cell.moisture);
        float veg_shielding = std::exp(-4.0f * (cell.grass + cell.tree));
        
        float q_wind = 0.0001f * (wind_speed * wind_speed * wind_speed) * moisture_damping * veg_shielding;
        float erodible = std::min(std::max(0.0f, cell.soil_mineral), q_wind * dt);
        
        sand_lifted[i] = erodible;
    }
    
    // Move wind-blown sediment downstream
    float angle = std::atan2(wind_y, wind_x);
    int step_x = (wind_x > 0.0f) ? 1 : ((wind_x < 0.0f) ? -1 : 0);
    int step_y = (wind_y > 0.0f) ? 1 : ((wind_y < 0.0f) ? -1 : 0);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            float lifted = sand_lifted[idx];
            if (lifted <= 0.0f) continue;
            
            grid[idx].soil_mineral = std::max(0.0f, grid[idx].soil_mineral - lifted);
            
            // Transport to target neighbor downwind
            int tx = x + step_x;
            int ty = y + step_y;
            if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
                int tidx = get_index(tx, ty);
                grid[tidx].soil_mineral += lifted;
            } else {
                // Discard off-grid
            }
        }
    }
}

void Simulator::run_hydraulic_incision(float dt) {
    // 1. Calculate D8 Routing target based on Hydraulic Head (terrain + water)
    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    float dist[] = {1.414f, 1.0f, 1.414f, 1.0f, 1.0f, 1.414f, 1.0f, 1.414f};
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            Cell& cell = grid[idx];
            cell.flow_target_idx = -1;
            cell.flow_accumulation = 1.0f; // Reset
            
            float h_hyd = cell.terrain_height() + cell.water;
            float max_slope = 0.0f;
            int target_idx = -1;
            
            for (int d = 0; d < 8; ++d) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    int nidx = get_index(nx, ny);
                    float n_hyd = grid[nidx].terrain_height() + grid[nidx].water;
                    float slope = (h_hyd - n_hyd) / dist[d];
                    if (slope > max_slope) {
                        max_slope = slope;
                        target_idx = nidx;
                    }
                }
            }
            cell.flow_target_idx = target_idx;
        }
    }
    
    // 2. Compute Flow Accumulation: route from highest to lowest cells
    std::vector<int> sorted_indices(width * height);
    std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
    std::sort(sorted_indices.begin(), sorted_indices.end(), [this](int a, int b) {
        return (grid[a].terrain_height() + grid[a].water) > (grid[b].terrain_height() + grid[b].water);
    });
    
    for (int idx : sorted_indices) {
        int target = grid[idx].flow_target_idx;
        if (target != -1) {
            grid[target].flow_accumulation += grid[idx].flow_accumulation;
        }
    }
    
    // 3. Dynamic Water and Sediment Flow routing (mass transfer)
    std::vector<float> water_outflow(width * height, 0.0f);
    std::vector<float> sed_outflow(width * height, 0.0f);
    float k_flow = 2.0f; // surface flow rate multiplier
    
    for (int idx : sorted_indices) {
        Cell& cell = grid[idx];
        int target = cell.flow_target_idx;
        if (target == -1 || cell.water <= 0.0f) continue;
        
        float h_hyd = cell.terrain_height() + cell.water;
        float n_hyd = grid[target].terrain_height() + grid[target].water;
        
        // Flow volume is limited by slope difference and available water
        float flow_vol = std::min(cell.water, (h_hyd - n_hyd) * 0.5f) * k_flow * dt;
        flow_vol = std::max(0.0f, flow_vol);
        
        // Sediment load carried is proportional to water flow fraction
        float sed_flow = cell.sediment * (flow_vol / (cell.water + 1e-6f));
        sed_flow = std::min(sed_flow, cell.sediment);
        
        water_outflow[idx] += flow_vol;
        sed_outflow[idx] += sed_flow;
        
        grid[target].water += flow_vol;
        grid[target].sediment += sed_flow;
    }
    
    // Apply outflows and boundary drainage
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            Cell& cell = grid[idx];
            cell.water = std::max(0.0f, cell.water - water_outflow[idx]);
            cell.sediment = std::max(0.0f, cell.sediment - sed_outflow[idx]);
            
            // Border cells drain off-grid to prevent accumulation at edges (above sea level)
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                float ocean_depth = std::max(0.0f, SEA_LEVEL - cell.terrain_height());
                cell.water = std::max(ocean_depth, cell.water - cell.water * k_flow * dt);
                cell.sediment = std::max(0.0f, cell.sediment - cell.sediment * k_flow * dt);
            }
        }
    }
    
    // 4. Channel Incision and Sediment Deposition
    for (int i = 0; i < width * height; ++i) {
        Cell& cell = grid[i];
        int target = cell.flow_target_idx;
        
        // Handle sediment deposition first
        if (cell.sediment > 0.0f) {
            float slope_g = 0.0f;
            if (target != -1) {
                slope_g = std::max(0.0f, (cell.terrain_height() - grid[target].terrain_height()));
            }
            
            // Transport capacity is proportional to water depth and slope
            float capacity = 0.1f * cell.water * slope_g;
            if (cell.sediment > capacity) {
                float excess = cell.sediment - capacity;
                float deposit = excess * 0.2f; // deposition settling rate
                cell.soil_mineral += deposit;
                cell.sediment -= deposit;
            }
        }
        
        // Handle stream power incision
        if (target == -1 || cell.water <= 0.01f) continue;
        
        float slope = (cell.terrain_height() - grid[target].terrain_height());
        if (slope <= 0.0f) continue;
        
        float root_mitigation = std::exp(-2.0f * cell.grass - 4.0f * cell.tree);
        float ke = 0.002f * root_mitigation;
        
        float e_depth = ke * std::sqrt(cell.flow_accumulation) * slope * dt;
        
        float soil_depth = cell.soil_mineral + cell.soil_organic;
        if (e_depth <= soil_depth) {
            float ratio = (soil_depth > 1e-5f) ? (e_depth / soil_depth) : 0.0f;
            float mineral_eroded = cell.soil_mineral * ratio;
            float organic_eroded = cell.soil_organic * ratio;
            
            cell.soil_mineral = std::max(0.0f, cell.soil_mineral - mineral_eroded);
            cell.soil_organic = std::max(0.0f, cell.soil_organic - organic_eroded);
            
            // Add eroded material to suspended load
            cell.sediment += (mineral_eroded + organic_eroded);
        } else {
            float extra = e_depth - soil_depth;
            float bedrock_eroded = std::min(cell.bedrock, extra);
            
            cell.bedrock = std::max(0.0f, cell.bedrock - bedrock_eroded);
            cell.soil_mineral = 0.0f;
            cell.soil_organic = 0.0f;
            
            // Add eroded material to suspended load
            cell.sediment += (soil_depth + bedrock_eroded);
        }
    }
    
    // Ensure ocean water is maintained for all cells
    for (int i = 0; i < width * height; ++i) {
        Cell& cell = grid[i];
        float ocean_depth = std::max(0.0f, SEA_LEVEL - cell.terrain_height());
        cell.water = std::max(cell.water, ocean_depth);
    }
}

void Simulator::run_vegetation_dynamics(float dt) {
    // Environmental limits and biological growth
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            Cell& cell = grid[idx];
            float h = cell.terrain_height();
            
            // 1. Climate Lapse Rate
            cell.temperature = global_temp_sea_level - 0.0065f * h;
            
            // 2. Sunlight Aspect Angle Index (South vs North)
            // Approximate aspect from gradient (simple finite difference)
            float dzdy = 0.0f;
            if (y > 0 && y < height - 1) {
                dzdy = (grid[get_index(x, y+1)].terrain_height() - grid[get_index(x, y-1)].terrain_height()) / 2.0f;
            }
            // In Northern Hemisphere, south-facing (dz/dy < 0) gets more sun
            cell.insolation = std::max(0.2f, 1.0f - dzdy * 0.1f);
            
            // 3. Tree Line Limit
            float tree_suit_temp = (cell.temperature > 10.0f) ? 1.0f : 0.0f;
            
            // 4. Growth Suitability Curves
            // Grass moisture suitability (bell curve)
            float g_moist_suit = std::exp(-std::pow(cell.moisture - 0.4f, 2) / (2.0f * 0.1f * 0.1f));
            // Tree moisture suitability: sigmoid (pushed tree limit to wetter soils to allow grassy plains)
            float t_moist_suit = 1.0f / (1.0f + std::exp(-12.0f * (cell.moisture - 0.48f)));
            
            // Soil depth limits
            float total_soil = cell.soil_mineral + cell.soil_organic;
            float g_soil_suit = 1.0f - std::exp(-total_soil / 0.05f); // viable at 5cm
            float t_soil_suit = 1.0f - std::exp(-total_soil / 0.50f); // viable at 50cm
            
            // 5. Tree Light Competition (Canopy shadow on Grass)
            float grass_sun = cell.insolation * (1.0f - cell.tree);
            float tree_sun = cell.insolation;
            
            // Water limitation: trees cannot grow in standing water > 10cm
            float tree_suit_water = (cell.water < 0.10f) ? 1.0f : 0.0f;
            
            // 6. Growth Equation Execution
            float grass_growth = 0.15f * g_moist_suit * g_soil_suit * grass_sun * (1.0f - cell.grass) - 0.05f * cell.grass;
            float tree_growth = 0.04f * t_moist_suit * t_soil_suit * tree_sun * tree_suit_temp * tree_suit_water * (1.0f - cell.tree) - 0.01f * cell.tree;
            
            cell.grass = std::clamp(cell.grass + grass_growth * dt, 0.0f, 1.0f);
            cell.tree = std::clamp(cell.tree + tree_growth * dt, 0.0f, 1.0f);
            
            // 7. Litterfall and Humification (Organic Soil Construction)
            float litter = (0.05f * cell.grass + 0.01f * cell.tree) * dt;
            float humification_factor = 0.15f; // 15% converted to stable organic humus
            float organic_prod = humification_factor * litter;
            float decomposition = 0.02f * cell.soil_organic * dt; // slow humic respiration
            
            cell.soil_organic = std::max(0.0f, cell.soil_organic + organic_prod - decomposition);
            
            // 8. Evapotranspiration (root water uptake and soil evaporation)
            float veg_frac = std::clamp(cell.grass + cell.tree, 0.0f, 1.0f);
            float et = (0.0002f * (1.0f - veg_frac) + 0.0008f * veg_frac) * cell.moisture * cell.insolation * dt;
            cell.groundwater = std::max(0.0f, cell.groundwater - et);
            
            // 9. Calculate Crop Arability Index
            float slope_val = 0.0f;
            int target = cell.flow_target_idx;
            if (target != -1) {
                slope_val = std::max(0.0f, (cell.terrain_height() - grid[target].terrain_height()));
            }
            float f_soil = 1.0f - std::exp(-total_soil / 0.3f);
            float f_slope = std::exp(-std::pow(slope_val / 0.2f, 2));
            float f_moist = std::exp(-std::pow(cell.moisture - 0.5f, 2) / (2.0f * 0.2f * 0.2f));
            float f_temp = (cell.temperature > 5.0f) ? std::clamp((cell.temperature - 5.0f) / 10.0f, 0.0f, 1.0f) : 0.0f;
            
            // Only land above sea level is arable
            if (cell.terrain_height() >= SEA_LEVEL) {
                cell.arability = f_soil * f_slope * f_moist * f_temp;
            } else {
                cell.arability = 0.0f;
            }
        }
    }
}

void Simulator::run_bank_slumping(float dt) {
    // Talus slumping based on dynamic angle of repose (friction decreases when wet, increases with roots)
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            int idx = get_index(x, y);
            Cell& cell = grid[idx];
            float h = cell.terrain_height();
            float soil_depth = cell.soil_mineral + cell.soil_organic;
            if (soil_depth <= 0.0f) continue;
            
            // Compute dynamic angle of repose (maximum stable slope tangent)
            // Wetness reduces stability, tree roots increase stability dramatically
            float root_cohesion = 0.2f * cell.grass + 0.6f * cell.tree * std::min(1.0f, soil_depth);
            float max_stable_slope = DRY_ANGLE_REPOSE * (1.0f - 0.6f * cell.moisture) + root_cohesion;
            
            for (int d = 0; d < 4; ++d) {
                if (soil_depth <= 1e-5f) break;
                int nx = x + dx[d];
                int ny = y + dy[d];
                int nidx = get_index(nx, ny);
                Cell& neighbor = grid[nidx];
                
                float slope = h - neighbor.terrain_height();
                if (slope > max_stable_slope) {
                    if (soil_depth <= 1e-5f) break;
                    // Move excess soil material to stabilize
                    float excess = (slope - max_stable_slope) * 0.5f;
                    excess = std::min(excess, soil_depth);
                    
                    // Displace proportionately
                    float ratio = excess / soil_depth;
                    float m_soil = cell.soil_mineral * ratio;
                    float o_soil = cell.soil_organic * ratio;
                    
                    cell.soil_mineral = std::max(0.0f, cell.soil_mineral - m_soil);
                    cell.soil_organic = std::max(0.0f, cell.soil_organic - o_soil);
                    
                    neighbor.soil_mineral += m_soil;
                    neighbor.soil_organic += o_soil;
                    
                    h = cell.terrain_height();
                    soil_depth = cell.soil_mineral + cell.soil_organic;
                }
            }
        }
    }
}

void Simulator::run_lithification(float dt) {
    for (int i = 0; i < width * height; ++i) {
        Cell& cell = grid[i];
        if (cell.soil_mineral <= 0.0f) continue;
        
        // Pressure conversion: compaction of bottom soil layer to bedrock
        float total_soil = cell.soil_mineral + cell.soil_organic;
        float pressure = LITH_OVERBURDEN_BASE * GRAVITY * total_soil;
        
        float lithified = K_LITHIFICATION * cell.soil_mineral * (pressure / 10000.0f) * dt;
        lithified = std::min(lithified, cell.soil_mineral);
        
        cell.soil_mineral -= lithified;
        cell.bedrock += lithified;
    }
}

void Simulator::step(float dt) {
    step_count++;
    run_weathering(dt);
    run_hydrology_and_infiltration(dt);
    run_groundwater_routing(dt);
    run_wind_erosion(dt);
    run_hydraulic_incision(dt);
    run_vegetation_dynamics(dt);
    run_bank_slumping(dt);
    run_lithification(dt);
}

bool Simulator::save_to_obj(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    
    out << "# JotCAD Terrain Mesh Export\n";
    
    // Write vertices
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            const Cell& cell = grid[idx];
            out << "v " << x << " " << cell.terrain_height() << " " << y << "\n";
        }
    }
    
    // Write faces (1-based index)
    for (int y = 0; y < height - 1; ++y) {
        for (int x = 0; x < width - 1; ++x) {
            int v0 = y * width + x + 1;
            int v1 = v0 + 1;
            int v2 = (y + 1) * width + x + 1;
            int v3 = v2 + 1;
            
            // Triangle 1
            out << "f " << v0 << " " << v2 << " " << v1 << "\n";
            // Triangle 2
            out << "f " << v1 << " " << v2 << " " << v3 << "\n";
        }
    }
    
    out.close();
    return true;
}

bool Simulator::save_layers_csv(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    
    out << "x,y,bedrock,soil_mineral,soil_organic,groundwater,grass,tree,moisture,temperature,arability\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            const Cell& cell = grid[idx];
            out << x << "," << y << ","
                << cell.bedrock << ","
                << cell.soil_mineral << ","
                << cell.soil_organic << ","
                << cell.groundwater << ","
                << cell.grass << ","
                << cell.tree << ","
                << cell.moisture << ","
                << cell.temperature << ","
                << cell.arability << "\n";
        }
    }
    out.close();
    return true;
}

bool Simulator::save_to_png(const std::string& filepath) const {
    std::vector<unsigned char> pixels(width * height * 3);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            const Cell& cell = grid[idx];
            
            // Calculate slope gradient for hillshading
            float h_l = (x > 0) ? grid[get_index(x-1, y)].terrain_height() : cell.terrain_height();
            float h_r = (x < width - 1) ? grid[get_index(x+1, y)].terrain_height() : cell.terrain_height();
            float h_d = (y > 0) ? grid[get_index(x, y-1)].terrain_height() : cell.terrain_height();
            float h_u = (y < height - 1) ? grid[get_index(x, y+1)].terrain_height() : cell.terrain_height();
            
            float dx = (h_r - h_l) / 2.0f;
            float dy = (h_u - h_d) / 2.0f;
            
            // Normal vector
            float nx = -dx;
            float ny = -dy;
            float nz = 1.0f;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= len; ny /= len; nz /= len;
            
            // Light source from North-East
            float lx = 1.0f;
            float ly = -1.0f;
            float lz = 1.0f;
            float llen = std::sqrt(lx * lx + ly * ly + lz * lz);
            lx /= llen; ly /= llen; lz /= llen;
            
            float diff = nx * lx + ny * ly + nz * lz;
            float shade = 0.5f + 0.5f * diff;
            
            // Check for settlements in neighboring pixels to draw larger icons
            int draw_settlement = 0;
            // Radius 2 search for Cities (4)
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        int nidx = get_index(nx, ny);
                        if (grid[nidx].settlement_type == 4) {
                            draw_settlement = 4;
                            break;
                        }
                    }
                }
                if (draw_settlement != 0) break;
            }
            
            // Radius 1 search for Towns (3) and Villages (2)
            if (draw_settlement == 0) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            int nidx = get_index(nx, ny);
                            if (grid[nidx].settlement_type == 3) {
                                draw_settlement = 3;
                                break;
                            }
                            if (grid[nidx].settlement_type == 2) {
                                draw_settlement = 2;
                                break;
                            }
                        }
                    }
                    if (draw_settlement != 0) break;
                }
            }
            
            // Hamlet (1) is a single pixel
            if (draw_settlement == 0 && cell.settlement_type == 1) {
                draw_settlement = 1;
            }
            
            // Color mapping based on cell state
            float r = 0.0f, g = 0.0f, b = 0.0f;
            bool is_road_or_settlement = false;
            
            if (draw_settlement == 4) {
                // City: Red
                r = 255.0f; g = 30.0f; b = 30.0f;
                is_road_or_settlement = true;
            } else if (draw_settlement == 3) {
                // Town: Orange
                r = 240.0f; g = 110.0f; b = 10.0f;
                is_road_or_settlement = true;
            } else if (draw_settlement == 2) {
                // Village: Yellow
                r = 230.0f; g = 180.0f; b = 20.0f;
                is_road_or_settlement = true;
            } else if (draw_settlement == 1) {
                // Hamlet: White
                r = 250.0f; g = 250.0f; b = 250.0f;
                is_road_or_settlement = true;
            } else if (cell.has_road) {
                // Roads: Charcoal
                r = 55.0f; g = 55.0f; b = 55.0f;
                is_road_or_settlement = true;
            } else if (cell.water > 0.02f || cell.flow_accumulation > 80.0f) {
                // Water / River / Lake (Blue depth gradient)
                float depth = cell.water;
                float t = std::clamp((depth - 0.02f) / 39.98f, 0.0f, 1.0f);
                r = (1.0f - t) * 50.0f + t * 10.0f;
                g = (1.0f - t) * 180.0f + t * 40.0f;
                b = (1.0f - t) * 230.0f + t * 120.0f;
                
                // Flat water surface normal
                nx = 0.0f; ny = 0.0f; nz = 1.0f;
                diff = nx * lx + ny * ly + nz * lz;
                shade = 0.5f + 0.5f * diff;
            } else if (cell.arability > 0.45f) {
                // Highly arable land: Golden yellow crops!
                float t = (cell.arability - 0.45f) / 0.55f; // [0, 1]
                r = (1.0f - t) * 190.0f + t * 230.0f;
                g = (1.0f - t) * 160.0f + t * 195.0f;
                b = (1.0f - t) * 60.0f  + t * 45.0f;
            } else if (cell.tree > 0.15f) {
                // Tree (Dark Green)
                r = 20.0f; g = 90.0f; b = 30.0f;
            } else if (cell.grass > 0.15f) {
                // Grass (Light Green)
                r = 100.0f; g = 175.0f; b = 70.0f;
            } else if (cell.soil_mineral + cell.soil_organic > 0.1f) {
                // Soil (Brown)
                r = 150.0f; g = 115.0f; b = 80.0f;
            } else {
                // Bedrock (Gray)
                r = 120.0f; g = 120.0f; b = 120.0f;
            }
            
            // Apply hillshading (skipped for cartographical elements)
            if (!is_road_or_settlement) {
                r *= shade; g *= shade; b *= shade;
            }
            
            int pix_idx = (y * width + x) * 3;
            pixels[pix_idx] = (unsigned char)std::clamp(r, 0.0f, 255.0f);
            pixels[pix_idx + 1] = (unsigned char)std::clamp(g, 0.0f, 255.0f);
            pixels[pix_idx + 2] = (unsigned char)std::clamp(b, 0.0f, 255.0f);
        }
    }
    
    // Write using STB
    int success = stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
    return success != 0;
}

bool Simulator::save_arability_png(const std::string& filepath) const {
    std::vector<unsigned char> pixels(width * height * 3);
    
    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    float dist[] = {1.414f, 1.0f, 1.414f, 1.0f, 1.0f, 1.414f, 1.0f, 1.414f};
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            const Cell& cell = grid[idx];
            
            // Calculate shade
            float dzdx = 0.0f;
            float dzdy = 0.0f;
            for (int d = 0; d < 8; ++d) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    int nidx = get_index(nx, ny);
                    float diff = grid[nidx].terrain_height() - cell.terrain_height();
                    if (dx[d] != 0) dzdx += diff / (dx[d] * dist[d]);
                    if (dy[d] != 0) dzdy += diff / (dy[d] * dist[d]);
                }
            }
            float nx_val = -dzdx;
            float ny_val = -dzdy;
            float nz_val = 1.0f;
            float len = std::sqrt(nx_val*nx_val + ny_val*ny_val + nz_val*nz_val);
            nx_val /= len; ny_val /= len; nz_val /= len;
            
            float lx = 0.5f; float ly = 0.5f; float lz = 0.707f;
            float shade = nx_val*lx + ny_val*ly + nz_val*lz;
            shade = 0.5f + 0.5f * shade;
            
            float r, g, b;
            
            if (cell.terrain_height() < SEA_LEVEL) {
                // Ocean: deep dark blue
                r = 10.0f; g = 50.0f; b = 100.0f;
            } else {
                // Land base: grayscale based on height
                float h_norm = std::clamp((cell.terrain_height() - SEA_LEVEL) / 50.0f, 0.0f, 1.0f);
                float base_gray = 110.0f + h_norm * 90.0f;
                
                // Mix in arability color: highly arable is rich green, non-arable is grayscale
                float a = cell.arability;
                
                r = base_gray * (1.0f - a) + 90.0f * a;
                g = base_gray * (1.0f - a) + 180.0f * a;
                b = base_gray * (1.0f - a) + 40.0f * a;
            }
            
            r *= shade; g *= shade; b *= shade;
            
            int pix_idx = (y * width + x) * 3;
            pixels[pix_idx] = (unsigned char)std::clamp(r, 0.0f, 255.0f);
            pixels[pix_idx + 1] = (unsigned char)std::clamp(g, 0.0f, 255.0f);
            pixels[pix_idx + 2] = (unsigned char)std::clamp(b, 0.0f, 255.0f);
        }
    }
    
    int success = stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
    return success != 0;
}

struct PathNode {
    int idx;
    float cost;
    bool operator>(const PathNode& other) const {
        return cost > other.cost;
    }
};

void Simulator::connect_with_road(int from_idx, int to_idx) {
    if (from_idx == to_idx) return;
    
    std::priority_queue<PathNode, std::vector<PathNode>, std::greater<PathNode>> pq;
    std::vector<float> dist(width * height, 1e9f);
    std::vector<int> parent(width * height, -1);
    
    dist[from_idx] = 0.0f;
    pq.push({from_idx, 0.0f});
    
    int dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
    int dy[] = {0, 0, -1, 1, -1, -1, 1, 1};
    float step_dist[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.414f, 1.414f, 1.414f, 1.414f};
    
    int x_target = to_idx % width;
    int y_target = to_idx / width;
    
    while (!pq.empty()) {
        PathNode curr = pq.top();
        pq.pop();
        
        if (curr.idx == to_idx) break;
        if (curr.cost > dist[curr.idx]) continue;
        
        int cx = curr.idx % width;
        int cy = curr.idx / width;
        
        for (int d = 0; d < 8; ++d) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                int nidx = ny * width + nx;
                const Cell& ncell = grid[nidx];
                
                float water_penalty = 1.0f;
                if (ncell.terrain_height() < SEA_LEVEL || ncell.water > 0.1f) {
                    water_penalty = 25.0f; // High bridge building cost
                }
                
                float slope = std::abs(ncell.terrain_height() - grid[curr.idx].terrain_height()) / step_dist[d];
                float slope_penalty = 1.0f + 30.0f * slope * slope;
                
                float sharing_bonus = ncell.has_road ? 0.05f : 1.0f; // road sharing
                
                float weight = step_dist[d] * water_penalty * slope_penalty * sharing_bonus;
                float next_dist = dist[curr.idx] + weight;
                
                float h = std::sqrt(static_cast<float>((nx - x_target)*(nx - x_target) + (ny - y_target)*(ny - y_target)));
                
                if (next_dist < dist[nidx]) {
                    dist[nidx] = next_dist;
                    parent[nidx] = curr.idx;
                    pq.push({nidx, next_dist + h});
                }
            }
        }
    }
    
    int curr_idx = to_idx;
    while (curr_idx != -1) {
        grid[curr_idx].has_road = true;
        curr_idx = parent[curr_idx];
    }
}

void Simulator::run_settlement_simulation() {
    std::vector<float> suitability(width * height, 0.0f);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = get_index(x, y);
            Cell& cell = grid[idx];
            
            if (cell.terrain_height() < SEA_LEVEL || cell.water > 0.05f) {
                suitability[idx] = -1.0f;
                continue;
            }
            
            float local_arable = 0.0f;
            int count = 0;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        local_arable += grid[ny * width + nx].arability;
                        count++;
                    }
                }
            }
            float arability_factor = (count > 0) ? (local_arable / count) : 0.0f;
            
            float slope_val = 0.0f;
            if (cell.flow_target_idx != -1) {
                slope_val = std::max(0.0f, (cell.terrain_height() - grid[cell.flow_target_idx].terrain_height()));
            }
            float slope_factor = std::exp(-std::pow(slope_val / 0.10f, 2));
            
            float water_bonus = 1.0f;
            bool adjacent_to_sea = false;
            bool adjacent_to_river = false;
            
            int ndx[] = {-1, 1, 0, 0};
            int ndy[] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int nx = x + ndx[d];
                int ny = y + ndy[d];
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    int nidx = ny * width + nx;
                    if (grid[nidx].terrain_height() < SEA_LEVEL) {
                        adjacent_to_sea = true;
                    }
                    if (grid[nidx].flow_accumulation > 60.0f) {
                        adjacent_to_river = true;
                    }
                }
            }
            
            if (adjacent_to_sea && adjacent_to_river) {
                water_bonus = 2.0f; // River mouth confluences
            } else if (adjacent_to_sea) {
                water_bonus = 1.5f; // Harbors
            } else if (adjacent_to_river) {
                water_bonus = 1.4f; // Water trade access
            }
            
            suitability[idx] = arability_factor * slope_factor * water_bonus;
        }
    }
    
    struct Settlement {
        int idx;
        int type;
        float suitability;
    };
    std::vector<Settlement> settlements;
    
    std::vector<int> indices(width * height);
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&suitability](int a, int b) {
        return suitability[a] > suitability[b];
    });
    
    auto get_dist = [this](int a, int b) {
        int ax = a % width; int ay = a / width;
        int bx = b % width; int by = b / width;
        return std::sqrt(static_cast<float>((ax - bx)*(ax - bx) + (ay - by)*(ay - by)));
    };
    
    int max_cities = 3;
    int max_towns = 8;
    int max_villages = 18;
    int max_hamlets = 28;
    
    for (int idx : indices) {
        if (suitability[idx] <= 0.05f) continue;
        
        float min_dist = 1e9f;
        for (const auto& s : settlements) {
            float d = get_dist(idx, s.idx);
            if (d < min_dist) {
                min_dist = d;
            }
        }
        
        int type_to_place = 0;
        if (min_dist > 35.0f && max_cities > 0) {
            type_to_place = 4; // City
            max_cities--;
        } else if (min_dist > 20.0f && max_towns > 0) {
            type_to_place = 3; // Town
            max_towns--;
        } else if (min_dist > 10.0f && max_villages > 0) {
            type_to_place = 2; // Village
            max_villages--;
        } else if (min_dist > 5.0f && max_hamlets > 0) {
            type_to_place = 1; // Hamlet
            max_hamlets--;
        }
        
        if (type_to_place > 0) {
            grid[idx].settlement_type = type_to_place;
            settlements.push_back({idx, type_to_place, suitability[idx]});
        }
        
        if (max_cities == 0 && max_towns == 0 && max_villages == 0 && max_hamlets == 0) {
            break;
        }
    }
    
    // Connect Cities with highways
    for (size_t i = 0; i < settlements.size(); ++i) {
        if (settlements[i].type != 4) continue;
        for (size_t j = i + 1; j < settlements.size(); ++j) {
            if (settlements[j].type != 4) continue;
            connect_with_road(settlements[i].idx, settlements[j].idx);
        }
    }
    
    // Connect Towns to nearest City or Town
    for (size_t i = 0; i < settlements.size(); ++i) {
        if (settlements[i].type != 3) continue;
        float min_d = 1e9f;
        int best_target = -1;
        for (size_t j = 0; j < settlements.size(); ++j) {
            if (i == j || settlements[j].type < 3) continue;
            float d = get_dist(settlements[i].idx, settlements[j].idx);
            if (d < min_d) {
                min_d = d;
                best_target = settlements[j].idx;
            }
        }
        if (best_target != -1) {
            connect_with_road(settlements[i].idx, best_target);
        }
    }
    
    // Connect Villages to nearest Town or City
    for (size_t i = 0; i < settlements.size(); ++i) {
        if (settlements[i].type != 2) continue;
        float min_d = 1e9f;
        int best_target = -1;
        for (size_t j = 0; j < settlements.size(); ++j) {
            if (i == j || settlements[j].type < 2) continue;
            float d = get_dist(settlements[i].idx, settlements[j].idx);
            if (d < min_d) {
                min_d = d;
                best_target = settlements[j].idx;
            }
        }
        if (best_target != -1) {
            connect_with_road(settlements[i].idx, best_target);
        }
    }
}

} // namespace sim
} // namespace jotcad
