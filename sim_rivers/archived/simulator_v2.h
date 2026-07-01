#ifndef SIM_RIVERS_SIMULATOR_H
#define SIM_RIVERS_SIMULATOR_H
 
#include <vector>
#include <string>
#include <random>
 
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
 
class Simulator {
private:
    int width;
    int height;
    std::vector<Cell> grid;
    std::string scenario;
    int step_count;
    unsigned int seed;
    std::mt19937 rng;
 
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
 
    int get_width() const { return width; }
    int get_height() const { return height; }
};
 
} // namespace sim_rivers
} // namespace jotcad
 
#endif // SIM_RIVERS_SIMULATOR_H
