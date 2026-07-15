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
                0.8f     // 0.8m annual rain threshold
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
