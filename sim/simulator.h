#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace jotcad {
namespace sim {

const int SEA_LEVEL = 20;

// Compatibility 2D surface column structure
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
    bool has_sea_route;
    int settlement_type;   // 0 = none, 1 = hamlet, 2 = village, 3 = town, 4 = city
    int nation_id;         // 0 = neutral, 1, 2, 3 = nations
    int bedrock_type;      // 0 = Granite, 1 = Limestone, 2 = Sandstone
    int mineral_type;      // 0 = none, 1 = Coal, 2 = Iron, 3 = Gold
    
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

// 3D Voxel Materials
enum Material {
    MAT_AIR = 0,
    MAT_GRANITE = 1,
    MAT_LIMESTONE = 2,
    MAT_SANDSTONE = 3,
    MAT_SOIL = 4,
    MAT_WATER = 5,
    MAT_COAL = 6,
    MAT_IRON = 7,
    MAT_GOLD = 8,
    MAT_ROAD = 9,
    MAT_SETTLEMENT_HAMLET = 10,
    MAT_SETTLEMENT_VILLAGE = 11,
    MAT_SETTLEMENT_TOWN = 12,
    MAT_SETTLEMENT_CITY = 13,
    MAT_PORT = 14,
    MAT_SEA_ROUTE = 15
};

// 3D Spatial Chunk (16x16x16 voxels)
struct Chunk {
    uint8_t voxels[16 * 16 * 16];
    uint8_t nation_ids[16 * 16 * 16];
};

class Simulator {
private:
    int width;
    int height;
    int depth; // 3D voxel depth (default: 64)
    int chunk_w, chunk_h, chunk_d;
    std::vector<Chunk*> chunks;
    std::vector<Cell> grid; // Compatibility 2D surface grid
    
    float global_temp_sea_level;
    float global_rain_rate;
    float wind_x;
    float wind_y;
    int step_count;
    unsigned int seed;
    std::string scenario;

    inline int get_chunk_idx(int cx, int cy, int cz) const {
        return (cz * chunk_h + cy) * chunk_w + cx;
    }

    void connect_with_road(int from_idx, int to_idx);
    void connect_with_sea_route(int from_idx, int to_idx);
    void sync_grid_from_voxels(float dt = 0.2f);

public:
    Simulator(int w, int h);
    ~Simulator();
    
    void set_scenario(const std::string& sc) { scenario = sc; }
    std::string get_scenario() const { return scenario; }
    
    void initialize(unsigned int seed);
    void step(float dt);
    void run_settlement_simulation();
    
    uint8_t get_voxel(int x, int y, int z) const;
    void set_voxel(int x, int y, int z, uint8_t mat);
    
    uint8_t get_nation(int x, int y, int z) const;
    void set_nation(int x, int y, int z, uint8_t nid);
    
    bool save_to_obj(const std::string& filepath) const;
    bool save_layers_csv(const std::string& filepath) const;
    bool save_to_png(const std::string& filepath, bool show_cutaway = false) const;
    bool save_top_view_png(const std::string& filepath) const;
    bool save_side_view_png(const std::string& filepath) const;
    bool save_arability_png(const std::string& filepath) const;

    // Getters for debugging/CLI
    int get_width() const { return width; }
    int get_height() const { return height; }
    const std::vector<Cell>& get_grid() const { return grid; }
};

} // namespace sim
} // namespace jotcad
