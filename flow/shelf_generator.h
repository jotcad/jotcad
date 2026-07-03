#ifndef SHELF_GENERATOR_H
#define SHELF_GENERATOR_H

#include "grid.h"
#include "perlin_noise.h"
#include <cmath>
#include <algorithm>

// Continental Shelf Generator class
class ShelfGenerator {
public:
    // Generates a West-to-East continental shelf profile on H_soil
    static void generate(Grid& g, 
                         float coast_u = 0.25f,      // Normalized X coordinate of shoreline
                         float break_u = 0.60f,      // Normalized X coordinate of shelf break
                         float abyssal_u = 0.85f,    // Normalized X coordinate of abyssal plain start
                         float coast_z = 0.20f,      // Elevation of shoreline
                         float break_z = -0.50f,     // Depth of shelf break
                         float abyssal_z = -5.00f)   // Depth of abyssal plain
    {
        int sz = g.size;
        PerlinNoise2D perlin;

        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float u = (float)x / (sz - 1); // Normalized X coordinate (0.0 = West, 1.0 = East)

                // Base profile calculation
                float z_base = 0.0f;
                if (u < coast_u) {
                    // 1. Coastal Landmass (slopes down to sea level)
                    float t = u / coast_u;
                    float land_height = 3.00f; // height of land at West edge
                    z_base = land_height * (1.0f - t) + coast_z * t;
                } else if (u < break_u) {
                    // 2. Continental Shelf (very gentle slope down to shelf break)
                    float t = (u - coast_u) / (break_u - coast_u);
                    z_base = coast_z * (1.0f - t) + break_z * t;
                } else if (u < abyssal_u) {
                    // 3. Continental Slope (steep drop-off)
                    float t = (u - break_u) / (abyssal_u - break_u);
                    // Smooth Hermite interpolation for realistic shelf break rounding
                    float t_smooth = t * t * (3.0f - 2.0f * t);
                    z_base = break_z * (1.0f - t_smooth) + abyssal_z * t_smooth;
                } else {
                    // 4. Abyssal Plain (flat, deep ocean)
                    z_base = abyssal_z;
                }

                // Add landscape noise
                float noise_land = perlin.noise(x * 0.08f, y * 0.08f) * 0.50f + perlin.noise(x * 0.20f, y * 0.20f) * 0.15f;
                float noise_marine = perlin.noise(x * 0.05f, y * 0.05f) * 0.18f;

                // Scale noise based on zone (land has higher relief than marine abyssal plain)
                if (u < coast_u) {
                    z_base += noise_land * (1.0f - u / coast_u);
                } else {
                    z_base += noise_marine;
                }

                // 5. Incise Submarine Canyons on the Slope face
                if (u >= break_u && u < abyssal_u) {
                    float slope_t = (u - break_u) / (abyssal_u - break_u);
                    
                    // Canyon position modulated along Y-axis
                    // We carve 3 main canyon tracks
                    float canyon_factor = 0.0f;
                    for (float y_center : {0.25f * sz, 0.50f * sz, 0.75f * sz}) {
                        // Curved canyon track using sine offset
                        float track_y = y_center + std::sin(u * 8.0f) * 3.5f;
                        float dist_y = std::abs((float)y - track_y);
                        
                        // Gaussian cross-section profile of the canyon
                        if (dist_y < 7.0f) {
                            float width_factor = 1.0f - (dist_y / 7.0f);
                            // Canyons are deepest in the middle of the continental slope
                            float depth_envelope = 4.0f * slope_t * (1.0f - slope_t);
                            canyon_factor = std::max(canyon_factor, width_factor * depth_envelope * 0.95f);
                        }
                    }
                    z_base -= canyon_factor;
                }

                g.H_soil[y][x] = z_base;
            }
        }
    }
};

#endif // SHELF_GENERATOR_H
