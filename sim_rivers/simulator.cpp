#include "simulator.h"
#include "noise.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <iostream>
#include <random>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../fs/cpp/vendor/stb_image_write.h"

namespace jotcad {
namespace sim_rivers {

// Physical scale constants for mapping simulation grid coordinates to world dimensions
const float GRID_CELL_SIZE_M = 1000.0f;       // 1 km horizontal cell step
const float VERTICAL_UNIT_M = 20.0f;          // Each height unit = 20.0 meters
const float CONTOUR_INTERVAL_M = 50.0f;       // Contour line spacing in meters
const float VERTICAL_EXAGGERATION = 1.0f;      // Visual vertical exaggeration for 3D relief (1.0f = none)

// Physical evolution time mapping constants
const float TOTAL_SIMULATION_YEARS = 500.0f;   // 500 years of total landscape evolution
const float DT_PER_STEP_YEARS = 0.5f;          // Each simulation step = 6 months (0.5 years)

static PerlinNoise noiseGen;

struct Vector2 {
    float x;
    float y;
    Vector2(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}
    void multiplyScalar(float s) {
        x *= s;
        y *= s;
    }
};

struct Vector3 {
    float x;
    float y;
    float z;
    Vector3(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) : x(_x), y(_y), z(_z) {}
    Vector3& add(const Vector3& o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    Vector3& multiplyScalar(float s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    Vector3 normalize() const {
        float len = std::sqrt(x * x + y * y + z * z);
        if (len > 0.00001f) {
            return Vector3(x / len, y / len, z / len);
        }
        return Vector3(0, 0, 0);
    }
};

static Vector3 centralDiffNormal(const std::vector<float>& heightMap, int width, int height, int i, int j, float verticalScale) {
    int ix0 = std::max(1, std::min(width - 2, i));
    int jy0 = std::max(1, std::min(height - 2, j));
    auto idx = [width](int x, int y) { return y * width + x; };

    float dzdx = ((heightMap[idx(ix0 + 1, jy0)] - heightMap[idx(ix0 - 1, jy0)]) * 0.5f) * verticalScale;
    float dzdy = ((heightMap[idx(ix0, jy0 + 1)] - heightMap[idx(ix0, jy0 - 1)]) * 0.5f) * verticalScale;

    return Vector3(-dzdx, 1.0f, -dzdy).normalize();
}

static Vector3 surfaceNormal(const std::vector<float>& heightMap, int width, int height, int i, int j, float verticalScale) {
    if (i <= 0 || i >= width - 1 || j <= 0 || j >= height - 1) {
        return centralDiffNormal(heightMap, width, height, i, j, verticalScale);
    }

    auto idx = [width](int x, int y) { return y * width + x; };

    float h00 = heightMap[idx(i, j)];

    // Cardinals
    float hpx = heightMap[idx(i + 1, j)];
    float hnx = heightMap[idx(i - 1, j)];
    float hpy = heightMap[idx(i, j + 1)];
    float hny = heightMap[idx(i, j - 1)];
    // Diagonals
    float hpp = heightMap[idx(i + 1, j + 1)];
    float hpn = heightMap[idx(i + 1, j - 1)];
    float hnp = heightMap[idx(i - 1, j + 1)];
    float hnn = heightMap[idx(i - 1, j - 1)];

    const float cardinalWeight = 0.15f;
    const float diagonalWeight = 0.10f;
    const float root2 = 1.41421356f;

    Vector3 n(0, 0, 0);

    n.add(Vector3(verticalScale * (h00 - hpx), 1.0f, 0.0f).normalize().multiplyScalar(cardinalWeight));
    n.add(Vector3(verticalScale * (hnx - h00), 1.0f, 0.0f).normalize().multiplyScalar(cardinalWeight));
    n.add(Vector3(0.0f, 1.0f, verticalScale * (h00 - hpy)).normalize().multiplyScalar(cardinalWeight));
    n.add(Vector3(0.0f, 1.0f, verticalScale * (hny - h00)).normalize().multiplyScalar(cardinalWeight));

    n.add(Vector3(verticalScale * (h00 - hpp) / root2, root2, verticalScale * (h00 - hpp) / root2).normalize().multiplyScalar(diagonalWeight));
    n.add(Vector3(verticalScale * (h00 - hpn) / root2, root2, verticalScale * (h00 - hpn) / root2).normalize().multiplyScalar(diagonalWeight));
    n.add(Vector3(verticalScale * (h00 - hnp) / root2, root2, verticalScale * (h00 - hnp) / root2).normalize().multiplyScalar(diagonalWeight));
    n.add(Vector3(verticalScale * (h00 - hnn) / root2, root2, verticalScale * (h00 - hnn) / root2).normalize().multiplyScalar(diagonalWeight));

    return n;
}

Simulator::Simulator(int w, int h)
    : width(w), height(h), scenario("standard"), step_count(0), seed(1337) {
    grid.resize(width * height);
    dat.resize(width * height, nullptr);
    pool.reserve(2000000);
}

Simulator::~Simulator() {}

void Simulator::loadsoil(const std::string& file) {
    std::ifstream in(file, std::ios::in);
    if (!in.is_open()) {
        std::cout << "Warning: Failed to open soil profile " << file << ". Using default fallback." << std::endl;
        soilmap.clear();
        soils.clear();
        layers.clear();
        
        SurfParam air; air.name = "Air"; air.porosity = 1.0f; air.friction = 0.1f;
        soilmap["Air"] = 0; soils.push_back(air);

        SurfParam rock; rock.name = "Rock"; rock.density = 0.95f; rock.solubility = 1.0f; rock.equrate = 0.1f; rock.friction = 0.15f; rock.maxdiff = 0.01f; rock.settling = 0.1f;
        soilmap["Rock"] = 1; soils.push_back(rock);
        
        SurfLayer r_layer(1); r_layer.bias = 0.5f; r_layer.scale = 0.8f; r_layer.octaves = 8.0f; r_layer.lacunarity = 2.0f; r_layer.gain = 0.5f; r_layer.frequency = 1.0f;
        layers.push_back(r_layer);
        return;
    }

    std::string line;
    int linenr = 0;

    auto syntaxerr = [&]() {
        std::cout << "Error: Incorrect Syntax in Line " << linenr << std::endl;
        exit(0);
    };

    auto hexcol = [&](std::string h) {
        if (h.size() < 6) syntaxerr();
        const std::string allowed = "0123456789ABCDEFabcdef";
        for (char c : h) {
            if (allowed.find(c) == std::string::npos) syntaxerr();
        }
        unsigned int R = std::stoul(h.substr(0, 2), nullptr, 16);
        unsigned int G = std::stoul(h.substr(2, 2), nullptr, 16);
        unsigned int B = std::stoul(h.substr(4, 2), nullptr, 16);
        Color4 col;
        col.r = R / 255.0f;
        col.g = G / 255.0f;
        col.b = B / 255.0f;
        col.a = 1.0f;
        return col;
    };

    SurfParam param;
    bool open = false;
    std::string soillayer;

    soilmap.clear();
    soils.clear();
    layers.clear();

    // Default Air/Water layer at index 0
    SurfParam air_param;
    air_param.name = "Air";
    air_param.porosity = 1.0f;
    air_param.friction = 0.1f;
    soilmap["Air"] = 0;
    soils.push_back(air_param);

    while (std::getline(in, line)) {
        linenr++;
        size_t found = line.find('#');
        if (found != std::string::npos) {
            line = line.substr(0, found);
        }
        while (!line.empty() && std::isspace(line.front())) line.erase(line.begin());
        while (!line.empty() && std::isspace(line.back())) line.pop_back();

        if (line.empty()) continue;

        if (line == "}") {
            if (!open) syntaxerr();
            if (soillayer == "SOIL") {
                std::cout << "Adding Soil Type " << param.name << std::endl;
                soils[soilmap[param.name]] = param;
            }
            open = false;
            continue;
        }

        found = line.find(' ');
        if (found == std::string::npos) syntaxerr();

        std::string tag = line.substr(0, found);
        std::string val = line.substr(found + 1);
        while (!val.empty() && std::isspace(val.front())) val.erase(val.begin());

        if (tag == "SOIL") {
            found = val.find('{');
            if (found == std::string::npos) syntaxerr();
            std::string name = val.substr(0, found);
            while (!name.empty() && std::isspace(name.back())) name.pop_back();
            param = SurfParam();
            param.name = name;
            if (!soilmap.count(name)) {
                soilmap[name] = soils.size();
                soils.push_back(param);
            }
            soillayer = tag;
            open = true;
            continue;
        }

        if (tag == "LAYER") {
            found = val.find('{');
            if (found == std::string::npos) syntaxerr();
            std::string name = val.substr(0, found);
            while (!name.empty() && std::isspace(name.back())) name.pop_back();
            if (!soilmap.count(name)) {
                std::cout << "Can't find SOIL " << name << std::endl;
                syntaxerr();
            }
            layers.emplace_back(soilmap[name]);
            soillayer = tag;
            open = true;
            continue;
        }

        if (tag == "WORLD") {
            found = val.find('{');
            if (found == std::string::npos) syntaxerr();
            soillayer = tag;
            open = true;
            continue;
        }

        if (soillayer == "SOIL") {
            if (tag == "TRANSPORTS") {
                if (!soilmap.count(val)) {
                    soilmap[val] = soils.size();
                    soils.push_back(param);
                }
                param.transports = soilmap[val];
            } else if (tag == "ERODES") {
                if (!soilmap.count(val)) {
                    soilmap[val] = soils.size();
                    soils.push_back(param);
                }
                param.erodes = soilmap[val];
            } else if (tag == "CASCADES") {
                if (!soilmap.count(val)) {
                    soilmap[val] = soils.size();
                    soils.push_back(param);
                }
                param.cascades = soilmap[val];
            } else if (tag == "ABRADES") {
                if (!soilmap.count(val)) {
                    soilmap[val] = soils.size();
                    soils.push_back(param);
                }
                param.abrades = soilmap[val];
            } else if (tag == "DENSITY") {
                param.density = std::stof(val);
            } else if (tag == "POROSITY") {
                param.porosity = std::stof(val);
            } else if (tag == "COLOR") {
                param.color = hexcol(val);
            } else if (tag == "SOLUBILITY") {
                param.solubility = std::stof(val);
            } else if (tag == "EQUILIBRIUM") {
                param.equrate = std::stof(val);
            } else if (tag == "FRICTION") {
                param.friction = std::stof(val);
            } else if (tag == "EROSIONRATE") {
                param.erosionrate = std::stof(val);
            } else if (tag == "MAXDIFF") {
                param.maxdiff = std::stof(val);
            } else if (tag == "SETTLING") {
                param.settling = std::stof(val);
            } else if (tag == "SUSPENSION") {
                param.suspension = std::stof(val);
            } else if (tag == "ABRASION") {
                param.abrasion = std::stof(val);
            } else if (tag == "Ka") {
                param.phong.r = std::stof(val);
            } else if (tag == "Kd") {
                param.phong.g = std::stof(val);
            } else if (tag == "Ks") {
                param.phong.b = std::stof(val);
            } else if (tag == "Kk") {
                param.phong.a = std::stof(val);
            }
        } else if (soillayer == "LAYER") {
            if (tag == "MIN") {
                layers.back().min = std::stof(val);
            } else if (tag == "BIAS") {
                layers.back().bias = std::stof(val);
            } else if (tag == "SCALE") {
                layers.back().scale = std::stof(val);
            } else if (tag == "OCTAVES") {
                layers.back().octaves = std::stof(val);
            } else if (tag == "LACUNARITY") {
                layers.back().lacunarity = std::stof(val);
            } else if (tag == "GAIN") {
                layers.back().gain = std::stof(val);
            } else if (tag == "FREQUENCY") {
                layers.back().frequency = std::stof(val);
            }
        } else if (soillayer == "WORLD") {
            if (tag == "SIZEX") {
                SIZEX = std::stoi(val);
            } else if (tag == "SIZEY") {
                SIZEY = std::stoi(val);
            } else if (tag == "SCALE") {
                SCALE = std::stoi(val);
            } else if (tag == "NWIND") {
                NWIND = std::stoi(val);
            } else if (tag == "NWATER") {
                NWATER = std::stoi(val);
            } else if (tag == "SPRING_X") {
                SPRING_X = std::stoi(val);
            } else if (tag == "SPRING_Y") {
                SPRING_Y = std::stoi(val);
            } else if (tag == "DRAIN_MARGINS") {
                DRAIN_MARGINS = std::stoi(val);
            } else if (tag == "BASE_SPILL") {
                BASE_SPILL = std::stoi(val);
            }
        }
    }
    in.close();
}

void Simulator::add_layer(int x, int y, sec* E) {
    if (E == nullptr) return;
    if (E->size <= 0.0) {
        pool.unget(E);
        return;
    }
    int idx = y * width + x;
    if (dat[idx] == nullptr) {
        dat[idx] = E;
        E->floor = 0.0;
        return;
    }
    if (dat[idx]->type == E->type) {
        dat[idx]->size += E->size;
        pool.unget(E);
        return;
    }
    if (dat[idx]->type == 0) { // Air / Water table is index 0
        sec* top = dat[idx];
        dat[idx] = top->prev;
        if (dat[idx]) dat[idx]->next = nullptr;
        add_layer(x, y, E);
        sec* new_top = dat[idx];
        new_top->next = top;
        top->prev = new_top;
        dat[idx] = top;
        
        // Recalculate floors
        double f = 0.0;
        sec* curr = top;
        while (curr->prev != nullptr) curr = curr->prev;
        while (curr != nullptr) {
            curr->floor = f;
            f += curr->size;
            curr = curr->next;
        }
        return;
    }
    dat[idx]->next = E;
    E->prev = dat[idx];
    E->floor = dat[idx]->floor + dat[idx]->size;
    dat[idx] = E;
}

double Simulator::remove_layer(int x, int y, double h) {
    int idx = y * width + x;
    if (dat[idx] == nullptr) return 0.0;
    if (dat[idx]->size <= 0.0) {
        sec* E = dat[idx];
        dat[idx] = E->prev;
        if (dat[idx]) dat[idx]->next = nullptr;
        pool.unget(E);
        return 0.0;
    }
    if (h <= 0.0) return 0.0;
    double diff = h - dat[idx]->size;
    dat[idx]->size -= h;
    if (diff >= 0.0) {
        sec* E = dat[idx];
        dat[idx] = E->prev;
        if (dat[idx]) dat[idx]->next = nullptr;
        pool.unget(E);
        
        double ret = remove_layer(x, y, diff);
        
        sec* curr = dat[idx];
        if (curr) {
            while (curr->prev != nullptr) curr = curr->prev;
            double f = 0.0;
            while (curr != nullptr) {
                curr->floor = f;
                f += curr->size;
                curr = curr->next;
            }
        }
        return ret;
    }
    return 0.0;
}

double Simulator::get_height(int x, int y) const {
    int idx = y * width + x;
    if (dat[idx] == nullptr) return 0.0;
    return dat[idx]->floor + dat[idx]->size;
}

int Simulator::get_surface_type(int x, int y) const {
    int idx = y * width + x;
    if (dat[idx] == nullptr) return 0;
    return dat[idx]->type;
}

void Simulator::cascade_scree(int x, int y, int transferloop) {
    static const int dx_n[] = {-1, 1, 0, 0, -1, -1, 1, 1};
    static const int dy_n[] = {0, 0, -1, 1, -1, 1, -1, 1};

    double h_curr = get_height(x, y);

    int best_nx = -1;
    int best_ny = -1;
    double max_diff = 0.0;

    for (int d = 0; d < 8; ++d) {
        int nx = x + dx_n[d];
        int ny = y + dy_n[d];
        if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
        double h_neigh = get_height(nx, ny);
        double diff = h_curr - h_neigh;
        if (diff > max_diff) {
            max_diff = diff;
            best_nx = nx;
            best_ny = ny;
        }
    }

    if (best_nx == -1) return;

    int type = get_surface_type(x, y);
    const SurfParam& param = soils[type];
    
    double excess = max_diff - param.maxdiff;
    if (excess <= 0.0) return;

    double transfer = param.settling * excess / 2.0;
    sec* top = dat[y * width + x];
    if (top == nullptr) return;

    if (transfer > top->size) {
        transfer = top->size;
    }

    if (transfer > 0.0) {
        remove_layer(x, y, transfer);
        add_layer(best_nx, best_ny, pool.get(transfer, param.cascades));
        
        if (transferloop > 0) {
            cascade_scree(best_nx, best_ny, transferloop - 1);
        }
    }
}

void Simulator::seep_water(int x, int y) {
    int idx = y * width + x;
    sec* top = dat[idx];
    if (top == nullptr) return;
    
    while (top != nullptr && top->prev != nullptr) {
        sec* prev = top->prev;
        const SurfParam& param = soils[top->type];
        const SurfParam& nparam = soils[prev->type];

        double vol = (top->type == 0) ? top->size : (top->size * top->saturation * param.porosity);
        double nvol = prev->size * prev->saturation * nparam.porosity;
        double nevol = prev->size * (1.0 - prev->saturation) * nparam.porosity;

        double seepage = 0.5;
        double transfer = std::min(vol, nevol);
        
        if (transfer > 0.0) {
            if (top->type == 0) { // Air / Water
                remove_layer(x, y, seepage * transfer);
            } else {
                top->saturation -= (seepage * transfer) / (top->size * param.porosity + 1e-9);
                if (top->saturation < 0.0) top->saturation = 0.0;
            }
            prev->saturation += (seepage * transfer) / (prev->size * nparam.porosity + 1e-9);
            if (prev->saturation > 1.0) prev->saturation = 1.0;
        }
        top = prev;
    }
}

void Simulator::cascade_water(int x, int y, int spill) {
    if (spill <= 0) return;

    int cell_idx = y * width + x;
    sec* top = dat[cell_idx];
    if (top == nullptr || top->type != 0) return;

    // Drain water at the grid margins (boundaries) to prevent bathtub flooding (if configured in profile)
    if (DRAIN_MARGINS) {
        if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
            remove_layer(x, y, top->size);
            return;
        }
    }

    static const int dx_n[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int dy_n[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    double h_curr = get_height(x, y);

    int best_nx = -1;
    int best_ny = -1;
    double max_diff = 0.0;

    for (int d = 0; d < 8; ++d) {
        int nx = x + dx_n[d];
        int ny = y + dy_n[d];
        if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
        
        double h_neigh = get_height(nx, ny);
        double diff = h_curr - h_neigh;
        if (diff > max_diff) {
            max_diff = diff;
            best_nx = nx;
            best_ny = ny;
        }
    }

    if (best_nx == -1) return;

    double transfer = max_diff / 2.0;
    if (transfer > top->size) {
        transfer = top->size;
    }

    if (transfer > 0.01) {
        remove_layer(x, y, transfer);
        add_layer(best_nx, best_ny, pool.get(transfer, 0));
        
        cascade_water(best_nx, best_ny, spill - 1);
    }
}

void Simulator::sync_grid_from_layermap() {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            Cell& cell = grid[idx];
            cell.bedrock = 0.0f;
            cell.soil_clay = 0.0f;
            cell.soil_gravel = 0.0f;
            cell.water = 0.0f;
            cell.groundwater = 0.0f;

            sec* curr = dat[idx];
            if (curr == nullptr) continue;
            while (curr->prev != nullptr) {
                curr = curr->prev;
            }
            while (curr != nullptr) {
                if (curr->type == 0) { // Air / Water table
                    cell.water += curr->size;
                } else if (curr->type == 1) { // Rock / Bedrock
                    cell.bedrock += curr->size;
                } else { // All other layers mapped to soil
                    if (soils[curr->type].name.find("Gravel") != std::string::npos ||
                        soils[curr->type].name.find("Sand") != std::string::npos) {
                        cell.soil_gravel += curr->size;
                    } else {
                        cell.soil_clay += curr->size;
                    }
                    cell.groundwater += curr->size * curr->saturation * soils[curr->type].porosity;
                }
                curr = curr->next;
            }
            cell.soil = cell.soil_clay + cell.soil_gravel;
        }
    }
}

void Simulator::initialize(unsigned int seed_val) {
    seed = seed_val;
    rng.seed(seed);
    step_count = 0;

    std::string soil_file = (scenario == "standard") ? "hills.soil" : (scenario + ".soil");
    loadsoil(soil_file);

    for (size_t l = 0; l < layers.size(); ++l) {
        layers[l].init();
    }

    pool.reset();
    std::fill(dat.begin(), dat.end(), nullptr);
    
    width = SIZEX;
    height = SIZEY;
    grid.resize(width * height);
    dat.resize(width * height, nullptr);

    water_frequency.assign(width * height, 0.0f);
    water_track.assign(width * height, 0.0f);
    wind_frequency.assign(width * height, 0.0f);
    forest_density.assign(width * height, 0.0f);

    const int MAXSEED = 10000;
    
    for (size_t l = 0; l < layers.size(); ++l) {
        const float f = (float)l / (float)layers.size();
        const int Z = seed + f * MAXSEED;
        
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                float nx = (float)i / (float)width;
                float ny = (float)j / (float)height;
                float nz = (float)(Z % MAXSEED);
                
                // Warp coordinates to bend parallel ridges/valleys organically
                float warp_x = 0.05f * (float)noiseGen.noise((double)i * 0.08, (double)j * 0.08);
                float warp_y = 0.05f * (float)noiseGen.noise((double)i * 0.08 + 100.0, (double)j * 0.08 + 100.0);

                double h = layers[l].get(nx + warp_x, ny + warp_y, nz);
                // Add small-scale high-frequency noise to break up parallel structures right at Step 0
                float h_perturb = 0.015f * (float)noiseGen.noise((double)i * 0.2, (double)j * 0.2);
                h = std::max(0.0, h + h_perturb);

                add_layer(i, j, pool.get(h, layers[l].type));
            }
        }
    }

