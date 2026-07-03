#ifndef SHELF_GENERATOR_H
#define SHELF_GENERATOR_H

#include "grid.h"
#include "perlin_noise.h"
#include <cmath>
#include <algorithm>

// Continental Shelf Generator class setting up a geophysical rifting basement profile
class ShelfGenerator {
public:
    // Generates an irregular continent with realistic crustal thickness zones (shelf, slope, abyss)
    static void generate(Grid& g, 
                         float coast_u = 0.35f,      // Hinge line / coast boundary reference
                         float break_u = 0.60f,      // Shelf break boundary reference
                         float abyssal_u = 0.82f,    // Abyssal plain start reference
                         float coast_z = 0.15f,      // Shoreline elevation reference
                         float break_z = -0.50f,     // Shelf break platform depth
                         float abyssal_z = -4.50f)   // Abyssal plain depth
    {
        int sz = g.size;
        PerlinNoise2D perlin;
        float center_x = sz / 2.0f;
        float center_y = sz / 2.0f;
        float R_max = sz / 2.0f; // Maximum radius to grid edges

        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float dx = (float)x - center_x;
                float dy = (float)y - center_y;
                
                // Rotate coordinate system 90 degrees clockwise (rx = -dy, ry = dx)
                float rx = -dy;
                float ry = dx;

                // Rotated grid coordinates for noise lookups
                float x_rot = center_x + rx;
                float y_rot = center_y + ry;

                float r_dist = std::sqrt(rx*rx + ry*ry);
                
                // Deform the radial coordinate with 2D Perlin noise to make the continent organic and irregular
                float u_noise = perlin.noise(x_rot * 0.04f, y_rot * 0.04f) * 0.22f + 
                                perlin.noise(x_rot * 0.12f, y_rot * 0.12f) * 0.08f;
                float u = r_dist / R_max + u_noise;

                // Base profile calculation reflecting crustal thickness step zones
                float z_base = 0.0f;
                if (u < coast_u) {
                    // 1. Thick Continental Crust (Highlands sloping to coast hinge line)
                    float t = u / coast_u;
                    float land_height = 2.80f;
                    z_base = land_height * (1.0f - t) + coast_z * t;
                } else if (u < break_u) {
                    // 2. Thinned Continental Crust (Shallow Shelf Platform basement)
                    float t = (u - coast_u) / (break_u - coast_u);
                    z_base = coast_z * (1.0f - t) + break_z * t;
                } else if (u < abyssal_u) {
                    // 3. Continent-Ocean Transition Zone (Steep Tectonic Slope)
                    float t = (u - break_u) / (abyssal_u - break_u);
                    float t_smooth = t * t * (3.0f - 2.0f * t); // Hermite curve
                    z_base = break_z * (1.0f - t_smooth) + abyssal_z * t_smooth;
                } else {
                    // 4. Thin Oceanic Crust (Deep Abyssal Plain)
                    z_base = abyssal_z;
                }

                // Add subaerial mountain ruggedness to the highlands
                float noise_land = perlin.noise(x_rot * 0.08f, y_rot * 0.08f) * 0.45f + 
                                   perlin.noise(x_rot * 0.22f, y_rot * 0.22f) * 0.12f;
                if (u < coast_u) {
                    z_base += noise_land * (1.0f - u / coast_u);
                }

                // Add tectonic abyssal hills (fault ridges) to the ocean basin floor
                if (u >= abyssal_u) {
                    float ridge_noise = perlin.noise(x_rot * 0.14f, y_rot * 0.03f) * 0.38f;
                    float fade = std::min(1.0f, (u - abyssal_u) / 0.06f);
                    z_base += ridge_noise * fade;
                }

                g.H_soil[y][x] = z_base;
            }
        }
    }
};

#endif // SHELF_GENERATOR_H
