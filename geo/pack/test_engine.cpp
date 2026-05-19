#include <iostream>
#include <cassert>
#include "packaide_engine.h"

using namespace pack;

void test_geometric_align() {
    std::cout << "Testing Pure Geometric Alignment..." << std::endl;

    // 1. Create a 10x10 square at origin
    packaide::Polygon_2 box;
    box.push_back(packaide::Point_2(0, 0));
    box.push_back(packaide::Point_2(10, 0));
    box.push_back(packaide::Point_2(10, 10));
    box.push_back(packaide::Point_2(0, 10));
    
    assert(packaide::is_good_polygon(box));

    PackaideEngine::PartInfo info;
    info.original_index = 0;
    info.cgal_poly = packaide::Polygon_with_holes_2(box);
    info.area = packaide::FT(100.0);
    info.xmin_off = packaide::FT(0);
    info.ymin_off = packaide::FT(0);

    std::vector<PackaideEngine::PartInfo> parts = { info };

    // 2. 50x50 sheet
    packaide::Sheet sheet = packaide::Sheet::rectangle(packaide::FT(50.0), packaide::FT(50.0));
    
    PackaideEngine::Config config;
    config.margin = packaide::FT(0.0);
    config.spacing = packaide::FT(0.0);

    // 3. Pack
    auto placements = PackaideEngine::pack_geometric(parts, sheet, config);

    // 4. Verify
    assert(placements.size() == 1);
    std::cout << "  - Placement: (" << placements[0].x << ", " << placements[0].y << ")" << std::endl;
    
    // Bottom-Left bias means it should be at (0,0)
    assert(placements[0].x == packaide::FT(0));
    assert(placements[0].y == packaide::FT(0));
}

void test_geometric_exact_fit() {
    std::cout << "Testing Geometric Exact Fit (Point Placement)..." << std::endl;

    // 1. Create a 10x10 square
    packaide::Polygon_2 box;
    box.push_back(packaide::Point_2(0, 0));
    box.push_back(packaide::Point_2(10, 0));
    box.push_back(packaide::Point_2(10, 10));
    box.push_back(packaide::Point_2(0, 10));
    
    PackaideEngine::PartInfo info;
    info.original_index = 0;
    info.cgal_poly = packaide::Polygon_with_holes_2(box);
    info.area = packaide::FT(100.0);
    info.xmin_off = packaide::FT(0);
    info.ymin_off = packaide::FT(0);

    std::vector<PackaideEngine::PartInfo> parts = { info };

    // 2. 10x10 sheet (EXACT FIT)
    packaide::Sheet sheet = packaide::Sheet::rectangle(packaide::FT(10.0), packaide::FT(10.0));
    
    PackaideEngine::Config config;
    config.margin = packaide::FT(0.0);
    config.spacing = packaide::FT(0.0);

    // 3. Pack
    auto placements = PackaideEngine::pack_geometric(parts, sheet, config);

    // 4. Verify
    assert(placements.size() == 1);
    std::cout << "  - Placement: (" << placements[0].x << ", " << placements[0].y << ")" << std::endl;
    assert(placements[0].x == packaide::FT(0));
    assert(placements[0].y == packaide::FT(0));
}

void test_geometric_islands() {
    std::cout << "Testing Material Islands (Disconnected Scraps)..." << std::endl;

    // 1. Create two disconnected 10x10 squares as material
    packaide::Polygon_2 scrap1;
    scrap1.push_back(packaide::Point_2(0, 0));
    scrap1.push_back(packaide::Point_2(10, 0));
    scrap1.push_back(packaide::Point_2(10, 10));
    scrap1.push_back(packaide::Point_2(0, 10));

    packaide::Polygon_2 scrap2;
    scrap2.push_back(packaide::Point_2(20, 0));
    scrap2.push_back(packaide::Point_2(30, 0));
    scrap2.push_back(packaide::Point_2(30, 10));
    scrap2.push_back(packaide::Point_2(20, 10));

    packaide::Sheet sheet;
    sheet.material.insert(scrap1);
    sheet.material.insert(scrap2);

    // 2. Create two 5x5 parts
    packaide::Polygon_2 box;
    box.push_back(packaide::Point_2(0, 0));
    box.push_back(packaide::Point_2(5, 0));
    box.push_back(packaide::Point_2(5, 5));
    box.push_back(packaide::Point_2(0, 5));
    
    PackaideEngine::PartInfo info;
    info.original_index = 0;
    info.cgal_poly = packaide::Polygon_with_holes_2(box);
    info.area = packaide::FT(25.0);
    info.xmin_off = packaide::FT(0);
    info.ymin_off = packaide::FT(0);

    std::vector<PackaideEngine::PartInfo> parts = { info, info };

    PackaideEngine::Config config;
    config.margin = packaide::FT(0.0);
    config.spacing = packaide::FT(1.0);

    // 3. Pack
    auto placements = PackaideEngine::pack_geometric(parts, sheet, config);

    // 4. Verify
    std::cout << "  - Placed " << placements.size() << " parts across islands" << std::endl;
    assert(placements.size() == 2);
    
    for (const auto& p : placements) {
        std::cout << "    * Part at (" << p.x << ", " << p.y << ")" << std::endl;
    }
}