    sync_grid_from_layermap();
}

struct Simulator::WaterParticle {
    Vector2 pos;
    Vector2 speed = Vector2(0, 0);
    double volume = 1.0;
    double sediment = 0.0;
    const double minvol = 0.01;
    double evaprate = 0.01;
    int spill = 3;
    int contains = 0;

    WaterParticle(int w, int h, const std::vector<SurfParam>& soils, const std::vector<sec*>& dat) {
        pos.x = rand() % w;
        pos.y = rand() % h;
        int ipx = std::round(pos.x);
        int ipy = std::round(pos.y);
        ipx = std::clamp(ipx, 0, w - 1);
        ipy = std::clamp(ipy, 0, h - 1);
        sec* top = dat[ipy * w + ipx];
        int surface = top ? top->type : 0;
        contains = soils[surface].transports;
    }
};

struct Simulator::WindParticle {
    Vector2 pos;
    Vector3 speed = Vector3(-2.0f, 0.0f, 1.0f);
    double sediment = 0.0;
    double height = 0.0;
    double sheight = 0.0;
    int contains = 0;

    WindParticle(int w, int h, const std::vector<SurfParam>& soils, const std::vector<sec*>& dat) {
        pos.x = rand() % w;
        pos.y = rand() % h;
        int ipx = std::round(pos.x);
        int ipy = std::round(pos.y);
        ipx = std::clamp(ipx, 0, w - 1);
        ipy = std::clamp(ipy, 0, h - 1);
        sec* top = dat[ipy * w + ipx];
        int surface = top ? top->type : 0;
        contains = soils[surface].transports;
    }
};

bool Simulator::water_move(WaterParticle& p, const std::vector<float>& w_freq) {
    int w = width;
    int h = height;
    int ipx = std::round(p.pos.x);
    int ipy = std::round(p.pos.y);
    ipx = std::clamp(ipx, 0, w - 1);
    ipy = std::clamp(ipy, 0, h - 1);

    float h_curr = get_height(ipx, ipy);
    float h_left = get_height(std::max(0, ipx - 1), ipy);
    float h_right = get_height(std::min(w - 1, ipx + 1), ipy);
    float h_up = get_height(ipx, std::max(0, ipy - 1));
    float h_down = get_height(ipx, std::min(h - 1, ipy + 1));
    
    Vector3 n(-(h_right - h_left) * 0.5f, 1.0f, -(h_down - h_up) * 0.5f);
    n = n.normalize();

    int surface = get_surface_type(ipx, ipy);
    SurfParam param = soils[surface];

    float freq = w_freq[ipy * w + ipx];
    param.friction = param.friction * (1.0f - freq);
    p.evaprate = 0.01f * (1.0f - 0.2f * freq);

    Vector2 slope_direction(n.x, n.z);
    if (std::sqrt(slope_direction.x * slope_direction.x + slope_direction.y * slope_direction.y) * param.friction < 1e-5) {
        return false;
    }

    p.speed.x = (1.0f - param.friction) * p.speed.x + param.friction * slope_direction.x;
    p.speed.y = (1.0f - param.friction) * p.speed.y + param.friction * slope_direction.y;
    
    float len = std::sqrt(p.speed.x * p.speed.x + p.speed.y * p.speed.y);
    if (len > 1e-5f) {
        p.speed.x = std::sqrt(2.0f) * p.speed.x / len;
        p.speed.y = std::sqrt(2.0f) * p.speed.y / len;
    }

    p.pos.x += p.speed.x;
    p.pos.y += p.speed.y;

    if (p.pos.x < 0 || p.pos.x >= w - 1 || p.pos.y < 0 || p.pos.y >= h - 1) {
        int clip_x = std::clamp((int)std::round(p.pos.x), 0, w - 1);
        int clip_y = std::clamp((int)std::round(p.pos.y), 0, h - 1);
        if (p.volume > p.minvol && p.spill > 0) {
            add_layer(clip_x, clip_y, pool.get(p.volume * 0.015, 0)); // Deposit water
            seep_water(clip_x, clip_y);
            cascade_water(clip_x, clip_y, p.spill);
        }
        p.volume = 0.0; // Discard remaining volume and sediment
        return false;
    }

    return true;
}

bool Simulator::water_interact(WaterParticle& p) {
    int w = width;
    int h = height;
    int ipx = std::round(p.pos.x);
    int ipy = std::round(p.pos.y);
    ipx = std::clamp(ipx, 0, w - 1);
    ipy = std::clamp(ipy, 0, h - 1);

    int surface = get_surface_type(ipx, ipy);
    SurfParam param = soils[surface];

    float h_curr = get_height(ipx, ipy);
    float h_pos = get_height(std::floor(p.pos.x), std::floor(p.pos.y));

    // Calculate physical slope: vertical change (m) divided by step distance (m)
    double height_diff_m = (h_curr - h_pos) * VERTICAL_UNIT_M;
    double step_dist_m = std::sqrt(2.0f) * GRID_CELL_SIZE_M;
    double physical_slope = height_diff_m / step_dist_m;

    // Compensate solubility to maintain erosion rate invariant of physical dimensions
    double solubility_compensation = step_dist_m / VERTICAL_UNIT_M;
    // Apply dynamic forest protection to reduce soil susceptibility to erosion
    double solubility = param.solubility * get_soil_solubility_modifier(ipx, ipy);
    double c_eq = solubility * physical_slope * solubility_compensation;
    c_eq = std::clamp(c_eq, 0.0, 1.0);

    double cdiff = c_eq - p.sediment;
    if (cdiff > 0.0) {
        p.sediment += param.equrate * cdiff;
        p.contains = soils[surface].transports;
        
        double diff = remove_layer(ipx, ipy, param.equrate * cdiff * p.volume);
        while (std::abs(diff) > 1e-8) {
            diff = remove_layer(ipx, ipy, diff);
        }
    } else if (cdiff < 0.0) {
        p.sediment += soils[p.contains].equrate * cdiff;
        add_layer(ipx, ipy, pool.get(-soils[p.contains].equrate * cdiff * p.volume, p.contains));
    }

    cascade_scree(ipx, ipy, 0);

    p.sediment /= (1.0 - p.evaprate);
    if (p.sediment > 1.0) p.sediment = 1.0;
    p.volume *= (1.0 - p.evaprate);

    return (p.volume > p.minvol);
}

bool Simulator::wind_move(WindParticle& p) {
    if (soils[p.contains].suspension == 0.0) return false;
    
    int w = width;
    int h = height;
    int ipx = std::round(p.pos.x);
    int ipy = std::round(p.pos.y);
    ipx = std::clamp(ipx, 0, w - 1);
    ipy = std::clamp(ipy, 0, h - 1);

    float h_curr = get_height(ipx, ipy);
    float h_left = get_height(std::max(0, ipx - 1), ipy);
    float h_right = get_height(std::min(w - 1, ipx + 1), ipy);
    float h_up = get_height(ipx, std::max(0, ipy - 1));
    float h_down = get_height(ipx, std::min(h - 1, ipy + 1));
    
    Vector3 n(-(h_right - h_left) * 0.5f, 1.0f, -(h_down - h_up) * 0.5f);
    n = n.normalize();

    p.sheight = h_curr * (float)SCALE / 80.0f;
    if (p.height < p.sheight) {
        p.height = p.sheight;
    }

    const float gravity = 0.25f;
    const float winddominance = 0.2f;
    const float windfriction = 0.8f;

    if (p.height > p.sheight) {
        p.speed.y -= gravity;
    } else {
        float dot_prod = p.speed.x * n.x + p.speed.y * n.y + p.speed.z * n.z;
        Vector3 tangent(p.speed.x - dot_prod * n.x, p.speed.y - dot_prod * n.y, p.speed.z - dot_prod * n.z);
        p.speed.x = (1.0f - windfriction) * p.speed.x + windfriction * tangent.x;
        p.speed.y = (1.0f - windfriction) * p.speed.y + windfriction * tangent.y;
        p.speed.z = (1.0f - windfriction) * p.speed.z + windfriction * tangent.z;
    }

    const Vector3 pspeed(-2.0f, 0.0f, 1.0f);
    p.speed.x = (1.0f - winddominance) * p.speed.x + winddominance * pspeed.x;
    p.speed.y = (1.0f - winddominance) * p.speed.y + winddominance * pspeed.y;
    p.speed.z = (1.0f - winddominance) * p.speed.z + winddominance * pspeed.z;

    p.pos.x += p.speed.x;
    p.pos.y += p.speed.z;
    p.height += p.speed.y;

    if (p.pos.x < 0 || p.pos.x >= w - 1 || p.pos.y < 0 || p.pos.y >= h - 1) {
        return false;
    }

    float speed_len = std::sqrt(p.speed.x * p.speed.x + p.speed.y * p.speed.y + p.speed.z * p.speed.z);
    if (speed_len < 0.01f) return false;

    return true;
}

bool Simulator::wind_interact(WindParticle& p) {
    int w = width;
    int h = height;
    int ipx = std::round(p.pos.x);
    int ipy = std::round(p.pos.y);
    ipx = std::clamp(ipx, 0, w - 1);
    ipy = std::clamp(ipy, 0, h - 1);

    int surface = get_surface_type(ipx, ipy);
    const SurfParam& param = soils[surface];

    if (p.height <= get_height(ipx, ipy) * (float)SCALE / 80.0f) {
        if (param.transports == p.contains) {
            float speed_len = std::sqrt(p.speed.x * p.speed.x + p.speed.y * p.speed.y + p.speed.z * p.speed.z);
            double force = speed_len * (get_height(ipx, ipy) - p.height) * (float)SCALE / 80.0f * (1.0f - p.sediment);
            double diff = remove_layer(ipx, ipy, param.suspension * force);
            p.sediment += (param.suspension * force - diff);
            
            cascade_scree(ipx, ipy, 1);
        }
    } else if (param.suspension > 0.0f) {
        p.sediment -= soils[p.contains].suspension * p.sediment;
        
        add_layer(ipx, ipy, pool.get(0.5f * soils[p.contains].suspension * p.sediment, p.contains));
        add_layer(ipx, ipy, pool.get(0.5f * soils[p.contains].suspension * p.sediment, p.contains));
        
        cascade_scree(ipx, ipy, 1);
    }
    return true;
}

void Simulator::step(float dt) {
    step_count++;
    (void)dt;

    for (int i = 0; i < width * height; ++i) {
        water_track[i] = 0.0f;
    }

    for (int d = 0; d < NWATER; ++d) {
        WaterParticle p(width, height, soils, dat);
        p.spill = BASE_SPILL;
        if (SPRING_X >= 0 && SPRING_Y >= 0) {
            // Pressure feedback on generation: spawn spring at lowest hydraulic head in 5x5 neighborhood around (SPRING_X, SPRING_Y)
            int cx = std::clamp(SPRING_X, 0, width - 1);
            int cy = std::clamp(SPRING_Y, 0, height - 1);
            int spawn_x = cx;
            int spawn_y = cy;
            double min_head = grid[cy * width + cx].hydraulic_head();
            for (int dy_off = -2; dy_off <= 2; ++dy_off) {
                for (int dx_off = -2; dx_off <= 2; ++dx_off) {
                    int nx = cx + dx_off;
                    int ny = cy + dy_off;
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        double head = grid[ny * width + nx].hydraulic_head();
                        if (head < min_head) {
                            min_head = head;
                            spawn_x = nx;
                            spawn_y = ny;
                        }
                    }
                }
            }
            p.pos.x = spawn_x;
            p.pos.y = spawn_y;
            int ipx = spawn_x;
            int ipy = spawn_y;
            sec* top = dat[ipy * width + ipx];
            int surface = top ? top->type : 0;
            p.contains = soils[surface].transports;
        }
        while (true) {
            int ipx = std::round(p.pos.x);
            int ipy = std::round(p.pos.y);
            ipx = std::clamp(ipx, 0, width - 1);
            ipy = std::clamp(ipy, 0, height - 1);

            // Calculate local slope magnitude to determine adaptive sub-stepping
            float h_left = get_height(std::max(0, ipx - 1), ipy);
            float h_right = get_height(std::min(width - 1, ipx + 1), ipy);
            float h_up = get_height(ipx, std::max(0, ipy - 1));
            float h_down = get_height(ipx, std::min(height - 1, ipy + 1));

            float dx = -(h_right - h_left) * 0.5f;
            float dy = -(h_down - h_up) * 0.5f;
            float len3D = std::sqrt(dx * dx + 1.0f + dy * dy);
            float slope_mag = std::sqrt(dx * dx + dy * dy) / len3D;

            // Run 1 to 5 sub-steps depending on the steepness of the slope
            int substeps = 1 + (int)std::round(4.0f * slope_mag);
            bool terminated = false;

            for (int s = 0; s < substeps; ++s) {
                int cipx = std::round(p.pos.x);
                int cipy = std::round(p.pos.y);
                cipx = std::clamp(cipx, 0, width - 1);
                cipy = std::clamp(cipy, 0, height - 1);

                water_track[cipy * width + cipx] += p.volume / (float)substeps;

                if (!water_move(p, water_frequency)) { terminated = true; break; }
                if (!water_interact(p)) { terminated = true; break; }
            }

            if (terminated) break;
        }
        
        int ipx = std::round(p.pos.x);
        int ipy = std::round(p.pos.y);
        ipx = std::clamp(ipx, 0, width - 1);
        ipy = std::clamp(ipy, 0, height - 1);
        
        if (p.volume > p.minvol && p.spill > 0) {
            add_layer(ipx, ipy, pool.get(p.sediment * soils[p.contains].equrate, p.contains));
            cascade_scree(ipx, ipy, 0);

            // Pressure feedback on deposition: find the cell in 3x3 neighborhood with the lowest hydraulic head
            int dep_x = ipx;
            int dep_y = ipy;
            double min_head = grid[ipy * width + ipx].hydraulic_head();
            for (int dy_off = -1; dy_off <= 1; ++dy_off) {
                for (int dx_off = -1; dx_off <= 1; ++dx_off) {
                    int nx = ipx + dx_off;
                    int ny = ipy + dy_off;
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        double head = grid[ny * width + nx].hydraulic_head();
                        if (head < min_head) {
                            min_head = head;
                            dep_x = nx;
                            dep_y = ny;
                        }
                    }
                }
            }

            add_layer(dep_x, dep_y, pool.get(p.volume * 0.015, 0)); // water layer type = 0
            seep_water(dep_x, dep_y);
            cascade_water(dep_x, dep_y, p.spill);
        }
    }

    const float lrate = 0.01f;
    const float K = 50.0f;
    for (int i = 0; i < width * height; ++i) {
        water_frequency[i] = (1.0f - lrate) * water_frequency[i] + lrate * K * water_track[i] / (1.0f + K * water_track[i]);
    }

    for (int d = 0; d < NWIND; ++d) {
        WindParticle p(width, height, soils, dat);
        int wind_steps = 0;
        while (wind_move(p) && wind_interact(p) && wind_steps++ < 100);
    }

    // Dynamic organic soil growth based on plant cover (humus generation)
    grow_forest_soil();
    update_hydrology_and_transpiration();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            seep_water(x, y);
            
            // Calculate local slope to determine dynamic cascade spill depth (drainage speed)
            float h_left = get_height(std::max(0, x - 1), y);
            float h_right = get_height(std::min(width - 1, x + 1), y);
            float h_up = get_height(x, std::max(0, y - 1));
            float h_down = get_height(x, std::min(height - 1, y + 1));

            float dx = -(h_right - h_left) * 0.5f;
            float dy = -(h_down - h_up) * 0.5f;
            float len3D = std::sqrt(dx * dx + 1.0f + dy * dy);
            float slope = std::sqrt(dx * dx + dy * dy) / len3D;

            int dynamic_spill = BASE_SPILL + (int)std::round(12.0f * slope);
            cascade_water(x, y, dynamic_spill);
        }
    }

    sync_grid_from_layermap();
}

