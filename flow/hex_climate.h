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
    float lake_threshold;          // Water depth threshold for rendering/statistics (m)
    bool check_wetland_groundwater; // True if saturated shallow water table represents wetlands
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
                60.0f,   // 60.0 m/yr conductivity (clayey loam)
                0.10f,   // 0.10m lake threshold
                true     // check_wetland_groundwater
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
                0.0060f, // 0.0060 evaporation coefficient (hot/arid, dry desert)
                0.0f,    // 0.0 initial saturation (0% - completely dry aquifer)
                0.04f,   // 0.04 infiltration rate (4% - crust repelled)
                250.0f,  // 250.0 m/yr conductivity (coarse sand)
                0.25f,   // 0.25m lake threshold
                false    // check_wetland_groundwater
            },
            {
                "Boreal Forest",
                0.75f,   // 0.75 m/yr rain (moderate rain/snowmelt)
                8.0f,    // 8C base temperature (cold subpolar)
                70.0f,   // 70m lapse rate divisor (coniferous alpine treeline)
                0.0f,    // 0C min temp limit for conifer growth
                10.0f,   // 10C temperature range for scaling
                0.08f,   // 0.08 slow vegetation growth rate
                0.4f,    // 0.4m rain threshold (efficient cold usage)
                0.0012f, // 0.0012 evaporation coefficient (very low evap due to cold)
                0.30f,   // 0.30 initial saturation (30% - boggy starting conditions)
                0.40f,   // 0.40 infiltration rate (40% - saturated peat)
                2.0f,    // 2.0 m/yr conductivity (extremely slow-draining peat)
                0.05f,   // 0.05m lake threshold (show shallow wetlands)
                true     // check_wetland_groundwater
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
