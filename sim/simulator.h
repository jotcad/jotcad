#pragma once
#include <vector>
#include <string>

namespace jotcad {
namespace sim {

struct Cell {
    float bedrock;         // H_bedrock (m)
    float soil_mineral;    // h_mineral (m)
    float soil_organic;    // h_organic (m)
    float water;           // surface water depth (m)
    float sediment;        // suspended sediment (m3)
    float groundwater;     // groundwater depth h_gw (m)
    
    // Derived/ecological states
    float moisture;        // soil moisture index
    float grass;           // grass density [0, 1]
    float tree;            // tree density [0, 1]
    float arability;       // crop arability index [0, 1]
    
    // Environmental/Climatic states
    float temperature;     // local temperature (C)
    float insolation;      // solar insolation [0, 1]
    
    // Settlement & Infrastructure
    bool has_road;
    int settlement_type;   // 0 = none, 1 = hamlet, 2 = village, 3 = town, 4 = city
    
    // Helper helper for D8 flow routing
    int flow_target_idx;   // index of cell this drains to (-1 if sink)
    float flow_accumulation;// accumulated upslope cells

    // Combined elevation
    float terrain_height() const {
        return bedrock + soil_mineral + soil_organic;
    }
    float hydraulic_head() const {
        return bedrock + groundwater;
    }
};

class Simulator {
private:
    int width;
    int height;
    std::vector<Cell> grid;
    float global_temp_sea_level;
    float global_rain_rate;
    float wind_x;
    float wind_y;
    int step_count;

    // Internal simulation operators
    void run_weathering(float dt);
    void run_hydrology_and_infiltration(float dt);
    void run_groundwater_routing(float dt);
    void run_wind_erosion(float dt);
    void run_hydraulic_incision(float dt);
    void run_vegetation_dynamics(float dt);
    void run_bank_slumping(float dt);
    void run_lithification(float dt);

    // Helpers
    inline int get_index(int x, int y) const {
        return y * width + x;
    }
    void connect_with_road(int from_idx, int to_idx);

public:
    Simulator(int w, int h);
    
    void initialize(unsigned int seed);
    void step(float dt);
    void run_settlement_simulation();
    
    bool save_to_obj(const std::string& filepath) const;
    bool save_layers_csv(const std::string& filepath) const;
    bool save_to_png(const std::string& filepath) const;
    bool save_arability_png(const std::string& filepath) const;

    // Getters for debugging/CLI
    int get_width() const { return width; }
    int get_height() const { return height; }
    const std::vector<Cell>& get_grid() const { return grid; }
};

} // namespace sim
} // namespace jotcad