bool Simulator::save_layers_csv(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "x,y,bedrock,soil,soil_clay,soil_gravel,water,groundwater,sediment\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            const Cell& cell = grid[idx];
            file << x << "," << y << "," << cell.bedrock << "," << cell.soil << ","
                 << cell.soil_clay << "," << cell.soil_gravel << ","
                 << cell.water << "," << cell.groundwater << "," << cell.sediment << "\n";
        }
    }
    return true;
}

bool Simulator::save_top_view_png(const std::string& filepath) const {
    std::vector<unsigned char> pixels(width * height * 3);
    std::vector<float> flow_acc(width * height, 1.0f);
    std::vector<int> sorted_indices(width * height);
    std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
    std::sort(sorted_indices.begin(), sorted_indices.end(), [this](int a, int b) {
        return grid[a].terrain_height() > grid[b].terrain_height();
    });

    int dx_d8[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy_d8[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    float dist_d8[] = {1.414f, 1.0f, 1.414f, 1.0f, 1.0f, 1.414f, 1.0f, 1.414f};

    for (int idx : sorted_indices) {
        int x = idx % width;
        int y = idx / width;
        float head_curr = grid[idx].terrain_height();
        int best_n = -1;
        float max_s = 0.0f;

        for (int d = 0; d < 8; ++d) {
            int nx = x + dx_d8[d];
            int ny = y + dy_d8[d];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            int nidx = ny * width + nx;
            float head_neigh = grid[nidx].terrain_height();
            if (head_curr > head_neigh) {
                float slope = (head_curr - head_neigh) / dist_d8[d];
                if (slope > max_s) {
                    max_s = slope;
                    best_n = nidx;
                }
            }
        }
        if (best_n != -1) {
            flow_acc[best_n] += flow_acc[idx];
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            const Cell& cell = grid[idx];
            
            unsigned char r_land, g_land, b_land;
            float total_soil = cell.soil_clay + cell.soil_gravel;
            
            if (total_soil <= 0.005f) {
                r_land = 135; g_land = 125; b_land = 125;
            } else {
                float clay_ratio = cell.soil_clay / (total_soil + 1e-6f);
                float gravel_ratio = cell.soil_gravel / (total_soil + 1e-6f);

                float r_clay = 130.0f, g_clay = 75.0f, b_clay = 55.0f;
                float r_gravel = 190.0f, g_gravel = 175.0f, b_gravel = 155.0f;

                float r_sed = clay_ratio * r_clay + gravel_ratio * r_gravel;
                float g_sed = clay_ratio * g_clay + gravel_ratio * g_gravel;
                float b_sed = clay_ratio * b_clay + gravel_ratio * b_gravel;

                float r_grass_base = 75.0f, g_grass_base = 145.0f, b_grass_base = 60.0f;
                if (grows_trees(x, y)) {
                    // Darker pine green for forested areas
                    r_grass_base = 34.0f; g_grass_base = 90.0f; b_grass_base = 40.0f;
                }
                float r_grass = r_grass_base * 0.8f + r_sed * 0.2f;
                float g_grass = g_grass_base * 0.8f + g_sed * 0.2f;
                float b_grass = b_grass_base * 0.8f + b_sed * 0.2f;

                float f_acc = flow_acc[idx];
                if (f_acc > 12.0f || cell.water > 0.02f) {
                    r_land = (unsigned char)std::clamp(r_sed, 0.0f, 255.0f);
                    g_land = (unsigned char)std::clamp(g_sed, 0.0f, 255.0f);
                    b_land = (unsigned char)std::clamp(b_sed, 0.0f, 255.0f);
                } else {
                    r_land = (unsigned char)std::clamp(r_grass, 0.0f, 255.0f);
                    g_land = (unsigned char)std::clamp(g_grass, 0.0f, 255.0f);
                    b_land = (unsigned char)std::clamp(b_grass, 0.0f, 255.0f);
                }
            }

            float wf = water_frequency[idx];
            float r_out = r_land;
            float g_out = g_land;
            float b_out = b_land;

            float water_depth = cell.water;
            float r_water = 69.0f;
            float g_water = 128.0f;
            float b_water = 179.0f;
            float blend = 0.0f;

            // Apply flow accumulation mask to restrict water blending to actual riverbeds
            float f_acc = flow_acc[idx];
            if (f_acc > 12.0f) {
                // Scale water visibility by frequency, with a slight boost
                blend = std::clamp(2.5f * wf, 0.0f, 0.9f);
            }
            
            // Standing water pools are color coded by depth
            if (water_depth > 0.02f) {
                float depth_m = water_depth * VERTICAL_UNIT_M;
                float absorption = 1.0f - std::exp(-depth_m / 2.0f);
                absorption = std::clamp(absorption, 0.10f, 0.95f);
                
                // Blend from shallow turquoise (60, 165, 185) to deep navy (15, 35, 75)
                r_water = (1.0f - absorption) * 60.0f + absorption * 15.0f;
                g_water = (1.0f - absorption) * 165.0f + absorption * 35.0f;
                b_water = (1.0f - absorption) * 185.0f + absorption * 75.0f;
                
                blend = std::max(blend, absorption);
            }

            if (blend > 0.0f) {
                r_out = (1.0f - blend) * r_land + blend * r_water;
                g_out = (1.0f - blend) * g_land + blend * g_water;
                b_out = (1.0f - blend) * b_land + blend * b_water;
            }

            // Apply topographic height rings on dry land surface
            if (water_depth <= 0.02f) {
                float scale_z = VERTICAL_UNIT_M * VERTICAL_EXAGGERATION;
                float h_soil = (cell.bedrock + cell.soil) * scale_z;
                float render_contour_step = (CONTOUR_INTERVAL_M / VERTICAL_UNIT_M) * VERTICAL_EXAGGERATION;
                int c_curr = (int)std::floor(h_soil / render_contour_step);
                
                bool is_contour = false;
                if (x < width - 1) {
                    const Cell& cell_right = grid[y * width + (x + 1)];
                    float h_right = (cell_right.bedrock + cell_right.soil) * scale_z;
                    int c_right = (int)std::floor(h_right / render_contour_step);
                    if (c_curr != c_right) is_contour = true;
                }
                if (y < height - 1) {
                    const Cell& cell_down = grid[(y + 1) * width + x];
                    float h_down = (cell_down.bedrock + cell_down.soil) * scale_z;
                    int c_down = (int)std::floor(h_down / render_contour_step);
                    if (c_curr != c_down) is_contour = true;
                }
                
                if (is_contour) {
                    r_out *= 0.5f;
                    g_out *= 0.5f;
                    b_out *= 0.5f;
                }
            }

            int pixel_idx = (y * width + x) * 3;
            pixels[pixel_idx] = (unsigned char)std::clamp(r_out, 0.0f, 255.0f);
            pixels[pixel_idx + 1] = (unsigned char)std::clamp(g_out, 0.0f, 255.0f);
            pixels[pixel_idx + 2] = (unsigned char)std::clamp(b_out, 0.0f, 255.0f);
        }
    }

    int success = stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
    return success != 0;
}

bool Simulator::save_side_view_png(const std::string& filepath) const {
    int img_w = 512;
    int img_h = 512;
    std::vector<unsigned char> pixels(img_w * img_h * 3, 20);
    
    int slice_y = height * 3 / 4;
    int vy = std::clamp(slice_y, 0, height - 1);
    
    float min_h = 999.0f;
    float max_h = -999.0f;
    for (int px = 0; px < img_w; ++px) {
        int vx = px * width / img_w;
        vx = std::clamp(vx, 0, width - 1);
        const Cell& cell = grid[vy * width + vx];
        float h_water = cell.bedrock + cell.soil_clay + cell.soil_gravel + cell.water;
        min_h = std::min(min_h, cell.bedrock);
        max_h = std::max(max_h, h_water);
    }
    
    float z_min = std::max(0.0f, min_h - 2.0f);
    float z_max = max_h + 2.0f;
    float z_range = std::max(5.0f, z_max - z_min);
    
    for (int py = 0; py < img_h; ++py) {
        float vz_float = z_min + (float)(img_h - 1 - py) * z_range / (float)img_h;
        
        for (int px = 0; px < img_w; ++px) {
            int vx = px * width / img_w;
            vx = std::clamp(vx, 0, width - 1);
            
            const Cell& cell = grid[vy * width + vx];
            float h_bedrock = cell.bedrock;
            float total_soil = cell.soil_clay + cell.soil_gravel;
            float h_soil = h_bedrock + total_soil;
            float water_depth = cell.water;
            
            float dz = z_range / (float)img_h;
            float v_water_top = h_soil + ((water_depth > 0.02f) ? std::max(water_depth, 0.04f) : 0.0f);
            float v_soil_top = h_bedrock + ((total_soil > 0.0f) ? std::max(total_soil, dz) : 0.0f);
            float v_bedrock_top = h_bedrock;
            
            unsigned char r = 25, g = 35, b = 45;
            
            if (vz_float <= v_bedrock_top) {
                r = 135; g = 125; b = 125;
            } else if (vz_float <= v_soil_top) {
                float r_sed = 130.0f, g_sed = 75.0f, b_sed = 55.0f;
                sec* curr_layer = dat[vy * width + vx];
                while (curr_layer != nullptr) {
                    if (curr_layer->type != 0) {
                        int s_idx = curr_layer->type;
                        if (s_idx >= 0 && s_idx < (int)soils.size()) {
                            r_sed = soils[s_idx].color.r * 255.0f;
                            g_sed = soils[s_idx].color.g * 255.0f;
                            b_sed = soils[s_idx].color.b * 255.0f;
                        }
                        break;
                    }
                    curr_layer = curr_layer->next;
                }

                if (vz_float > v_soil_top - dz * 1.5f && water_depth <= 0.0f) {
                    unsigned char r_grass = 75; unsigned char g_grass = 145; unsigned char b_grass = 60;
                    r = 0.3f * r_sed + 0.7f * r_grass;
                    g = 0.3f * g_sed + 0.7f * g_grass;
                    b = 0.3f * b_sed + 0.7f * b_grass;
                } else {
                    r = (unsigned char)std::clamp(r_sed, 0.0f, 255.0f);
                    g = (unsigned char)std::clamp(g_sed, 0.0f, 255.0f);
                    b = (unsigned char)std::clamp(b_sed, 0.0f, 255.0f);
                }
            } else if (vz_float <= v_water_top) {
                r = 0; g = 210; b = 255;
            }
            
            int pix_idx = (py * img_w + px) * 3;
            pixels[pix_idx] = r;
            pixels[pix_idx + 1] = g;
            pixels[pix_idx + 2] = b;
        }
    }
    
    int success = stbi_write_png(filepath.c_str(), img_w, img_h, 3, pixels.data(), img_w * 3);
    return success != 0;
}

bool Simulator::save_iso_view_png(const std::string& filepath) const {
    int img_w = 1024;
    int img_h = 1024;
    std::vector<unsigned char> pixels(img_w * img_h * 3, 20);

    // Compute flow accumulation locally to map river channel beds
    std::vector<float> flow_acc(width * height, 1.0f);
    std::vector<int> sorted_indices(width * height);
    std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
    std::sort(sorted_indices.begin(), sorted_indices.end(), [this](int a, int b) {
        return grid[a].terrain_height() > grid[b].terrain_height();
    });
    int dx_d8[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy_d8[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    float dist_d8[] = {1.414f, 1.0f, 1.414f, 1.0f, 1.0f, 1.414f, 1.0f, 1.414f};
    for (int idx : sorted_indices) {
        int x = idx % width;
        int y = idx / width;
        float head_curr = grid[idx].terrain_height();
        int best_n = -1;
        float max_s = 0.0f;
        for (int d = 0; d < 8; ++d) {
            int nx = x + dx_d8[d];
            int ny = y + dy_d8[d];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            int nidx = ny * width + nx;
            float head_neigh = grid[nidx].terrain_height();
            if (head_curr > head_neigh) {
                float slope = (head_curr - head_neigh) / dist_d8[d];
                if (slope > max_s) {
                    max_s = slope;
                    best_n = nidx;
                }
            }
        }
        if (best_n != -1) {
            flow_acc[best_n] += flow_acc[idx];
        }
    }
    
    float dx_ray = -0.707f;
    float dy_ray = -0.707f;
    float dz_ray = -0.55f;
    float len = std::sqrt(dx_ray * dx_ray + dy_ray * dy_ray + dz_ray * dz_ray);
    dx_ray /= len; dy_ray /= len; dz_ray /= len;
    
    float rx = 0.707f; float ry = -0.707f; float rz = 0.0f;
    float ux = -0.38f; float uy = -0.38f; float uz = 0.85f;
    
    float wx_c = (float)width / 2.0f;
    float wy_c = (float)height / 2.0f;
    
    // Scale all heights by physical vertical unit and exaggeration
    float scale_z = VERTICAL_UNIT_M * VERTICAL_EXAGGERATION;
    
    // Set target height matching scaled terrain center (average normalized height is ~0.5)
    float wz_c = 0.5f * scale_z;
    
    float max_dim = (float)std::max(width, height);
    float scale = 0.3f * (max_dim / 256.0f);
    // Camera distance
    float cam_dist = 200.0f * (max_dim / 256.0f);
    
    // Precompute forest layout and tree canopy heights once per snapshot
    std::vector<float> tree_height_map(width * height, 0.0f);
    std::vector<float> canopy_noise_map(width * height, 0.0f);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            float fd = forest_density[idx];
            tree_height_map[idx] = fd;
            if (fd > 0.1f) {
                float c_noise = (float)noiseGen.noise((double)x * 15.0, (double)y * 15.0);
                canopy_noise_map[idx] = c_noise;
            }
        }
    }

    for (int py = 0; py < img_h; ++py) {
        for (int px = 0; px < img_w; ++px) {
            float ox = wx_c + ((float)px - 512.0f) * rx * scale + (512.0f - (float)py) * ux * scale - cam_dist * dx_ray;
            float oy = wy_c + ((float)px - 512.0f) * ry * scale + (512.0f - (float)py) * uy * scale - cam_dist * dy_ray;
            float oz = wz_c + ((float)px - 512.0f) * rz * scale + (512.0f - (float)py) * uz * scale - cam_dist * dz_ray;
            
            float r = 25.0f, g = 35.0f, b = 45.0f;
            
            float t = 0.0f;
            float t_max = 2.5f * max_dim;
            float dt_step = 0.35f;
            bool was_inside = false;
            
            while (t < t_max) {
                float cx = ox + t * dx_ray;
                float cy = oy + t * dy_ray;
                float cz = oz + t * dz_ray;
                
                int vx = (int)std::floor(cx);
                int vy = (int)std::floor(cy);
                
                if (vx >= 0 && vx < width && vy >= 0 && vy < height) {
                    int cell_idx = vy * width + vx;
                    const Cell& cell = grid[cell_idx];
                    
                    // Bilinearly interpolate bedrock, soil, and water to smooth out pixelation
                    float fx = cx - 0.5f;
                    float fy = cy - 0.5f;
                    int x0 = (int)std::floor(fx);
                    int y0 = (int)std::floor(fy);
                    int x1 = x0 + 1;
                    int y1 = y0 + 1;
                    
                    x0 = std::clamp(x0, 0, width - 1);
                    x1 = std::clamp(x1, 0, width - 1);
                    y0 = std::clamp(y0, 0, height - 1);
                    y1 = std::clamp(y1, 0, height - 1);
                    
                    float tx = fx - (float)x0;
                    float ty = fy - (float)y0;
                    tx = std::clamp(tx, 0.0f, 1.0f);
                    ty = std::clamp(ty, 0.0f, 1.0f);
                    
                    auto get_cell_vals = [&](int x, int y, float& bedrock, float& soil, float& water) {
                        const Cell& c = grid[y * width + x];
                        bedrock = c.bedrock * scale_z;
                        soil = (c.soil_clay + c.soil_gravel) * scale_z;
                        water = c.water * scale_z;
                    };
                    
                    float b00, s00, w00; get_cell_vals(x0, y0, b00, s00, w00);
                    float b10, s10, w10; get_cell_vals(x1, y0, b10, s10, w10);
                    float b01, s01, w01; get_cell_vals(x0, y1, b01, s01, w01);
                    float b11, s11, w11; get_cell_vals(x1, y1, b11, s11, w11);
                    
                    float h_bedrock = (1.0f - tx) * (1.0f - ty) * b00 + tx * (1.0f - ty) * b10 + (1.0f - tx) * ty * b01 + tx * ty * b11;
                    float total_soil = (1.0f - tx) * (1.0f - ty) * s00 + tx * (1.0f - ty) * s10 + (1.0f - tx) * ty * s01 + tx * ty * s11;
                    float h_soil = h_bedrock + total_soil;
                    float water_depth = (1.0f - tx) * (1.0f - ty) * w00 + tx * (1.0f - ty) * w10 + (1.0f - tx) * ty * w01 + tx * ty * w11;
                    
                    // Render standing water pools above a threshold proportional to scale_z
                    float min_water_render_h = 0.003f * scale_z;
                    float h_water = h_soil + ((water_depth > min_water_render_h) ? water_depth : 0.0f);
                    
                    // Modular Forest Canopy rendering height (bilinearly interpolated from precomputed maps)
                    float th00 = tree_height_map[y0 * width + x0];
                    float th10 = tree_height_map[y0 * width + x1];
                    float th01 = tree_height_map[y1 * width + x0];
                    float th11 = tree_height_map[y1 * width + x1];
                    float tree_height = (1.0f - tx) * (1.0f - ty) * th00 + tx * (1.0f - ty) * th10 + (1.0f - tx) * ty * th01 + tx * ty * th11;

                    float cn00 = canopy_noise_map[y0 * width + x0];
                    float cn10 = canopy_noise_map[y0 * width + x1];
                    float cn01 = canopy_noise_map[y1 * width + x0];
                    float cn11 = canopy_noise_map[y1 * width + x1];
                    float canopy_noise = (1.0f - tx) * (1.0f - ty) * cn00 + tx * (1.0f - ty) * cn10 + (1.0f - tx) * ty * cn01 + tx * ty * cn11;

                    float h_vegetation = h_soil; // Draw trees flat like grass to prevent obscuring terrain features
                    float h_surface = std::max(h_water, h_vegetation);
                    
                    bool entered_from_top = (t == 0.0f) || was_inside;
                    was_inside = true;

                    if (cz <= h_surface) {
                        float shade = 0.65f;
                        float water_shade_h = 0.05f * scale_z;
                        if (cz > h_water - water_shade_h) shade = 1.0f;
                        
                        float f_acc = flow_acc[cell_idx];
                        float depth_m = water_depth / scale_z * VERTICAL_UNIT_M;
                        float blend = 0.0f;
                        float min_depth_m = 0.003f * VERTICAL_UNIT_M;
                        float wf = water_frequency[cell_idx];
                        
                        // Draw stream lines using flow history (wf) so the channels are always visible on land
                        if (f_acc > 12.0f && water_depth <= min_water_render_h) {
                            blend = std::clamp(2.5f * wf, 0.0f, 0.9f);
                        }

                        // Color code rivers: sandy-brown if dry, and blue-navy if wet based on water depth
                        float r_river = 180.0f;
                        float g_river = 160.0f;
                        float b_river = 130.0f;
                        if (depth_m > 1e-4f) {
                            float norm_depth = std::clamp(depth_m / min_depth_m, 0.0f, 1.0f);
                            if (norm_depth < 0.5f) {
                                float t_col = norm_depth / 0.5f;
                                r_river = (1.0f - t_col) * 100.0f + t_col * 40.0f;
                                g_river = (1.0f - t_col) * 200.0f + t_col * 100.0f;
                                b_river = (1.0f - t_col) * 240.0f + t_col * 220.0f;
                            } else {
                                float t_col = (norm_depth - 0.5f) / 0.5f;
                                r_river = (1.0f - t_col) * 40.0f + t_col * 10.0f;
                                g_river = (1.0f - t_col) * 100.0f + t_col * 25.0f;
                                b_river = (1.0f - t_col) * 220.0f + t_col * 120.0f;
                            }
                        }


                        bool draw_water = false;
                        bool draw_trees = false;
                        bool draw_soil_grass = false;
                        bool draw_bedrock = false;

                        if (entered_from_top) {
                            // Render top surface layer cover
                            if (water_depth > min_water_render_h) {
                                draw_water = true;
                            } else if (tree_height > 0.0f) {
                                draw_trees = true;
                            } else {
                                draw_soil_grass = true;
                            }
                        } else {
                            // Side cross-sections cuts
                            if (cz <= h_bedrock) {
                                draw_bedrock = true;
                            } else if (cz <= h_soil) {
                                draw_soil_grass = true;
                            } else {
                                draw_water = true;
                            }
                        }

                        // Base sediment and soil colors (look up from the top-most soil layer under water)
                        float total_soil_raw = cell.soil_clay + cell.soil_gravel;
                        float r_sed = 130.0f;
                        float g_sed = 75.0f;
                        float b_sed = 55.0f;

                        sec* curr_layer = dat[cell_idx];
                        while (curr_layer != nullptr) {
                            if (curr_layer->type != 0) { // Skip water
                                int s_idx = curr_layer->type;
                                if (s_idx >= 0 && s_idx < (int)soils.size()) {
                                    r_sed = soils[s_idx].color.r * 255.0f;
                                    g_sed = soils[s_idx].color.g * 255.0f;
                                    b_sed = soils[s_idx].color.b * 255.0f;
                                }
                                break;
                            }
                            curr_layer = curr_layer->next;
                        }

                        float r_grass = 75.0f; float g_grass = 145.0f; float b_grass = 60.0f;
                        float r_grass_final = 0.3f * r_sed + 0.7f * r_grass;
                        float g_grass_final = 0.3f * g_sed + 0.7f * g_grass;
                        float b_grass_final = 0.3f * b_sed + 0.7f * b_grass;

                        bool is_river_bed = (f_acc > 12.0f);

                        if (draw_trees) {
                            // Render rich forest green smoothly blended with grass/sediment based on density
                            float r_forest = 34.0f; float g_forest = 90.0f; float b_forest = 40.0f;
                            float brightness = 0.8f + 0.2f * std::max(0.0f, canopy_noise);
                            float fd = forest_density[cell_idx];

                            float r_bg = is_river_bed ? r_sed : r_grass_final;
                            float g_bg = is_river_bed ? g_sed : g_grass_final;
                            float b_bg = is_river_bed ? b_sed : b_grass_final;

                            float r_base = fd * (r_forest * brightness) + (1.0f - fd) * r_bg;
                            float g_base = fd * (g_forest * brightness) + (1.0f - fd) * g_bg;
                            float b_base = fd * (b_forest * brightness) + (1.0f - fd) * b_bg;

                            r = (1.0f - blend) * r_base + blend * r_river;
                            g = (1.0f - blend) * g_base + blend * g_river;
                            b = (1.0f - blend) * b_base + blend * b_river;
                        }
                        else if (draw_bedrock) {
                            float r_base = 135.0f; float g_base = 125.0f; float b_base = 125.0f;
                            r = (1.0f - blend) * r_base + blend * r_river;
                            g = (1.0f - blend) * g_base + blend * g_river;
                            b = (1.0f - blend) * b_base + blend * b_river;
                        }
                        else if (draw_soil_grass) {
                            float r_base = r_sed;
                            float g_base = g_sed;
                            float b_base = b_sed;

                            float grass_render_h = 0.20f * scale_z;
                            bool should_render_grass = entered_from_top ? !is_river_bed : (cz > h_soil - grass_render_h && !is_river_bed);
                            if (should_render_grass) {
                                r_base = r_grass_final;
                                g_base = g_grass_final;
                                b_base = b_grass_final;
                            }

                            r = (1.0f - blend) * r_base + blend * r_river;
                            g = (1.0f - blend) * g_base + blend * g_river;
                            b = (1.0f - blend) * b_base + blend * b_river;
                        }
                        else if (draw_water) {
                            // Render standing water!
                            float r_bed = r_sed;
                            float g_bed = g_sed;
                            float b_bed = b_sed;
                            if (total_soil_raw <= 0.005f) {
                                r_bed = 135.0f; g_bed = 125.0f; b_bed = 125.0f;
                            }

                            float depth_m = water_depth / scale_z * VERTICAL_UNIT_M;
                            float absorption;
                            if (depth_m < 0.15f) {
                                absorption = (depth_m / 0.15f) * 0.8f; // Transparent shore transition
                            } else {
                                absorption = 1.0f; // Fully opaque outside the shoreline
                            }

                            // Blend from shallow turquoise (60, 165, 185) to deep navy (15, 35, 75)
                            float w_r = (1.0f - absorption) * 60.0f + absorption * 15.0f;
                            float w_g = (1.0f - absorption) * 165.0f + absorption * 35.0f;
                            float w_b = (1.0f - absorption) * 185.0f + absorption * 75.0f;

                            float r_base = (1.0f - absorption) * r_bed + absorption * w_r;
                            float g_base = (1.0f - absorption) * g_bed + absorption * w_g;
                            float b_base = (1.0f - absorption) * b_bed + absorption * w_b;

                            // Add bright red shoreline foam highlight (when water depth < 15cm and near deep water)
                            if (depth_m < 0.15f) {
                                bool near_deep = false;
                                for (int dy_off = -1; dy_off <= 1; ++dy_off) {
                                    for (int dx_off = -1; dx_off <= 1; ++dx_off) {
                                        int nx = vx + dx_off;
                                        int ny = vy + dy_off;
                                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                            if (grid[ny * width + nx].water * VERTICAL_UNIT_M > 0.4f) {
                                                near_deep = true;
                                                break;
                                            }
                                        }
                                    }
                                    if (near_deep) break;
                                }
                                if (near_deep) {
                                    float foam = 1.0f - (depth_m / 0.15f);
                                    r_base = (1.0f - foam) * r_base + foam * 255.0f;
                                    g_base = (1.0f - foam) * g_base + foam * 0.0f;
                                    b_base = (1.0f - foam) * b_base + foam * 0.0f;
                                }
                            }

                            // Apply atmospheric distance fog
                            r = (1.0f - blend) * r_base + blend * 69.0f;
                            g = (1.0f - blend) * g_base + blend * 128.0f;
                            b = (1.0f - blend) * b_base + blend * 179.0f;
                            shade = 0.9f;
                        }

                        // Apply topographic height rings map-wide across the visible surface (terrain or water level)
                        {
                            float render_contour_step = (CONTOUR_INTERVAL_M / VERTICAL_UNIT_M) * VERTICAL_EXAGGERATION;
                            // Use total surface height (ground + water depth) to represent the visible top surface
                            float h_surface = h_soil + water_depth;
                            float norm_h = h_surface / render_contour_step;
                            
                            // Check if height is within 4% of a contour boundary
                            if (std::abs(norm_h - std::round(norm_h)) < 0.04f) {
                                r *= 0.5f;
                                g *= 0.5f;
                                b *= 0.5f;
                            }
                        }
                        
                        r *= shade; g *= shade; b *= shade;
                        break;
                    }
                } else {
                    was_inside = false;
                }
                t += dt_step;
            }
            
            int pix_idx = (py * img_w + px) * 3;
            pixels[pix_idx] = (unsigned char)std::clamp(r, 0.0f, 255.0f);
            pixels[pix_idx + 1] = (unsigned char)std::clamp(g, 0.0f, 255.0f);
            pixels[pix_idx + 2] = (unsigned char)std::clamp(b, 0.0f, 255.0f);
        }
    }
    
    int success = stbi_write_png(filepath.c_str(), img_w, img_h, 3, pixels.data(), img_w * 3);
    return success != 0;
}

bool Simulator::save_velocity_png(const std::string& filepath) const {
    std::vector<unsigned char> pixels(width * height * 3);
    
    std::vector<float> heightMap(width * height);
    for (int i = 0; i < width * height; ++i) {
        heightMap[i] = grid[i].terrain_height();
    }
    
    // 3x3 Box filter to smooth chaotic vector directions
    std::vector<float> smooth_vx(width * height, 0.0f);
    std::vector<float> smooth_vy(width * height, 0.0f);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            float sum_vx = 0.0f;
            float sum_vy = 0.0f;
            float count = 0.0f;
            for (int ky = -1; ky <= 1; ++ky) {
                int ny = y + ky;
                if (ny >= 0 && ny < height) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        int nx = x + kx;
                        if (nx >= 0 && nx < width) {
                            int nidx = ny * width + nx;
                            sum_vx += grid[nidx].vx;
                            sum_vy += grid[nidx].vy;
                            count += 1.0f;
                        }
                    }
                }
            }
            smooth_vx[idx] = sum_vx / count;
            smooth_vy[idx] = sum_vy / count;
        }
    }
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            const Cell& cell = grid[idx];
            
            // Slate hillshade for ground
            Vector3 normal = surfaceNormal(heightMap, width, height, x, y, 1.0f);
            float shade = 0.3f + 0.7f * normal.y;
            float r_g = shade * 50.0f;
            float g_g = shade * 55.0f;
            float b_g = shade * 65.0f;
            
            float r = r_g, g = g_g, b = b_g;
            
            float water_depth = cell.water;
            // Only render velocity where there is significant water depth (matching thresholds)
            if (water_depth > 0.05f) {
                float vx = smooth_vx[idx];
                float vy = smooth_vy[idx];
                float speed = std::sqrt(vx * vx + vy * vy);
                
                // Max expected speed for normalization is around 1.2m/s
                float t = std::clamp(speed / 1.2f, 0.0f, 1.0f);
                
                // Sleek thermal flow color mapping: Indigo -> Cyan -> Neon Green -> Neon Orange/Red
                float r_w, g_w, b_w;
                if (t < 0.2f) {
                    float f = t / 0.2f;
                    r_w = (1.0f - f) * 20.0f + f * 79.0f;
                    g_w = (1.0f - f) * 30.0f + f * 70.0f;
                    b_w = (1.0f - f) * 120.0f + f * 229.0f;
                } else if (t < 0.5f) {
                    float f = (t - 0.2f) / 0.3f;
                    r_w = (1.0f - f) * 79.0f + f * 6.0f;
                    g_w = (1.0f - f) * 70.0f + f * 182.0f;
                    b_w = (1.0f - f) * 212.0f + f * 212.0f;
                } else if (t < 0.8f) {
                    float f = (t - 0.5f) / 0.3f;
                    r_w = (1.0f - f) * 6.0f + f * 34.0f;
                    g_w = (1.0f - f) * 182.0f + f * 197.0f;
                    b_w = (1.0f - f) * 212.0f + f * 94.0f;
                } else {
                    float f = (t - 0.8f) / 0.2f;
                    r_w = (1.0f - f) * 34.0f + f * 239.0f;
                    g_w = (1.0f - f) * 197.0f + f * 68.0f;
                    b_w = (1.0f - f) * 94.0f + f * 68.0f;
                }
                
                // Blend water speed color onto terrain base
                float alpha = 0.85f;
                r = (1.0f - alpha) * r_g + alpha * r_w;
                g = (1.0f - alpha) * g_g + alpha * g_w;
                b = (1.0f - alpha) * b_g + alpha * b_w;
            }
            
            int pixel_idx = idx * 3;
            pixels[pixel_idx] = (unsigned char)std::clamp(r, 0.0f, 255.0f);
            pixels[pixel_idx + 1] = (unsigned char)std::clamp(g, 0.0f, 255.0f);
            pixels[pixel_idx + 2] = (unsigned char)std::clamp(b, 0.0f, 255.0f);
        }
    }
    
    int success = stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
    return success != 0;
}

