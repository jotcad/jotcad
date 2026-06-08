#include "test_base.h"
#include "../../fs/cpp/cid.h"
#include "../../fs/cpp/vendor/stb_image_write.h"
#include <cmath>
#include <cassert>
#include <fstream>

extern "C" unsigned char *stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("decal");
    register_all_ops(&vfs);

    std::cout << "Testing Decal (Robust Per-Patch Texturing) Operation..." << std::endl;

    // 1. Create a Box Subject: width=30, height=30, depth=10
    // Centered at [0,0,0], so top face is at Z = 5.0
    Selector subject_sel = Selector{"jot/Box", {{"width", 30.0}, {"height", 30.0}, {"depth", 10.0}}}.with_output("$out");
    Processor::execute(&vfs, subject_sel);

    // 2. Create a Disk Pattern: diameter=10
    Selector pattern_sel = Selector{"jot/Disk", {{"diameter", 10.0}, {"zag", 0.5}}}.with_output("$out");
    Processor::execute(&vfs, pattern_sel);

    // 3. Generate a 2x2 mock grayscale image
    unsigned char pixels[12] = {
        0, 0, 0,
        255, 255, 255,
        255, 255, 255,
        255, 255, 255
    };
    int len = 0;
    unsigned char* png_data = stbi_write_png_to_mem(pixels, 2 * 3, 2, 2, 3, &len);
    assert(png_data);
    std::vector<uint8_t> png_bytes(png_data, png_data + len);
    free(png_data);

    Selector image_sel = Selector{"jot/Image", {{"url", "mock://decal_test.png"}}}.with_output("$out");
    vfs.write(image_sel, png_bytes);

    // 4. Generate open relief shape using close=false
    Selector relief_sel = Selector{"jot/relief", {
        {"$in", image_sel.to_json()},
        {"width", 24.0},
        {"breadth", 24.0},
        {"height", 1.9},
        {"base", 0.0},
        {"minArea", 0.0},
        {"close", false}
    }}.with_output("$out");
    Processor::execute(&vfs, relief_sel);

    // 5. Test Case 1: Execute DecalOp with seam="skirting" (default)
    Selector decal_skirting_sel = Selector{"jot/decal", {
        {"$in", subject_sel.to_json()},
        {"pattern", pattern_sel.to_json()},
        {"relief", relief_sel.to_json()},
        {"seam", "skirting"}
    }}.with_output("$out");

    std::cout << "  - Executing DecalOp (Test Case 1: seam=skirting)..." << std::endl;
    try {
        Processor::execute(&vfs, decal_skirting_sel);
    } catch (const std::exception& e) {
        std::cerr << "❌ DecalOp (skirting) failed: " << e.what() << std::endl;
        return 1;
    }

    // Verify results for Test Case 1
    try {
        Shape result = vfs.read<Shape>(decal_skirting_sel);
        assert(result.geometry.has_value());
        Geometry geo = vfs.read<Geometry>(result.geometry.value());
        
        std::cout << "    * Output vertices count: " << geo.vertices.size() << std::endl;
        std::cout << "    * Output triangles count: " << geo.triangles.size() << std::endl;
        assert(geo.vertices.size() > 0);
        assert(geo.triangles.size() > 0);

        double max_z = -1e18;
        for (const auto& v : geo.vertices) {
            double vz = CGAL::to_double(v.z);
            if (vz > max_z) max_z = vz;
        }

        std::cout << "    * Max Z detected: " << max_z << " (Expected ~6.9)" << std::endl;
        assert(max_z > 6.85 && max_z < 6.95);

        boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
        if (!CGAL::is_closed(mesh)) {
            std::cout << "DEBUG: Mesh is not closed. Listing open border edges:" << std::endl;
            for (auto h : mesh.halfedges()) {
                if (mesh.is_border(h)) {
                    auto v1 = mesh.source(h);
                    auto v2 = mesh.target(h);
                    std::cout << "  Border edge: " << mesh.point(v1) << " <-> " << mesh.point(v2) << std::endl;
                }
            }
        }
        vfs.verify_well_formed_solid(geo, "Decal Skirting Case");
        std::cout << "    * Solid verification: PASS" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Decal (skirting) verification failed: " << e.what() << std::endl;
        return 1;
    }

    // 6. Test Case 2: Execute DecalOp with seam="attenuate"
    Selector decal_attenuate_sel = Selector{"jot/decal", {
        {"$in", subject_sel.to_json()},
        {"pattern", pattern_sel.to_json()},
        {"relief", relief_sel.to_json()},
        {"seam", "attenuate"},
        {"fade_radius", 2.0}
    }}.with_output("$out");

    std::cout << "  - Executing DecalOp (Test Case 2: seam=attenuate)..." << std::endl;
    try {
        Processor::execute(&vfs, decal_attenuate_sel);
    } catch (const std::exception& e) {
        std::cerr << "❌ DecalOp (attenuate) failed: " << e.what() << std::endl;
        return 1;
    }

    // Verify results for Test Case 2
    try {
        Shape result = vfs.read<Shape>(decal_attenuate_sel);
        assert(result.geometry.has_value());
        Geometry geo = vfs.read<Geometry>(result.geometry.value());
        
        std::cout << "    * Output vertices count: " << geo.vertices.size() << std::endl;
        std::cout << "    * Output triangles count: " << geo.triangles.size() << std::endl;
        assert(geo.vertices.size() > 0);
        assert(geo.triangles.size() > 0);

        double max_z = -1e18;
        for (const auto& v : geo.vertices) {
            double vz = CGAL::to_double(v.z);
            if (vz > max_z) max_z = vz;
        }

        std::cout << "    * Max Z detected: " << max_z << " (Expected ~6.9)" << std::endl;
        assert(max_z > 6.85 && max_z < 6.95);

        boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
        if (!CGAL::is_closed(mesh)) {
            std::cout << "DEBUG: Mesh is not closed (attenuate). Listing open border edges:" << std::endl;
            for (auto h : mesh.halfedges()) {
                if (mesh.is_border(h)) {
                    auto v1 = mesh.source(h);
                    auto v2 = mesh.target(h);
                    std::cout << "  Border edge: " << mesh.point(v1) << " <-> " << mesh.point(v2) << std::endl;
                }
            }
        }
        vfs.verify_well_formed_solid(geo, "Decal Attenuate Case");
        std::cout << "    * Solid verification: PASS" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Decal (attenuate) verification failed: " << e.what() << std::endl;
        return 1;
    }

    // 7. Test Case 3: Execute DecalOp with seam="smooth"
    Selector decal_smooth_sel = Selector{"jot/decal", {
        {"$in", subject_sel.to_json()},
        {"pattern", pattern_sel.to_json()},
        {"relief", relief_sel.to_json()},
        {"seam", "smooth"}
    }}.with_output("$out");

    std::cout << "  - Executing DecalOp (Test Case 3: seam=smooth)..." << std::endl;
    try {
        Processor::execute(&vfs, decal_smooth_sel);
    } catch (const std::exception& e) {
        std::cerr << "❌ DecalOp (smooth) failed: " << e.what() << std::endl;
        return 1;
    }

    // Verify results for Test Case 3
    try {
        Shape result = vfs.read<Shape>(decal_smooth_sel);
        assert(result.geometry.has_value());
        Geometry geo = vfs.read<Geometry>(result.geometry.value());
        
        std::cout << "    * Output vertices count: " << geo.vertices.size() << std::endl;
        std::cout << "    * Output triangles count: " << geo.triangles.size() << std::endl;
        assert(geo.vertices.size() > 0);
        assert(geo.triangles.size() > 0);

        double max_z = -1e18;
        for (const auto& v : geo.vertices) {
            double vz = CGAL::to_double(v.z);
            if (vz > max_z) max_z = vz;
        }

        std::cout << "    * Max Z detected: " << max_z << " (Expected ~6.9)" << std::endl;
        assert(max_z > 6.85 && max_z < 6.95);

        boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
        if (!CGAL::is_closed(mesh)) {
            std::cout << "DEBUG: Mesh is not closed (smooth). Listing open border edges:" << std::endl;
            for (auto h : mesh.halfedges()) {
                if (mesh.is_border(h)) {
                    auto v1 = mesh.source(h);
                    auto v2 = mesh.target(h);
                    std::cout << "  Border edge: " << mesh.point(v1) << " <-> " << mesh.point(v2) << std::endl;
                }
            }
        }
        vfs.verify_well_formed_solid(geo, "Decal Smooth Case");
        std::cout << "    * Solid verification: PASS" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Decal (smooth) verification failed: " << e.what() << std::endl;
        return 1;
    }

    // 8. Test Case 4: Verify decal rejects closed relief mesh (close=true)
    Selector closed_relief_sel = Selector{"jot/relief", {
        {"$in", image_sel.to_json()},
        {"width", 10.0},
        {"breadth", 10.0},
        {"height", 1.9},
        {"base", 1.0},
        {"minArea", 0.0},
        {"close", true}
    }}.with_output("$out");
    Processor::execute(&vfs, closed_relief_sel);

    Selector decal_closed_sel = Selector{"jot/decal", {
        {"$in", subject_sel.to_json()},
        {"pattern", pattern_sel.to_json()},
        {"relief", closed_relief_sel.to_json()},
        {"seam", "skirting"}
    }}.with_output("$out");

    std::cout << "  - Executing DecalOp (Test Case 4: closed relief validation)..." << std::endl;
    bool threw = false;
    try {
        Processor::execute(&vfs, decal_closed_sel);
    } catch (const std::exception& e) {
        std::cout << "    * Caught expected exception: " << e.what() << std::endl;
        std::string err_msg = e.what();
        if (err_msg.find("Decal requires an open relief mesh") != std::string::npos) {
            threw = true;
        }
    }
    assert(threw);
    std::cout << "    * Closed relief validation: PASS" << std::endl;

    std::cout << "✅ Decal Test PASS" << std::endl;
    return 0;
}
