#ifndef STREAM_EXPORTER_H
#define STREAM_EXPORTER_H

#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include "grid.h"

struct GridState {
    int step;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface;
    std::vector<std::vector<float>> grid_vx;
    std::vector<std::vector<float>> grid_vy;
    struct ParticleExport {
        int id;
        float x, y;
    };
    std::vector<ParticleExport> particles;
    std::vector<std::vector<float>> h_soil_water;
};

inline void save_bmp(const std::string& filename, const std::vector<std::vector<float>>& H_soil, const std::vector<std::vector<float>>& h_surface, const std::vector<std::vector<float>>& H_initial) {
    int w = H_soil[0].size();
    int h = H_soil.size();
    int row_size = w * 3;
    int padding = (4 - (row_size % 4)) % 4;
    int filesize = 54 + (row_size + padding) * h;
    
    unsigned char bmpFileHeader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpInfoHeader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
    
    bmpFileHeader[ 2] = (unsigned char)(filesize      );
    bmpFileHeader[ 3] = (unsigned char)(filesize >>  8);
    bmpFileHeader[ 4] = (unsigned char)(filesize >> 16);
    bmpFileHeader[ 5] = (unsigned char)(filesize >> 24);
    
    bmpInfoHeader[ 4] = (unsigned char)(w      );
    bmpInfoHeader[ 5] = (unsigned char)(w >>  8);
    bmpInfoHeader[ 6] = (unsigned char)(w >> 16);
    bmpInfoHeader[ 7] = (unsigned char)(w >> 24);
    bmpInfoHeader[ 8] = (unsigned char)(h      );
    bmpInfoHeader[ 9] = (unsigned char)(h >>  8);
    bmpInfoHeader[10] = (unsigned char)(h >> 16);
    bmpInfoHeader[11] = (unsigned char)(h >> 24);

    std::ofstream f(filename, std::ios::out | std::ios::binary);
    f.write((char*)bmpFileHeader, 14);
    f.write((char*)bmpInfoHeader, 40);

    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            float H = H_soil[y][x];
            float H0 = H_initial[y][x];
            float water = h_surface[y][x];
            float erosion = std::max(0.0f, H0 - H);

            unsigned char r = 0, g = 0, b = 0;

            float h_L = x > 0 ? H_soil[y][x-1] : H;
            float h_R = x < w - 1 ? H_soil[y][x+1] : H;
            float h_T = y > 0 ? H_soil[y-1][x] : H;
            float h_B = y < h - 1 ? H_soil[y+1][x] : H;
            
            float dx = h_R - h_L;
            float dy = h_B - h_T;
            float shadow = 1.0f - 0.20f * dx - 0.20f * dy;
            float shade = std::max(0.55f, std::min(1.45f, shadow));

            float normH = std::max(0.0f, std::min(1.0f, (H + 4.0f) / 8.0f));
            
            float sr = (34.0f + (80.0f - 34.0f) * normH) * shade;
            float sg = (197.0f + (50.0f - 197.0f) * normH) * shade;
            float sb = (94.0f + (25.0f - 94.0f) * normH) * shade;

            if (erosion > 0.015f) {
                float t = std::min(1.0f, erosion * 6.0f);
                sr = (1.0f - t) * sr + t * 71.0f;
                sg = (1.0f - t) * sg + t * 85.0f;
                sb = (1.0f - t) * sb + t * 105.0f;
            }

            r = (unsigned char)std::max(0.0f, std::min(255.0f, sr));
            g = (unsigned char)std::max(0.0f, std::min(255.0f, sg));
            b = (unsigned char)std::max(0.0f, std::min(255.0f, sb));

            if (water > 0.01f) {
                float w_norm = std::min(1.0f, water / 0.50f);
                float wr = 56.0f + (3.0f - 56.0f) * w_norm;
                float wg = 189.0f + (105.0f - 189.0f) * w_norm;
                float wb = 248.0f + (161.0f - 248.0f) * w_norm;
                
                float w_opacity = std::min(0.90f, 0.40f + w_norm * 0.40f);
                r = (unsigned char)((1.0f - w_opacity) * r + w_opacity * wr);
                g = (unsigned char)((1.0f - w_opacity) * g + w_opacity * wg);
                b = (unsigned char)((1.0f - w_opacity) * b + w_opacity * wb);
            }

            // Mark the sink as red in BMP
            if (x == 74 && y == 49) {
                r = 239; g = 68; b = 68;
            }
            f.put(b); f.put(g); f.put(r);
        }
        for (int p = 0; p < padding; ++p) {
            f.put(0);
        }
    }
}

inline void export_to_javascript(const std::string& filename, const std::vector<GridState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const streamData = {\n";
    out << "  grid_size: " << grid_size << ",\n";
    out << "  steps: [\n";
    for (size_t s = 0; s < history.size(); ++s) {
        out << "    {\n";
        out << "      step: " << history[s].step << ",\n";
        out << "      grid_H_soil: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].H_soil[y][x];
                if (x < grid_size - 1) out << ", ";
            }
            out << "]";
            if (y < grid_size - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_h_surface: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].h_surface[y][x];
                if (x < grid_size - 1) out << ", ";
            }
            out << "]";
            if (y < grid_size - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_vx: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].grid_vx[y][x];
                if (x < grid_size - 1) out << ", ";
            }
            out << "]";
            if (y < grid_size - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_vy: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].grid_vy[y][x];
                if (x < grid_size - 1) out << ", ";
            }
            out << "]";
            if (y < grid_size - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_h_soil_water: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].h_soil_water[y][x];
                if (x < grid_size - 1) out << ", ";
            }
            out << "]";
            if (y < grid_size - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      particles: [\n";
        for (size_t i = 0; i < history[s].particles.size(); ++i) {
            out << "        { id: " << history[s].particles[i].id
                << ", x: " << history[s].particles[i].x
                << ", y: " << history[s].particles[i].y << " }";
            if (i < history[s].particles.size() - 1) out << ",";
            out << "\n";
        }
        out << "      ]\n";
        out << "    }";
        if (s < history.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "};\n";
    out.close();
}

#endif // STREAM_EXPORTER_H