void Simulator::print_diagnostics() const {
    double total_bedrock = 0.0;
    double total_soil = 0.0;
    double total_water = 0.0;
    double total_gw = 0.0;
    double max_water = 0.0;
    double max_wf = 0.0;
    double max_elev = -999.0;
    double min_elev = 999.0;
    int water_cells = 0;

    for (int i = 0; i < width * height; ++i) {
        const Cell& cell = grid[i];
        total_bedrock += cell.bedrock;
        total_soil += cell.soil;
        total_water += cell.water;
        total_gw += cell.groundwater;
        
        double elev = cell.bedrock + cell.soil;
        if (elev > max_elev) max_elev = elev;
        if (elev < min_elev) min_elev = elev;

        if (cell.water > max_water) max_water = cell.water;
        if (cell.water > 0.02) water_cells++;
        if (water_frequency[i] > max_wf) max_wf = water_frequency[i];
    }

    int n = width * height;
    std::cout << "[Diagnostics Step " << step_count << "]\n"
              << "  Avg Bedrock: " << ((total_bedrock / n) * VERTICAL_UNIT_M) << " m\n"
              << "  Avg Soil:    " << ((total_soil / n) * VERTICAL_UNIT_M) << " m\n"
              << "  Min Elevation: " << (min_elev * VERTICAL_UNIT_M) << " m\n"
              << "  Max Elevation: " << (max_elev * VERTICAL_UNIT_M) << " m\n"
              << "  Avg Water:   " << ((total_water / n) * VERTICAL_UNIT_M) << " m\n"
              << "  Max Water:   " << (max_water * VERTICAL_UNIT_M) << " m\n"
              << "  Avg Groundwater: " << ((total_gw / n) * VERTICAL_UNIT_M) << " m\n"
              << "  Water Cells: " << water_cells << " / " << n << "\n"
              << "  Max Water Freq: " << max_wf << "\n";
}

