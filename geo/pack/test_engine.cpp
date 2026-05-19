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

int main() {
    try {
        test_geometric_align();
        test_geometric_exact_fit();
        test_geometric_islands();
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
