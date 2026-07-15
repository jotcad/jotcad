#ifndef HEX_CLIMATE_H
#define HEX_CLIMATE_H

#include <string>
#include <vector>

// Unified struct representing a simulation climate profile
struct ClimateProfile {
    std::string name;
    float rain_rate;               // Annual rainfall in m/yr (e.g. 1.4f)
    float base_temp;               // Warm lowland temperature at H=0 in Celsius (e.g. 25.0f)
    float lapse_rate_divisor;      // Altitude divisor for cooling (meters per 1C cooling, e.g. 85.0f)
    float min_temp_limit;          // Cut-off temperature for vegetation growth in Celsius (e.g. 10.0f)
    float growth_temp_range;       // Celsius range above limit for full growth (e.g. 8.0f)
    float growth_rate;             // Base growth rate of vegetation (e.g. 0.16f)
    float rain_threshold;          // Minimum rain in meters for full growth (e.g. 0.8f)
    float evap_coefficient;        // Evaporation rate K in m/yr/C (e.g. 0.0024f)
    float gw_initial_sat;          // Initial groundwater saturation fraction (0.0 to 1.0)
    float gw_infiltration_rate;    // Groundwater annual infiltration rate (1/yr)
    float gw_conductivity;         // Darcy hydraulic conductivity K_sat (m/yr)
};

// Registry housing available climate profiles
class ClimateRegistry {
public:
    static std::vector<ClimateProfile> get_all_profiles() {
        return {
            {
                "Subtropical Highland",
                1.4f,    // 1.4 m/yr rain
                25.0f,   // 25C base temperature
                85.0f,   // 85m altitude cooling divisor (treeline)
                10.0f,   // 10C limit
                8.0f,    // 8C temperature range for scaling
                0.16f,   // 0.16 vegetation growth rate
                0.8f,    // 0.8m annual rain threshold
                0.0024f, // 0.0024 evaporation coefficient
                0.15f,   // 0.15 initial saturation (15%)
                0.25f,   // 0.25 infiltration rate (25%)
                60.0f    // 60.0 m/yr conductivity (clayey loam)
            },
            {
                "Arid Desert",
                0.15f,   // 0.15 m/yr rain (extremely dry)
                35.0f,   // 35C base temperature (hot desert)
                90.0f,   // 90m altitude cooling divisor
                8.0f,    // 8C temperature limit
                12.0f,   // 12C range for scaling
                0.03f,   // 0.03 vegetation growth rate
                0.8f,    // 0.8m annual rain threshold
                0.004286f, // 0.004286 evaporation coefficient (hot/arid, net-zero valley balance)
                0.0f,    // 0.0 initial saturation (0% - completely dry aquifer)
                0.04f,   // 0.04 infiltration rate (4% - crust repelled)
                250.0f   // 250.0 m/yr conductivity (coarse sand)
            }
        };
    }

    static ClimateProfile get_profile(const std::string& name) {
        auto profiles = get_all_profiles();
        for (const auto& p : profiles) {
            if (p.name == name) return p;
        }
        // Fallback to the first profile (Subtropical Highland) if not found
        return profiles[0];
    }
};

#endif // HEX_CLIMATE_H
