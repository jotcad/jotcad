#ifndef SIM_RIVERS_SIMULATOR_H
#define SIM_RIVERS_SIMULATOR_H
 
#include <vector>
#include <string>
#include <random>
#include <map>
#include "FastNoiseLite.h"

namespace jotcad {
namespace sim_rivers {
 
struct Cell {
    float bedrock;          // Bedrock elevation (m)
    float soil;             // Mineral soil thickness (m)
    float water;            // Surface water depth (m)
    float groundwater;      // Subsoil groundwater storage (m)
    float sediment;         // Suspended sediment (m)
    float flow_acc;         // Flow accumulation value
    int flow_target_idx;    // D8 downhill target index (-1 if sink)
    float soil_clay;        // Clay/silt fraction thickness (m)
    float soil_gravel;      // Sand/gravel fraction thickness (m)
    float vx;               // Persistent water velocity x (m/s)
    float vy;               // Persistent water velocity y (m/s)
    
    float terrain_height() const {
        return bedrock + soil_clay + soil_gravel;
    }
    float hydraulic_head() const {
        return bedrock + soil_clay + soil_gravel + water;
    }
    float subsoil_head() const {
        return bedrock + groundwater;
    }
};

// SoilMachine RLE linked list structures
struct sec {
    sec* next = nullptr;     // Element Above
    sec* prev = nullptr;     // Element Below
    int type = 0;            // Soil type index
    double size = 0.0;       // Thickness
    double floor = 0.0;      // Cumulative height at bottom
    double saturation = 0.0; // Water saturation
};

class secpool {
public:
    int size = 0;
    sec* start = nullptr;
    std::vector<sec*> free_list;

    secpool() {}
    secpool(int N) { reserve(N); }
    ~secpool() {
        delete[] start;
    }
    void reserve(int N) {
        start = new sec[N];
        free_list.resize(N);
        for (int i = 0; i < N; ++i) {
            free_list[i] = &start[i];
        }
        size = N;
    }
    sec* get(double size_val, int type_val) {
        if (free_list.empty()) {
            return nullptr;
        }
        sec* E = free_list.back();
        free_list.pop_back();
        E->next = nullptr;
        E->prev = nullptr;
        E->size = size_val;
        E->type = type_val;
        E->floor = 0.0;
        E->saturation = 0.0;
        return E;
    }
    void unget(sec* E) {
        if (E == nullptr) return;
        E->next = nullptr;
        E->prev = nullptr;
        E->size = 0.0;
        E->floor = 0.0;
        E->saturation = 0.0;
        free_list.push_back(E);
    }
    void reset() {
        free_list.clear();
        for (int i = 0; i < size; ++i) {
            start[i].next = nullptr;
            start[i].prev = nullptr;
            free_list.push_back(&start[i]);
        }
    }
};

struct Color4 {
    float r = 0.5f, g = 0.5f, b = 0.5f, a = 1.0f;
};

struct SurfParam {
    std::string name;
    float density = 1.0f;
    float porosity = 0.0f;
    Color4 color;
    Color4 phong;

    int transports = 0;
    float solubility = 1.0f;
    float equrate = 1.0f;
    float friction = 1.0f;

    int erodes = 0;
    float erosionrate = 0.0f;

    int cascades = 0;
    float maxdiff = 1.0f;
    float settling = 0.0f;

    int abrades = 0;
    float suspension = 0.0f;
    float abrasion = 0.0f;
};

struct SurfLayer {
    int type = 0;
    float min = 0.0f;
    float bias = 0.0f;
    float scale = 1.0f;
    FastNoiseLite noise;
    float octaves = 1.0f;
    float lacunarity = 1.0f;
    float gain = 0.0f;
    float frequency = 1.0f;

    SurfLayer(int _type) : type(_type) {}

    void init() {
        noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        noise.SetFractalType(FastNoiseLite::FractalType_FBm);
        noise.SetFractalOctaves(octaves);
        noise.SetFractalLacunarity(lacunarity);
        noise.SetFractalGain(gain);
        noise.SetFrequency(frequency);
    }

    float get(float x, float y, float z) {
        float val = bias + scale * noise.GetNoise(x, y, z);
        if (val < min) val = min;
        return val;
    }
};
 
class Simulator {
private:
    int width;
    int height;
    std::vector<Cell> grid;
    std::string scenario;
    int step_count;
    unsigned int seed;
    std::mt19937 rng;

    // SoilMachine members
    secpool pool;
    std::vector<sec*> dat; // Layermap grid of top section pointers
    std::vector<SurfParam> soils;
    std::map<std::string, int> soilmap;
    std::vector<SurfLayer> layers;

    // World parameters parsed from soil config
    int SIZEX = 256;
    int SIZEY = 256;
    int SCALE = 80;
    int NWIND = 100;
    int NWATER = 250;

    // Water and Wind Particle tracking maps
    std::vector<float> water_frequency;
    std::vector<float> water_track;
    std::vector<float> wind_frequency;
    std::vector<float> forest_density;

    // Layermap management helpers
    void loadsoil(const std::string& filepath);
    void add_layer(int x, int y, sec* E);
    double remove_layer(int x, int y, double h);
    double get_height(int x, int y) const;
    int get_surface_type(int x, int y) const;
    void cascade_scree(int x, int y, int transferloop);
    void seep_water(int x, int y);
    void cascade_water(int x, int y, int spill);

    struct WaterParticle;
    struct WindParticle;

    bool water_move(WaterParticle& p, const std::vector<float>& w_freq);
    bool water_interact(WaterParticle& p);
    bool wind_move(WindParticle& p);
    bool wind_interact(WindParticle& p);

    void sync_grid_from_layermap();
 
    // Modular Forest & Vegetation parameters and growth loops
    bool grows_trees(int x, int y) const;
    int get_neighbor_tree_count(int x, int y) const;
    float get_soil_solubility_modifier(int x, int y) const;
    void grow_forest_soil();
    void update_hydrology_and_transpiration();
 
public:
    Simulator(int w, int h);
    ~Simulator();
 
    void set_scenario(const std::string& sc) { scenario = sc; }
    std::string get_scenario() const { return scenario; }
 
    void initialize(unsigned int seed);
    void step(float dt);
 
    bool save_layers_csv(const std::string& filepath) const;
    bool save_top_view_png(const std::string& filepath) const;
    bool save_side_view_png(const std::string& filepath) const;
    bool save_iso_view_png(const std::string& filepath) const;
    bool save_velocity_png(const std::string& filepath) const;
 
    void print_diagnostics() const;

    int get_width() const { return width; }
    int get_height() const { return height; }
};
 
} // namespace sim_rivers
} // namespace jotcad
 
#endif // SIM_RIVERS_SIMULATOR_H
