#include "test_base.h"
#include "../pack/engine.h"
#include "../ops/hexagon_op.h"
#include "../ops/box_op.h"
#include <iostream>
#include <cassert>

using namespace jotcad::geo;
using namespace libnest2d;

void test_engine_pack_hexagons() {
    std::cout << "--- test_engine_pack_hexagons (Matching Repro) ---" << std::endl;
    MockVFS vfs("engine_test");
    hexagon_init(&vfs);
    box_init(&vfs);

    // 1. Create Hexagon(edgeToEdge=247)
    fs::Selector hex_sel("jot/Hexagon/edgeToEdge");
    hex_sel.parameters["edgeToEdge"] = 247.0;
    hex_sel.output = "$out";
    Processor::execute(&vfs, hex_sel);
    Shape hex = vfs.read<Shape>(hex_sel);

    std::vector<Shape> parts = { hex, hex };
    
    // 2. Create Sheet Box(600, 300)
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 600.0;
    box_sel.parameters["height"] = 300.0;
    box_sel.output = "$out";
    Processor::execute(&vfs, box_sel);
    Shape sheet = vfs.read<Shape>(box_sel);
    std::vector<Shape> sheets = { sheet };

    // 3. Pack
    pack::Engine::Config config;
    config.spacing = 2.0;
    config.margin = 5.0;

    pack::PackResult res = pack::Engine::pack(&vfs, parts, sheets, config);

    std::cout << "  Placements: " << res.placements.size() << std::endl;
    for (size_t i = 0; i < res.placements.size(); ++i) {
        const auto& p = res.placements[i];
        std::cout << "  Part " << p.part_index << " translation: " 
                  << CGAL::to_double(p.tf.t.hm(0,3)) << ", " 
                  << CGAL::to_double(p.tf.t.hm(1,3)) << std::endl;
    }

    if (res.placements.size() < 2) {
        std::cout << "  FAILURE: Only " << res.placements.size() << " parts placed!" << std::endl;
    } else {
        double dx = std::abs(CGAL::to_double(res.placements[0].tf.t.hm(0,3)) - CGAL::to_double(res.placements[1].tf.t.hm(0,3)));
        double dy = std::abs(CGAL::to_double(res.placements[0].tf.t.hm(1,3)) - CGAL::to_double(res.placements[1].tf.t.hm(1,3)));
        std::cout << "  Distance: dx=" << dx << ", dy=" << dy << std::endl;
        if (dx < 1.0 && dy < 1.0) {
            std::cout << "  FAILURE: Engine overlapped parts!" << std::endl;
        } else {
            std::cout << "  SUCCESS: Engine placed parts correctly." << std::endl;
        }
    }
}

void test_raw_libnest2d_rects() {
    std::cout << "--- test_raw_libnest2d_rects ---" << std::endl;
    
    using Item = libnest2d::Item;
    using Box = libnest2d::Box;
    const long long SCALE = 1000000;
    
    std::vector<Item> items;
    items.push_back(Item({ {0, 0}, {100*SCALE, 0}, {100*SCALE, 100*SCALE}, {0, 100*SCALE} }));
    items.push_back(Item({ {0, 0}, {100*SCALE, 0}, {100*SCALE, 100*SCALE}, {0, 100*SCALE} }));

    Box bin(500*SCALE, 500*SCALE);
    BottomLeftPlacer::Config bl_cfg;
    bl_cfg.allow_rotations = false;
    _Nester<BottomLeftPlacer, FirstFitSelection> nester(bin, 2*SCALE, bl_cfg);
    std::vector<std::reference_wrapper<Item>> batch = { std::ref(items[0]), std::ref(items[1]) };
    nester.execute(batch.begin(), batch.end());

    for (size_t i = 0; i < items.size(); ++i) {
        auto tr = items[i].translation();
        std::cout << "  Item " << i << " translation: " << getX(tr)/double(SCALE) << ", " << getY(tr)/double(SCALE) << std::endl;
    }
}

void test_raw_libnest2d_hexagons() {
    std::cout << "--- test_raw_libnest2d_hexagons ---" << std::endl;
    const long long SCALE = 1000000;
    auto create_hex = [&](double r) {
        ClipperLib::Path path;
        for (int i = 0; i < 6; ++i) {
            double angle = i * M_PI / 3.0;
            path.push_back(ClipperLib::IntPoint(
                static_cast<ClipperLib::cInt>(r * std::cos(angle) * SCALE),
                static_cast<ClipperLib::cInt>(r * std::sin(angle) * SCALE)
            ));
        }
        if (ClipperLib::Orientation(path)) ClipperLib::ReversePath(path);
        return libnest2d::Item(path);
    };
    std::vector<Item> items = { create_hex(50.0), create_hex(50.0) };
    Box bin(500*SCALE, 500*SCALE);
    BottomLeftPlacer::Config bl_cfg;
    bl_cfg.allow_rotations = true;
    _Nester<BottomLeftPlacer, FirstFitSelection> nester(bin, 10*SCALE, bl_cfg);
    std::vector<std::reference_wrapper<Item>> batch = { std::ref(items[0]), std::ref(items[1]) };
    nester.execute(batch.begin(), batch.end());

    for (size_t i = 0; i < items.size(); ++i) {
        auto tr = items[i].translation();
        std::cout << "  Item " << i << " translation: " << getX(tr)/double(SCALE) << ", " << getY(tr)/double(SCALE) << std::endl;
    }
}

void test_clipper_orientation() {
    std::cout << "--- test_clipper_orientation ---" << std::endl;
    ClipperLib::Path ccw = { {0,0}, {100,0}, {0,100} };
    ClipperLib::Path cw = { {0,0}, {0,100}, {100,0} };
    std::cout << "  CCW (0,0)->(100,0)->(0,100) Orientation: " << (ClipperLib::Orientation(ccw) ? "TRUE" : "FALSE") << std::endl;
    std::cout << "  CW  (0,0)->(0,100)->(100,0) Orientation: " << (ClipperLib::Orientation(cw) ? "TRUE" : "FALSE") << std::endl;
}

int main() {
    test_clipper_orientation();
    test_engine_pack_hexagons();
    test_raw_libnest2d_rects();
    test_raw_libnest2d_hexagons();
    return 0;
}