void test_geometric_nest_in_hole() {
    std::cout << "Testing Nesting Inside Part Holes..." << std::endl;

    // 1. 80x80 Sheet (Exactly matches Part A size)
    packaide::Sheet sheet = packaide::Sheet::rectangle(packaide::FT(80.0), packaide::FT(80.0));

    // 2. Part A: 80x80 Square with a 40x40 Hole
    packaide::Polygon_2 outer;
    outer.push_back(packaide::Point_2(0, 0));
    outer.push_back(packaide::Point_2(80, 0));
    outer.push_back(packaide::Point_2(80, 80));
    outer.push_back(packaide::Point_2(0, 80));

    packaide::Polygon_2 hole;
    hole.push_back(packaide::Point_2(20, 20));
    hole.push_back(packaide::Point_2(20, 60));
    hole.push_back(packaide::Point_2(60, 60));
    hole.push_back(packaide::Point_2(60, 20));

    packaide::Polygon_with_holes_2 pwh_A(outer);
    pwh_A.add_hole(hole);

    PackaideEngine::PartInfo info_A;
    info_A.original_index = 0;
    info_A.cgal_poly = pwh_A;
    info_A.area = packaide::FT(80*80 - 40*40);
    info_A.xmin_off = 0;
    info_A.ymin_off = 0;

    // 3. Part B: 20x20 Solid Square
    packaide::Polygon_2 p_B;
    p_B.push_back(packaide::Point_2(0, 0));
    p_B.push_back(packaide::Point_2(20, 0));
    p_B.push_back(packaide::Point_2(20, 20));
    p_B.push_back(packaide::Point_2(0, 20));

    PackaideEngine::PartInfo info_B;
    info_B.original_index = 1;
    info_B.cgal_poly = packaide::Polygon_with_holes_2(p_B);
    info_B.area = packaide::FT(400.0);
    info_B.xmin_off = 0;
    info_B.ymin_off = 0;

    std::vector<PackaideEngine::PartInfo> parts = { info_A, info_B };

    PackaideEngine::Config config;
    config.margin = packaide::FT(0.0);
    config.spacing = packaide::FT(0.0);

    // 4. Pack
    auto placements = PackaideEngine::pack_geometric(parts, sheet, config);

    // 5. Verify
    std::cout << "  - Placed " << placements.size() << " parts" << std::endl;
    assert(placements.size() == 2);
    
    // Part B (index 1) should be placed at (20, 20) inside Part A's hole
    bool found_in_hole = false;
    for (const auto& p : placements) {
        if (p.original_index == 1) {
            std::cout << "    * Part B placed at (" << p.x << ", " << p.y << ")" << std::endl;
            if (p.x == packaide::FT(20) && p.y == packaide::FT(20)) {
                found_in_hole = true;
            }
        }
    }
    assert(found_in_hole);
    std::cout << "  - SUCCESS: Part B nested inside Part A's void." << std::endl;
}

int main() {
    try {
        test_geometric_align();
        test_geometric_exact_fit();
        test_geometric_islands();
        test_geometric_nest_in_hole();
        std::cout << "✨ Pure Geometric Engine Test Passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown error" << std::endl;
        return 1;
    }
    return 0;
}
