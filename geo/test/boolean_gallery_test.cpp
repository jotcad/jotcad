#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include "impl/protocols.h"
#include "vfs_node.h"
#include "impl/processor.h"
#include "impl/rasterizer.h"
#include "box_op.h"
#include "plane_op.h"
#include "cut_op.h"
#include "segment_op.h"
#include "points_op.h"
#include "../../fs/cpp/cid.h"

using namespace jotcad;
using namespace jotcad::geo;
using namespace fs;

void save_gallery_image(const Geometry& geo, const std::string& label) {
    std::vector<uint8_t> png_bytes = Rasterizer::render_png(geo, 512, 512);
    std::string filename = "boolean_gallery_" + label + ".actual.png";
    std::ofstream out(filename, std::ios::binary);
    out.write((char*)png_bytes.data(), png_bytes.size());
    out.close();
    std::cout << "  📸 Generated: " << filename << std::endl;
}

// Side-by-side helper
void save_gallery_comparison(const Geometry& target, const Geometry& tool, const Geometry& result, const std::string& label) {
    // Offset them for a "story" view: [Target] [Tool] -> [Result]
    Geometry combined;
    auto add_offset = [&](const Geometry& src, double dx) {
        int base = (int)combined.vertices.size();
        for (const auto& v : src.vertices) combined.vertices.push_back({v.x + FT(dx), v.y, v.z});
        for (const auto& f : src.faces) {
            Geometry::Face nf = f;
            for (auto& loop : nf.loops) for (auto& idx : loop) idx += base;
            combined.faces.push_back(nf);
        }
        for (const auto& t : src.triangles) combined.triangles.push_back({t[0] + base, t[1] + base, t[2] + base});
        for (const auto& s : src.segments) combined.segments.push_back({s[0] + base, s[1] + base});
    };

    add_offset(target, -15.0);
    add_offset(tool, 0.0);
    add_offset(result, 15.0);

    save_gallery_image(combined, label);
}

int main() {
    VFSNode::Config cfg;
    cfg.id = "boolean-gallery-node";
    cfg.storage_dir = ".vfs_storage_boolean_gallery";
    std::filesystem::remove_all(cfg.storage_dir);
    VFSNode vfs(cfg);
    
    // Register Ops
    Processor::register_op<BoxOp<>, double, double, double>("jot/Box");
    Processor::register_op<XOp<>>("jot/X");
    Processor::register_op<YOp<>>("jot/Y");
    Processor::register_op<ZOp<>>("jot/Z");
    Processor::register_op<CutOp<>, Shape, std::vector<Shape>, bool>("jot/cut");
    Processor::register_op<LinkOp<>, std::vector<Shape>>("jot/link");
    Processor::register_op<PointOp<>, double, double, double>("jot/Point");

    std::cout << "Generating Boolean Interaction Gallery..." << std::endl;

    // 1. Mesh-Mesh: Box Subtraction
    {
        BoxOp<>::execute(&vfs, {"jot/Box/Target", {}}, 10.0, 10.0, 10.0);
        BoxOp<>::execute(&vfs, {"jot/Box/Tool", {}}, 5.0, 5.0, 20.0);
        Shape target = vfs.read<Shape>({"jot/Box/Target", {}});
        Shape tool = vfs.read<Shape>({"jot/Box/Tool", {}});
        tool.tf = Matrix::translate(2, 2, 0).to_vec();

        CutOp<>::execute(&vfs, {"jot/cut/mesh", {}}, target, {tool}, false);
        Shape res_shape = vfs.read<Shape>({"jot/cut/mesh", {}});

        Geometry t_geo = vfs.read<Geometry>(target.geometry.value());
        Geometry l_geo = vfs.read<Geometry>(tool.geometry.value());
        l_geo.apply_tf(tool.tf);
        Geometry r_geo = vfs.read<Geometry>(res_shape.geometry.value());

        save_gallery_comparison(t_geo, l_geo, r_geo, "mesh_mesh");
    }

    // 2. Mesh-Plane: Box Clipping
    {
        BoxOp<>::execute(&vfs, {"jot/Box/Target2", {}}, 10.0, 10.0, 10.0);
        ZOp<>::execute(&vfs, {"jot/Z/Tool", {}});
        Shape target = vfs.read<Shape>({"jot/Box/Target2", {}});
        Shape tool = vfs.read<Shape>({"jot/Z/Tool", {}});
        tool.tf = Matrix::translate(0, 0, 2).to_vec();

        CutOp<>::execute(&vfs, {"jot/cut/plane", {}}, target, {tool}, false);
        Shape res_shape = vfs.read<Shape>({"jot/cut/plane", {}});

        Geometry t_geo = vfs.read<Geometry>(target.geometry.value());
        Geometry l_geo = vfs.read<Geometry>(tool.geometry.value());
        l_geo.apply_tf(tool.tf);
        Geometry r_geo = vfs.read<Geometry>(res_shape.geometry.value());

        save_gallery_comparison(t_geo, l_geo, r_geo, "mesh_plane");
    }

    /*
    // 5. PWH-PWH: Coplanar Subtract (Hole)
    {
        Geometry large;
...
        save_gallery_comparison(t_geo, l_geo, r_geo, "pwh_pwh");
    }
    */

    std::cout << "Gallery Generation Complete." << std::endl;
    return 0;
}
