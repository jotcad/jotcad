#include "test_base.h"
#include <iomanip>

using namespace jotcad::geo;

int main() {
    MockVFS vfs("selection");
    register_all_ops(&vfs);
    
    std::cout << "Testing Selection System (highest/lowest)..." << std::endl;

    // 1. Setup a Box and extract its faces
    fs::Selector box_sel = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Processor::execute(&vfs, box_sel);
    Shape box = vfs.read<Shape>(box_sel);

    fs::Selector faces_sel = fs::Selector{"jot/faces", {{"$in", vfs.materialize(box).value}}}.with_output("$out");
    Processor::execute(&vfs, faces_sel);
    Shape faces_group = vfs.read<Shape>(faces_sel);

    assert(faces_group.components.size() == 6);
    std::cout << "  - Extracted 6 faces from Box(10,10,10)" << std::endl;

    // 2. Test highest(z(), 0) -> Should be all faces reaching Z=5
    // Top face (Z=5) + 4 side faces (Z=[-5, 5])
    fs::Selector highest_z_sel = fs::Selector{"jot/highest", {
        {"$in", vfs.materialize(faces_group).value},
        {"measure", fs::Selector{"jot/z"}},
        {"bucket", 0}
    }}.with_output("$out");
    Processor::execute(&vfs, highest_z_sel);
    Shape highest_z = vfs.read<Shape>(highest_z_sel);

    std::cout << "  - highest(z(), 0) count: " << highest_z.components.size() << std::endl;
    assert(highest_z.components.size() == 5);

    // 2b. Test highest(facing(UP), 0) -> Should be only the top face
    fs::Selector facing_up_sel = fs::Selector{"jot/highest", {
        {"$in", vfs.materialize(faces_group).value},
        {"measure", fs::Selector{"jot/facing", {{"vec", std::vector<double>{0,0,1}}}} },
        {"bucket", 0}
    }}.with_output("$out");
    Processor::execute(&vfs, facing_up_sel);
    Shape highest_facing = vfs.read<Shape>(facing_up_sel);
    std::cout << "  - highest(facing(UP), 0) count: " << highest_facing.components.size() << std::endl;
    assert(highest_facing.components.size() == 1);
    assert(std::abs(CGAL::to_double(highest_facing.components[0].tf.t.hm(2, 3)) - 5.0) < 1e-6);

    // 3. Test lowest(z(), 0) -> Should be all faces reaching Z=-5
    // Bottom face (Z=-5) + 4 side faces (Z=[-5, 5])
    fs::Selector lowest_z_sel = fs::Selector{"jot/lowest", {
        {"$in", vfs.materialize(faces_group).value},
        {"measure", fs::Selector{"jot/z"}},
        {"bucket", 0}
    }}.with_output("$out");
    Processor::execute(&vfs, lowest_z_sel);
    Shape lowest_z = vfs.read<Shape>(lowest_z_sel);

    std::cout << "  - lowest(z(), 0) count: " << lowest_z.components.size() << std::endl;
    assert(lowest_z.components.size() == 5);

    // 4. Test Epsilon Bucketing (Sideways faces)
    // facing(UP) should give: Bucket 0 (Top, val=1), Bucket 1 (Sides, val=0), Bucket 2 (Bottom, val=-1)
    fs::Selector side_faces_sel = fs::Selector{"jot/highest", {
        {"$in", vfs.materialize(faces_group).value},
        {"measure", fs::Selector{"jot/facing", {{"vec", std::vector<double>{0,0,1}}}} },
        {"bucket", 1}
    }}.with_output("$out");
    Processor::execute(&vfs, side_faces_sel);
    Shape side_faces = vfs.read<Shape>(side_faces_sel);

    std::cout << "  - highest(facing(UP), 1) (sides) count: " << side_faces.components.size() << std::endl;
    assert(side_faces.components.size() == 4);

    // 5. Test Area Ranking
    // Create a group with a big box face and a small triangle face
    Shape big_face = highest_z.components[0];
    
    fs::Selector tri_sel = fs::Selector{"jot/Triangle", {{"va", 2.0}, {"vb", 2.0}, {"vc", 2.0}}}.with_output("$out");
    Processor::execute(&vfs, tri_sel);
    Shape small_tri = vfs.read<Shape>(tri_sel); // Area = sqrt(3) ~ 1.73
    
    Shape mix_group;
    mix_group.add_tag("type", "group");
    mix_group.components = {big_face, small_tri};

    fs::Selector highest_area_sel = fs::Selector{"jot/highest", {
        {"$in", vfs.materialize(mix_group).value},
        {"measure", fs::Selector{"jot/area"}},
        {"bucket", 0}
    }}.with_output("$out");
    Processor::execute(&vfs, highest_area_sel);
    Shape high_area = vfs.read<Shape>(highest_area_sel);
    
    std::cout << "  - highest(area(), 0) count: " << high_area.components.size() << std::endl;
    assert(high_area.components.size() == 1);
    // Big face is 10x10 = 100
    
    fs::Selector lowest_area_sel = fs::Selector{"jot/lowest", {
        {"$in", vfs.materialize(mix_group).value},
        {"measure", fs::Selector{"jot/area"}},
        {"bucket", 0}
    }}.with_output("$out");
    Processor::execute(&vfs, lowest_area_sel);
    Shape low_area = vfs.read<Shape>(lowest_area_sel);
    std::cout << "  - lowest(area(), 0) count: " << low_area.components.size() << std::endl;
    assert(low_area.components.size() == 1);

    // 6. Test jot/top and jot/bottom on Box(10,10,10)
    {
        fs::Selector top_sel = fs::Selector("jot/top", {{"$in", vfs.materialize(box).value}}).with_output("$out");
        Processor::execute(&vfs, top_sel);
        Shape top_face = vfs.read<Shape>(top_sel);
        assert(top_face.components.empty()); // Should be a single face component, not a group
        assert(std::abs(CGAL::to_double(top_face.tf.t.cartesian(2, 3)) - 5.0) < 1e-6);

        fs::Selector bottom_sel = fs::Selector("jot/bottom", {{"$in", vfs.materialize(box).value}}).with_output("$out");
        Processor::execute(&vfs, bottom_sel);
        Shape bottom_face = vfs.read<Shape>(bottom_sel);
        assert(bottom_face.components.empty()); // Should be a single face component
        assert(std::abs(CGAL::to_double(bottom_face.tf.t.cartesian(2, 3)) - (-5.0)) < 1e-6);
        std::cout << "  - top() and bottom() on Box(10,10,10) passed" << std::endl;
    }

    // 7. Test jot/top and jot/bottom on composite steps (staircase)
    {
        fs::Selector b1_sel = fs::Selector("jot/Box", {{"width", 2.0}, {"height", 2.0}, {"depth", 2.0}}).with_output("$out");
        Processor::execute(&vfs, b1_sel);
        Shape b1 = vfs.read<Shape>(b1_sel); // Z center at 0, Z range [-1, 1]

        fs::Selector b2_sel = fs::Selector("jot/Box", {{"width", 2.0}, {"height", 2.0}, {"depth", 2.0}}).with_output("$out");
        Processor::execute(&vfs, b2_sel);
        Shape b2 = vfs.read<Shape>(b2_sel);
        b2.apply_transform(Matrix::translate(0, 0, 2.0)); // Z center at 2, Z range [1, 3]

        fs::Selector fuse_sel = fs::Selector("jot/Fuse", {{"shapes", nlohmann::json::array({vfs.materialize(b1).value, vfs.materialize(b2).value})}}).with_output("$out");
        Processor::execute(&vfs, fuse_sel);
        Shape fused = vfs.read<Shape>(fuse_sel); // Fused solid Z range [-1, 3]

        fs::Selector top_sel = fs::Selector("jot/top", {{"$in", vfs.materialize(fused).value}}).with_output("$out");
        Processor::execute(&vfs, top_sel);
        Shape top_face = vfs.read<Shape>(top_sel);
        assert(top_face.components.empty()); // Should be a single face component
        assert(std::abs(CGAL::to_double(top_face.tf.t.cartesian(2, 3)) - 3.0) < 1e-6); // Top-most face of fused steps is at Z=3

        fs::Selector bottom_sel = fs::Selector("jot/bottom", {{"$in", vfs.materialize(fused).value}}).with_output("$out");
        Processor::execute(&vfs, bottom_sel);
        Shape bottom_face = vfs.read<Shape>(bottom_sel);
        assert(bottom_face.components.empty()); // Should be a single face component
        assert(std::abs(CGAL::to_double(bottom_face.tf.t.cartesian(2, 3)) - (-1.0)) < 1e-6); // Bottom-most face of fused steps is at Z=-1
        std::cout << "  - top() and bottom() on composite steps passed" << std::endl;
    }

    // 8. Test a.by(top().o()) places the top of a at the origin
    {
        fs::Selector top_sel = fs::Selector("jot/top", {{"$in", vfs.materialize(box).value}}).with_output("$out");
        
        fs::Selector o_sel = fs::Selector("jot/o", {{"$in", top_sel}}).with_output("$out");
        
        fs::Selector by_sel = fs::Selector("jot/by", {
            {"$in", vfs.materialize(box).value},
            {"targets", nlohmann::json::array({o_sel})}
        }).with_output("$out");
        
        Processor::execute(&vfs, by_sel);
        Shape moved = vfs.read<Shape>(by_sel);
        
        // The top face of the 'moved' box should be at Z = 0
        fs::Selector moved_top_sel = fs::Selector("jot/top", {{"$in", vfs.materialize(moved).value}}).with_output("$out");
        Processor::execute(&vfs, moved_top_sel);
        Shape moved_top = vfs.read<Shape>(moved_top_sel);
        assert(std::abs(CGAL::to_double(moved_top.tf.t.cartesian(2, 3))) < 1e-6);
        std::cout << "  - a.by(top().o()) places top at origin passed" << std::endl;
    }

    std::cout << "✅ ALL Selection Tests Passed" << std::endl;
    return 0;
}
