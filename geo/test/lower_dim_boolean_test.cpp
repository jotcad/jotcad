#include "test_base.h"
#include <fstream>

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("lower_dim_boolean");
    register_all_ops(&vfs);
    
    auto save_png = [&](const Selector& addr, const std::string& filename) {
        Selector png_addr = Selector{"jot/png", {{"$in", addr}}}.with_output("$out");
        Processor::execute(&vfs, png_addr);
        std::vector<uint8_t> bytes = vfs.read<std::vector<uint8_t>>(png_addr);
        std::ofstream out("actual/" + filename, std::ios::binary);
        out.write((char*)bytes.data(), bytes.size());
    };

    // --- 1. POINTS VS POINTS ---
    std::cout << "--- Points vs Points ---" << std::endl;
    std::vector<std::vector<double>> pts1_data = {{0.0, 0.0, 0.0}, {100.0, 0.0, 0.0}};
    Selector pts1 = Selector{"jot/Points", {{"points", pts1_data}}}.with_output("$out");
    Processor::execute(&vfs, pts1);

    std::vector<std::vector<double>> pts2_data = {{100.0, 0.0, 0.0}, {200.0, 0.0, 0.0}};
    Selector pts2 = Selector{"jot/Points", {{"points", pts2_data}}}.with_output("$out");
    Processor::execute(&vfs, pts2);

    // Union (Join)
    std::cout << "  Testing Join..." << std::endl;
    Selector join_pts = Selector{"jot/join", {{"$in", pts1}, {"tools", json::array({pts2})}}}.with_output("$out");
    Processor::execute(&vfs, join_pts);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(join_pts).geometry.value());
        std::cout << "    Points: " << res.points.size() << " (Expected 3)" << std::endl;
        assert(res.points.size() == 3); // (0,0,0), (100,0,0), (200,0,0)
    }

    // Subtract (Cut)
    std::cout << "  Testing Cut..." << std::endl;
    Selector cut_pts = Selector{"jot/cut", {{"$in", pts1}, {"tools", json::array({pts2})}}}.with_output("$out");
    Processor::execute(&vfs, cut_pts);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(cut_pts).geometry.value());
        std::cout << "    Points: " << res.points.size() << " (Expected 1)" << std::endl;
        assert(res.points.size() == 1); // Only (0,0,0) remains
    }

    // Intersect (Clip)
    std::cout << "  Testing Clip..." << std::endl;
    Selector clip_pts = Selector{"jot/clip", {{"$in", pts1}, {"tools", json::array({pts2})}}}.with_output("$out");
    Processor::execute(&vfs, clip_pts);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(clip_pts).geometry.value());
        std::cout << "    Points: " << res.points.size() << " (Expected 1)" << std::endl;
        assert(res.points.size() == 1); // Only (100,0,0) remains
    }

    // --- 2. SEGMENTS VS SEGMENTS (Collinear) ---
    std::cout << "--- Segments vs Segments ---" << std::endl;
    // Seg1: (0,0,0) -> (100,0,0)
    std::vector<std::vector<double>> seg1_pts = {{0.0, 0.0, 0.0}, {100.0, 0.0, 0.0}};
    Selector seg1_pts_addr = Selector{"jot/Points", {{"points", seg1_pts}}}.with_output("$out");
    Processor::execute(&vfs, seg1_pts_addr);
    Selector seg1 = Selector{"jot/link", {{"$in", seg1_pts_addr}}}.with_output("$out");
    Processor::execute(&vfs, seg1);

    // Seg2: (50,0,0) -> (150,0,0)
    std::vector<std::vector<double>> seg2_pts = {{50.0, 0.0, 0.0}, {150.0, 0.0, 0.0}};
    Selector seg2_pts_addr = Selector{"jot/Points", {{"points", seg2_pts}}}.with_output("$out");
    Processor::execute(&vfs, seg2_pts_addr);
    Selector seg2 = Selector{"jot/link", {{"$in", seg2_pts_addr}}}.with_output("$out");
    Processor::execute(&vfs, seg2);

    // Union (Join)
    std::cout << "  Testing Join (Collinear)..." << std::endl;
    Selector join_segs = Selector{"jot/join", {{"$in", seg1}, {"tools", json::array({seg2})}}}.with_output("$out");
    Processor::execute(&vfs, join_segs);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(join_segs).geometry.value());
        std::cout << "    Segments: " << res.segments.size() << " (Expected 1)" << std::endl;
        assert(res.segments.size() == 1); // Merged into (0,0,0) -> (150,0,0)
        save_png(join_segs, "join_segments_collinear.png");
    }

    // Subtract (Cut)
    std::cout << "  Testing Cut (Collinear)..." << std::endl;
    Selector cut_segs = Selector{"jot/cut", {{"$in", seg1}, {"tools", json::array({seg2})}}}.with_output("$out");
    Processor::execute(&vfs, cut_segs);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(cut_segs).geometry.value());
        std::cout << "    Segments: " << res.segments.size() << " (Expected 1)" << std::endl;
        assert(res.segments.size() == 1); // (0,0,0) -> (50,0,0) remains
        save_png(cut_segs, "cut_segments_collinear.png");
    }

    // Intersect (Clip)
    std::cout << "  Testing Clip (Collinear)..." << std::endl;
    Selector clip_segs = Selector{"jot/clip", {{"$in", seg1}, {"tools", json::array({seg2})}}}.with_output("$out");
    Processor::execute(&vfs, clip_segs);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(clip_segs).geometry.value());
        std::cout << "    Segments: " << res.segments.size() << " (Expected 1)" << std::endl;
        assert(res.segments.size() == 1); // (50,0,0) -> (100,0,0) remains
        save_png(clip_segs, "clip_segments_collinear.png");
    }

    // --- 3. SEGMENTS VS POINTS ---
    std::cout << "--- Segments vs Points ---" << std::endl;
    // Midpoint of seg1
    std::vector<std::vector<double>> mid_pts_data = {{50.0, 0.0, 0.0}};
    Selector mid_pts = Selector{"jot/Points", {{"points", mid_pts_data}}}.with_output("$out");
    Processor::execute(&vfs, mid_pts);

    // Cut segment by point (split)
    std::cout << "  Testing Cut Segment by Point (Split)..." << std::endl;
    Selector split_seg = Selector{"jot/cut", {{"$in", seg1}, {"tools", json::array({mid_pts})}}}.with_output("$out");
    Processor::execute(&vfs, split_seg);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(split_seg).geometry.value());
        std::cout << "    Segments: " << res.segments.size() << " (Expected 2)" << std::endl;
        assert(res.segments.size() == 2);
    }

    // --- 4. POINTS VS SEGMENTS ---
    std::cout << "--- Points vs Segments ---" << std::endl;
    // Points on seg1
    std::vector<std::vector<double>> test_pts_data = {{25.0, 0.0, 0.0}, {125.0, 0.0, 0.0}};
    Selector test_pts = Selector{"jot/Points", {{"points", test_pts_data}}}.with_output("$out");
    Processor::execute(&vfs, test_pts);

    // Cut points by segment
    std::cout << "  Testing Cut Points by Segment..." << std::endl;
    Selector cut_pts_by_seg = Selector{"jot/cut", {{"$in", test_pts}, {"tools", json::array({seg1})}}}.with_output("$out");
    Processor::execute(&vfs, cut_pts_by_seg);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(cut_pts_by_seg).geometry.value());
        std::cout << "    Points: " << res.points.size() << " (Expected 1)" << std::endl;
        assert(res.points.size() == 1); // (125,0,0) remains
    }

    std::cout << "✅ Lower-Dimensional Boolean Test Finished" << std::endl;
    return 0;
}
