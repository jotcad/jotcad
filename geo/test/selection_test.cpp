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

    std::cout << "✅ ALL Selection Tests Passed" << std::endl;
    return 0;
}
