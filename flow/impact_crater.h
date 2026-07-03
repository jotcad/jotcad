#ifndef IMPACT_CRATER_H
#define IMPACT_CRATER_H

#include "element.h"
#include <cmath>
#include <algorithm>
#include <vector>

// ImpactCrater Element representing kinetic bolide excavation, ejecta deposition, and elastic peak rebound
class ImpactCrater : public Element {
private:
    float x_c, y_c;
    float radius;
    float depth;
    float H_peak; // Central rebound peak height

public:
    ImpactCrater(float xc, float yc, float r, float d, float peak_h)
        : x_c(xc), y_c(yc), radius(r), depth(d), H_peak(peak_h) {}

    void step(Grid& g, float dt, int step, int total_steps) override {
        // Apply the physical impact excavation only once at the first step
        if (step != 1) return;

        int sz = g.size;
        float H_rim_peak = depth * 0.22f; // Rim height is geologically ~22% of crater depth

        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float dx = (float)x - x_c;
                float dy = (float)y - y_c;
                float r = std::sqrt(dx*dx + dy*dy);

                if (r < radius) {
                    // 1. Excavation bowl cavity
                    float excav = -depth * (1.0f - (r / radius) * (r / radius));
                    g.H_soil[y][x] += excav;

                    // 2. Central elastic rebound peak (Gaussian)
                    float r_peak = 0.15f * radius;
                    float peak = H_peak * std::exp(-(r * r) / (r_peak * r_peak));
                    g.H_soil[y][x] += peak;
                } else {
                    // 3. Ejecta rim deposition decaying at r^-3 power law
                    float ratio = r / radius;
                    float ejecta = H_rim_peak * std::pow(ratio, -3.0f);
                    ejecta = std::min(ejecta, H_rim_peak); // Cap thickness at the rim edge
                    g.H_soil[y][x] += ejecta;
                }
            }
        }
    }
};

#endif // IMPACT_CRATER_H