bool Simulator::grows_trees(int x, int y) const {
    int idx = y * width + x;
    return forest_density[idx] > 0.1f;
}

int Simulator::get_neighbor_tree_count(int x, int y) const {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                if (grows_trees(nx, ny)) {
                    count++;
                }
            }
        }
    }
    return count;
}

float Simulator::get_soil_solubility_modifier(int x, int y) const {
    if (!grows_trees(x, y)) return 1.0f;
    int n_trees = get_neighbor_tree_count(x, y);
    float factor = 1.0f - 0.1f * (float)n_trees;
    return std::clamp(factor, 0.2f, 1.0f);
}

void Simulator::grow_forest_soil() {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (grows_trees(x, y)) {
                int n_trees = get_neighbor_tree_count(x, y);
                Cell& cell = grid[y * width + x];
                float soil_m = cell.soil * VERTICAL_UNIT_M;
                // Extremely slow, realistic soil/humus growth: max 25 cm over 500 years (1000 steps)
                float growth_m = 0.00005f * std::max(0.0f, 10.0f - soil_m) * (1.0f + 0.25f * n_trees);
                // Convert back from physical growth (meters) to coordinate units (size value)
                float growth_coord = growth_m / VERTICAL_UNIT_M;
                if (growth_coord > 0.0f) {
                    add_layer(x, y, pool.get(growth_coord, 2)); // Type 2 is Red Sand / Soil
                }
            }
        }
    }
}

