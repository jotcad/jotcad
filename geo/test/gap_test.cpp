#include "test_base.h"
#include "../ops/transform_ops.h"
#include "../ops/box_op.h"
#include "../ops/join_op.h"
#include "../ops/cut_op.h"
#include "../ops/clip_op.h"
#include "../boolean/engine.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("gap_test");
    register_all_ops(&vfs);

    auto make_box = [&](double w, double h, double d) {
        fs::Selector sel("jot/Box", {{"width", w}, {"height", h}, {"depth", d}});
        return vfs.read<Shape>(sel.with_output("$out"));
    };

    auto make_gap = [&](const Shape& s) {
        fs::Selector sel("jot/gap");
        sel.parameters["$in"] = vfs.materialize(s).value;
        return vfs.read<Shape>(sel.with_output("$out"));
    };

    // --- TEST 1: Join With Gap ---
    {
        std::cout << "Testing Join with Gap..." << std::endl;
        Shape b1 = make_box(10, 10, 10);
        Shape b2 = make_box(10, 10, 10);
        b2.apply_transform(Matrix::translate(5, 0, 0));
        Shape gap = make_gap(b2);

        fs::Selector join_sel("jot/join");
        join_sel.parameters["$in"] = vfs.materialize(b1).value;
        join_sel.parameters["tools"] = {vfs.materialize(gap).value};
        Shape res = vfs.read<Shape>(join_sel.with_output("$out"));

        Geometry geo = vfs.read<Geometry>(res.geometry.value());
        boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
        double vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(mesh));
        
        std::cout << "  Volume after join with gap: " << vol << " (expected < 1000)" << std::endl;
        if (vol >= 1000.0 || vol <= 0.0) {
            std::cerr << "FAIL: Join with gap did not subtract volume!" << std::endl;
            return 1;
        }
    }

    // --- TEST 2: Clip With Gap ---
    {
        std::cout << "Testing Clip with Gap..." << std::endl;
        Shape b1 = make_box(10, 10, 10);
        Shape b2 = make_box(10, 10, 10);
        b2.apply_transform(Matrix::translate(2, 0, 0));
        Shape b3 = make_box(4, 4, 4);
        b3.apply_transform(Matrix::translate(5, 0, 0));
        Shape gap = make_gap(b3);

        fs::Selector clip_sel("jot/clip");
        clip_sel.parameters["$in"] = vfs.materialize(b1).value;
        clip_sel.parameters["tools"] = {vfs.materialize(b2).value, vfs.materialize(gap).value};
        Shape res = vfs.read<Shape>(clip_sel.with_output("$out"));

        Geometry geo = vfs.read<Geometry>(res.geometry.value());
        boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
        double vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(mesh));
        
        std::cout << "  Volume after clip with gap: " << vol << " (expected 768)" << std::endl;
        if (std::abs(vol - 768.0) > 1.0) {
            std::cerr << "FAIL: Clip with gap volume mismatch! Vol: " << vol << std::endl;
            return 1;
        }
    }

    // --- TEST 3: Gap Is Persistent ---
    {
        std::cout << "Testing Gap Persistency..." << std::endl;
        Shape b1 = make_box(10, 10, 10);
        Shape b2 = make_box(10, 10, 10);
        b2.apply_transform(Matrix::translate(5, 0, 0));
        Shape gap = make_gap(b1);

        fs::Selector join_sel("jot/join");
        join_sel.parameters["$in"] = vfs.materialize(gap).value;
        join_sel.parameters["tools"] = {vfs.materialize(b2).value};
        Shape res = vfs.read<Shape>(join_sel.with_output("$out"));

        bool found_gap = false;
        std::function<void(const Shape&)> check = [&](const Shape& s) {
            if (s.is_gap()) {
                found_gap = true;
                Geometry g = vfs.read<Geometry>(s.geometry.value());
                boolean::Surface_mesh m = boolean::Engine::geometry_to_mesh(g);
                double vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(m));
                std::cout << "  Found gap component, volume: " << vol << std::endl;
                if (std::abs(vol - 1000.0) > 1.0) {
                    std::cerr << "FAIL: Gap was modified by join tool!" << std::endl;
                    exit(1);
                }
            }
            for (const auto& c : s.components) check(c);
        };
        check(res);
        if (!found_gap) {
            std::cerr << "FAIL: Gap component lost in output!" << std::endl;
            return 1;
        }
    }

    std::cout << "SUCCESS: gap behavior verified." << std::endl;
    return 0;
}
