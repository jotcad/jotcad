#include "simulator.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

void print_help() {
    std::cout << "JotCAD Focused River Simulator CLI\n"
              << "Usage: sim_rivers [options]\n"
              << "Options:\n"
              << "  --width <w>     Grid width (default: 128)\n"
              << "  --height <h>    Grid height (default: 128)\n"
              << "  --steps <s>     Simulation iterations (default: 200)\n"
              << "  --dt <val>      Time step duration (default: 0.5)\n"
              << "  --seed <val>    Noise seed (default: 1337)\n"
              << "  --scenario <sc> Active scenario: standard, valley_test (default: standard)\n"
              << "  --delay <ms>    Sleep duration per step in milliseconds (default: 0)\n"
              << "  --out-data <f>  Output CSV filename (default: layers.csv)\n"
              << "  --out-image <f> Output PNG filename (default: terrain.png)\n"
              << "  --help          Show this message\n";
}

int main(int argc, char** argv) {
    int width = 256;
    int height = 256;
    int steps = 200;
    float dt = 0.5f;
    unsigned int seed = 1337;
    std::string scenario = "standard";
    std::string out_data = "layers.csv";
    std::string out_image = "terrain.png";
    int delay_ms = 0;

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
        } else if (arg == "--scenario" && i + 1 < argc) {
            scenario = argv[++i];
        } else if (arg == "--out-data" && i + 1 < argc) {
            out_data = argv[++i];
        } else if (arg == "--out-image" && i + 1 < argc) {
            out_image = argv[++i];
        } else if (arg == "--delay" && i + 1 < argc) {
            delay_ms = std::stoi(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_help();
            return 1;
        }
    }

    std::cout << "Starting River Simulator [Scenario: " << scenario 
              << ", Grid: " << width << "x" << height 
              << ", Steps: " << steps << ", dt: " << dt 
              << ", Seed: " << seed << ", Delay: " << delay_ms << "ms]...\n";
              
    auto start_time = std::chrono::high_resolution_clock::now();
    
    jotcad::sim_rivers::Simulator simulator(width, height);
    simulator.set_scenario(scenario);
    simulator.initialize(seed);

    // Determine side and iso filenames beforehand
    std::string out_side_image = "";
    std::string out_iso_image = "";
    size_t pos = out_image.find("terrain.png");
    if (pos != std::string::npos) {
        out_side_image = out_image;
        out_side_image.replace(pos, 11, "terrain_side.png");
        
        out_iso_image = out_image;
        out_iso_image.replace(pos, 11, "terrain_iso.png");
    } else {
        size_t dot = out_image.find_last_of('.');
        if (dot != std::string::npos) {
            out_side_image = out_image.substr(0, dot) + "_side.png";
            out_iso_image = out_image.substr(0, dot) + "_iso.png";
        } else {
            out_side_image = out_image + "_side.png";
            out_iso_image = out_image + "_iso.png";
        }
    }

    // Resolve output directory
    size_t last_slash = out_image.find_last_of("/\\");
    std::string base_dir = (last_slash != std::string::npos) ? out_image.substr(0, last_slash + 1) : "";

    // Save Step 0 (initial state)
    std::cout << "Saving Step 0 initial snapshots...\n";
    simulator.save_top_view_png(base_dir + "terrain_0.png");
    simulator.save_side_view_png(base_dir + "terrain_side_0.png");
    simulator.save_iso_view_png(base_dir + "terrain_iso_0.png");
    simulator.save_velocity_png(base_dir + "terrain_velocity_0.png");

    for (int step = 0; step < steps; ++step) {
        simulator.step(dt);
        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
        int s = step + 1;
        if (s % 100 == 0 || s == steps || s == 1) {
            std::cout << "  [Step " << s << "/" << steps << "] completed. Saving step snapshots...\n";
            simulator.print_diagnostics();
            std::string s_str = std::to_string(s);
            simulator.save_top_view_png(base_dir + "terrain_" + s_str + ".png");
            simulator.save_side_view_png(base_dir + "terrain_side_" + s_str + ".png");
            simulator.save_iso_view_png(base_dir + "terrain_iso_" + s_str + ".png");
            simulator.save_velocity_png(base_dir + "terrain_velocity_" + s_str + ".png");
        }
    }

    // Also write final step to the generic base filenames
    std::cout << "Saving final step snapshots to base filenames...\n";
    simulator.save_top_view_png(out_image);
    simulator.save_side_view_png(out_side_image);
    simulator.save_iso_view_png(out_iso_image);
    
    std::string out_vel_image = out_image;
    size_t pos_v = out_image.find("terrain.png");
    if (pos_v != std::string::npos) {
        out_vel_image.replace(pos_v, 11, "terrain_velocity.png");
    } else {
        size_t dot = out_image.find_last_of('.');
        if (dot != std::string::npos) {
            out_vel_image = out_image.substr(0, dot) + "_velocity.png";
        } else {
            out_vel_image = out_image + "_velocity.png";
        }
    }
    simulator.save_velocity_png(out_vel_image);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "Simulation core finished in " << duration.count() << " seconds.\n";

    std::cout << "Saving data to " << out_data << "...\n";
    if (!simulator.save_layers_csv(out_data)) {
        std::cerr << "Error: Failed to save " << out_data << "\n";
        return 1;
    }

    std::cout << "SUCCESS: River simulation output generated.\n";
    return 0;
}