void Simulator::update_hydrology_and_transpiration() {
    std::vector<double> curr_sat(width * height, 0.0);
    std::vector<sec*> soil_layer_ptr(width * height, nullptr);

    // 1. Identify top soil layers and retrieve current saturation
    for (int i = 0; i < width * height; ++i) {
        sec* curr = dat[i];
        while (curr != nullptr) {
            if (curr->type > 1) { // Type > 1 represents soil layers (not Air=0 or Rock=1)
                curr_sat[i] = curr->saturation;
                soil_layer_ptr[i] = curr;
                break; // only track top-most soil layer for diffusion
            }
            curr = curr->prev; // Walk down
        }
    }

    // 2. Apply background rain infiltration & vegetation transpiration drawdowns
    const double rain_infilt = 0.008;
    // Transpiration is realistically budgeted as ~35% of rainfall infiltration
    const double transpiration_drawdown = 0.0028;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (soil_layer_ptr[idx] == nullptr) continue;

            const Cell& cell = grid[idx];
            float wf = water_frequency[idx];
            float soil_m = cell.soil * VERTICAL_UNIT_M;
            float groundwater_m = cell.groundwater * VERTICAL_UNIT_M;
            float water_m = cell.water * VERTICAL_UNIT_M;

            // Check if moisture and soil conditions are ideal for forest growth
            bool ideal = (soil_m > 0.8f && wf < 0.45f && water_m <= 0.4f && groundwater_m > 0.02f);
            float target_density = ideal ? 1.0f : 0.0f;
            float diff = target_density - forest_density[idx];
            if (diff > 0.0f) {
                forest_density[idx] += 0.05f * diff; // Growth/colonization delay
            } else {
                forest_density[idx] += 0.02f * diff; // Wilting/dieback delay
            }

            double sat = curr_sat[idx];
            // Background rainfall infiltration (everywhere with soil)
            sat = std::min(1.0, sat + rain_infilt);

            // Transpiration drawdown scaled by forest density
            double trans = forest_density[idx] * transpiration_drawdown;
            sat = std::max(0.0, sat - trans);
            
            curr_sat[idx] = sat;
        }
    }

    // 3. Diffuse soil moisture laterally (groundwater percolation & bank storage seepage)
    std::vector<double> diffused_sat = curr_sat;
    const double k_diff = 0.15; // lateral diffusion coefficient
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (soil_layer_ptr[idx] == nullptr) continue;

            double sum = 0.0;
            double count = 0.0;
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    int nidx = ny * width + nx;
                    if (soil_layer_ptr[nidx] != nullptr) {
                        sum += curr_sat[nidx];
                        count += 1.0;
                    }
                }
            }
            if (count > 0.0) {
                diffused_sat[idx] = (1.0 - k_diff) * curr_sat[idx] + k_diff * (sum / count);
            }
        }
    }

    // 4. Write back new saturation values to layermap
    for (int i = 0; i < width * height; ++i) {
        if (soil_layer_ptr[i] != nullptr) {
            soil_layer_ptr[i]->saturation = diffused_sat[i];
        }
    }
}

} // namespace sim_rivers
} // namespace jotcad
