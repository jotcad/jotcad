#include "simulator.h"
#include <iostream>
#include <string>
#include <chrono>

void print_help() {
    std::cout << "JotCAD Physical Landscape Simulator CLI\n"
              << "Usage: sim [options]\n"
              << "Options:\n"
              << "  --width <w>     Grid width (default: 256)\n"
              << "  --height <h>    Grid height (default: 256)\n"
              << "  --steps <s>     Simulation iterations (default: 100)\n"
              << "  --dt <val>      Time step duration (default: 0.2)\n"
              << "  --seed <val>    Perlin noise seed (default: 1337)\n"
              << "  --out-mesh <f>  Output OBJ filename (default: terrain.obj)\n"
              << "  --out-data <f>  Output CSV filename (default: layers.csv)\n"
              << "  --out-image <f> Output 2D top-down PNG filename (default: terrain.png)\n"
              << "  --out-side-image <f> Output side-view PNG filename (default: terrain_side.png)\n"
              << "  --out-iso-image <f> Output 3D isometric PNG filename (default: terrain_iso.png)\n"
              << "  --cutaway       Enable cutaway on the 3D isometric image\n"
              << "  --help          Show this message\n";
}

int main(int argc, char** argv) {
    int width = 512;
    int height = 512;
    int steps = 100;
    float dt = 0.2f;
    unsigned int seed = 1337;
    std::string out_mesh = "terrain.obj";
    std::string out_data = "layers.csv";
    std::string out_image = "terrain.png";
    std::string out_side_image = "";
    std::string out_iso_image = "";
    bool cutaway = false;
    std::string scenario = "standard";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        } else if (arg == "--width" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (arg == "--steps" && i + 1 < argc) {
            steps = std::stoi(argv[++i]);
        } else if (arg == "--dt" && i + 1 < argc) {
            dt = std::stof(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoul(argv[++i]);
        } else if (arg == "--out-mesh" && i + 1 < argc) {
            out_mesh = argv[++i];
        } else if (arg == "--out-data" && i + 1 < argc) {
            out_data = argv[++i];
        } else if (arg == "--out-image" && i + 1 < argc) {
            out_image = argv[++i];
        } else if (arg == "--out-side-image" && i + 1 < argc) {
            out_side_image = argv[++i];
        } else if (arg == "--out-iso-image" && i + 1 < argc) {
            out_iso_image = argv[++i];
        } else if (arg == "--cutaway") {
            cutaway = true;
        } else if (arg == "--scenario" && i + 1 < argc) {
            scenario = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_help();
            return 1;
        }
    }

    std::cout << "Initializing simulator with grid: " << width << "x" << height 
              << ", seed: " << seed << ", scenario: " << scenario << "...\n";
              
    auto start_time = std::chrono::high_resolution_clock::now();
    
    jotcad::sim::Simulator simulator(width, height);
    simulator.set_scenario(scenario);
    simulator.initialize(seed);

    std::cout << "Running " << steps << " simulation iterations (dt=" << dt << ")...\n";
    for (int step = 0; step < steps; ++step) {
        simulator.step(dt);
        if ((step + 1) % 10 == 0 || step == steps - 1) {
            std::cout << "  [Step " << (step + 1) << "/" << steps << "] completed.\n";
        }
    }

    std::cout << "Simulating road infrastructure and settlement growth...\n";
    simulator.run_settlement_simulation();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "Simulation completed in " << duration.count() << " seconds.\n";

    std::cout << "Saving 3D mesh model to " << out_mesh << "...\n";
    if (!simulator.save_to_obj(out_mesh)) {
        std::cerr << "Error saving OBJ mesh file.\n";
        return 1;
    }

    std::cout << "Saving layer data to " << out_data << "...\n";
    if (!simulator.save_layers_csv(out_data)) {
        std::cerr << "Error saving CSV data file.\n";
        return 1;
    }

    if (out_side_image.empty()) {
        size_t dot_pos = out_image.find_last_of('.');
        if (dot_pos != std::string::npos) {
            out_side_image = out_image.substr(0, dot_pos) + "_side.png";
        } else {
            out_side_image = out_image + "_side.png";
        }
    }
    if (out_iso_image.empty()) {
        size_t dot_pos = out_image.find_last_of('.');
        if (dot_pos != std::string::npos) {
            out_iso_image = out_image.substr(0, dot_pos) + "_iso.png";
        } else {
            out_iso_image = out_image + "_iso.png";
        }
    }

    std::cout << "Saving landscape top-view visualization to " << out_image << "...\n";
    if (!simulator.save_top_view_png(out_image)) {
        std::cerr << "Error saving top-view PNG visualization file.\n";
        return 1;
    }

    std::cout << "Saving landscape side-view cross-section to " << out_side_image << "...\n";
    if (!simulator.save_side_view_png(out_side_image)) {
        std::cerr << "Error saving side-view PNG visualization file.\n";
        return 1;
    }

    std::cout << "Saving landscape 3D isometric visualization to " << out_iso_image << "...\n";
    if (!simulator.save_to_png(out_iso_image, cutaway)) {
        std::cerr << "Error saving 3D isometric PNG visualization file.\n";
        return 1;
    }

    std::string out_arability = "arability.png";
    std::cout << "Saving crop arability map to " << out_arability << "...\n";
    if (!simulator.save_arability_png(out_arability)) {
        std::cerr << "Error saving arability PNG file.\n";
        return 1;
    }

    std::cout << "SUCCESS: Landscape simulation files generated.\n";
    return 0;
}
