#include "boolean/engine.h"
#include "render/rasterizer.h"
#include "test_base.h"
#include <fstream>

using namespace jotcad::geo;
using namespace jotcad::geo::boolean;

void save_png(fs::VFSNode& vfs, const Shape& s, const std::string& name) {
    auto png_data = Rasterizer::render_png(&vfs, s, 512, 512, 0.0, 0.0);
    if (png_data.empty()) {
        std::cout << "  ⚠️  Skipped actual/" << name << ".png (Empty Geometry)" << std::endl;
        return;
    }
    std::ofstream out("actual/" + name + ".png", std::ios::binary);
    out.write((const char*)png_data.data(), png_data.size());
    std::cout << "  📸 Saved actual/" << name << ".png" << std::endl;
}

Shape execute_op(fs::VFSNode* vfs, const std::string& path, const Shape& in, const std::vector<Shape>& tools) {
    fs::Selector sel(path);
    sel.parameters["$in"] = in;
    sel.parameters["tools"] = tools;
    sel.output = "$out";
    Processor::execute(vfs, sel);
    return vfs->read<Shape>(sel);
}

Geometry create_grid(int size, int step) {
    Geometry geo;
    for (int i = -size; i <= size; i += step) {
        int v0 = geo.vertices.size();
        geo.vertices.push_back({FT(i), FT(-size), FT(0)});
        geo.vertices.push_back({FT(i), FT(size), FT(0)});
        geo.segments.push_back({v0, v0+1});

        int v2 = geo.vertices.size();
        geo.vertices.push_back({FT(-size), FT(i), FT(0)});
        geo.vertices.push_back({FT(size), FT(i), FT(0)});
        geo.segments.push_back({v2, v2+1});
    }
    return geo;
}

void test_boolean_visuals() {
    MockVFS vfs("boolean_visuals");
    register_all_ops(&vfs);
    
    Geometry grid_geo = create_grid(20, 1); // Finer grid
    Shape grid;
    grid.geometry = vfs.materialize<Geometry>(grid_geo);
    grid.add_tag("type", "segments");

    // 1. Box (Solid) - Diamond rotation
    // BoxOp(15) -> [-7.5, 7.5]^3
    fs::Selector box_sel("jot/Box");
    box_sel.parameters = {{"width", 15.0}, {"height", 15.0}, {"depth", 15.0}};
    box_sel.output = "$out";
    Processor::execute(&vfs, box_sel);
    Shape box_tool = vfs.read<Shape>(box_sel);
    box_tool.tf = Matrix::rotationZ(0.125); 

    save_png(vfs, execute_op(&vfs, "jot/clip", grid, {box_tool}), "clip_segments_diamond");
    save_png(vfs, execute_op(&vfs, "jot/cut", grid, {box_tool}), "cut_segments_diamond");

    // 2. Disk (Unextruded)
    // Disk(20) centered at 0
    fs::Selector disk_sel("jot/Disk");
    disk_sel.parameters = {{"diameter", std::vector<double>{-10.0, 10.0}}};
    disk_sel.output = "$out";
    Processor::execute(&vfs, disk_sel);
    Shape disk_tool = vfs.read<Shape>(disk_sel);

    save_png(vfs, execute_op(&vfs, "jot/clip", grid, {disk_tool}), "clip_segments_disk_flat");
    save_png(vfs, execute_op(&vfs, "jot/cut", grid, {disk_tool}), "cut_segments_disk_flat");

    // 3. Cylinder (Extruded Disk)
    fs::Selector cyl_sel("jot/ez");
    cyl_sel.parameters = {{"$in", disk_tool}, {"height", std::vector<double>{-5.0, 5.0}}};
    cyl_sel.output = "$out";
    Processor::execute(&vfs, cyl_sel);
    Shape cyl_tool = vfs.read<Shape>(cyl_sel);

    save_png(vfs, execute_op(&vfs, "jot/clip", grid, {cyl_tool}), "clip_segments_cylinder");
    save_png(vfs, execute_op(&vfs, "jot/cut", grid, {cyl_tool}), "cut_segments_cylinder");

    // 4. Plane
    fs::Selector plane_sel("jot/Z");
    plane_sel.parameters = {{"offset", 0.0}};
    plane_sel.output = "$out";
    Processor::execute(&vfs, plane_sel);
    Shape plane_tool = vfs.read<Shape>(plane_sel);
    plane_tool.tf = Matrix::rotationY(0.25); 

    save_png(vfs, execute_op(&vfs, "jot/clip", grid, {plane_tool}), "clip_segments_plane");
    save_png(vfs, execute_op(&vfs, "jot/cut", grid, {plane_tool}), "cut_segments_plane");
}

int main() {
    std::cout << "Testing Visual Booleans (High-Level Ops) [Build Confirm 1]..." << std::endl;
    std::filesystem::create_directories("actual");
    test_boolean_visuals();
    std::cout << "✅ Visual Tests Complete" << std::endl;
    return 0;
}
