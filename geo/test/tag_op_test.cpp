#include "test_base.h"
#include "tag_ops.h"
#include "filter_ops.h"
#include "box_op.h"
#include "group_op.h"

using namespace jotcad::geo;
using namespace fs;

void test_set_get(MockVFS& vfs) {
    std::cout << "Testing Set/Get Tags..." << std::endl;
    
    // 1. Create a box
    Selector box_sel("jot/Box", {{"width", 10.0}, {"height", 10.0}});
    box_sel.output = "$out";
    Shape box = vfs.read<Shape>(box_sel);

    // 2. Set a tag
    Selector set_sel("jot/set", {{"$in", vfs.materialize(box).value}, {"key", "material"}, {"value", "steel"}});
    set_sel.output = "$out";
    Shape tagged = vfs.read<Shape>(set_sel);
    if (tagged.tags["material"] != "steel") {
        std::cerr << "FAIL: Tag not set correctly. Expected 'steel', got " << tagged.tags["material"] << std::endl;
        exit(1);
    }

    // 3. Get the tag back
    Selector get_sel("jot/get", {{"$in", vfs.materialize(tagged).value}, {"key", "material"}});
    get_sel.output = "$out";
    std::string val = vfs.read<std::string>(get_sel);
    if (val != "steel") {
        std::cerr << "FAIL: Tag not retrieved correctly. Expected 'steel', got " << val << std::endl;
        exit(1);
    }
    std::cout << "  SUCCESS" << std::endl;
}

void test_keep_drop(MockVFS& vfs) {
    std::cout << "Testing Keep/Drop by Tag..." << std::endl;

    // 1. Create two boxes with different tags
    Selector selA("jot/Box", {{"width", 5.0}});
    selA.output = "$out";
    Shape boxA = vfs.read<Shape>(selA);
    boxA.add_tag("id", "A");
    
    Selector selB("jot/Box", {{"width", 10.0}});
    selB.output = "$out";
    Shape boxB = vfs.read<Shape>(selB);
    boxB.add_tag("id", "B");

    // 2. Create a group
    Shape group = Shape::group({boxA, boxB});
    CID group_cid = vfs.materialize(group);

    // 3. Keep only A
    Selector hasA("jot/has", {{"key", "id"}, {"value", "A"}});
    Selector keepA("jot/keep", {{"$in", group_cid.value}, {"selector", hasA}});
    keepA.output = "$out";
    Shape keptA = vfs.read<Shape>(keepA);

    if (keptA.components.size() != 1 || keptA.components[0].tags["id"] != "A") {
        std::cerr << "FAIL: Keep logic failed." << std::endl;
        exit(1);
    }

    // 4. Drop A
    Selector dropA("jot/drop", {{"$in", group_cid.value}, {"selector", hasA}});
    dropA.output = "$out";
    Shape droppedA = vfs.read<Shape>(dropA);

    if (droppedA.components.size() != 1 || droppedA.components[0].tags["id"] != "B") {
        std::cerr << "FAIL: Drop logic failed." << std::endl;
        exit(1);
    }
    std::cout << "  SUCCESS" << std::endl;
}

void test_recursive_pruning(MockVFS& vfs) {
    std::cout << "Testing Recursive Pruning..." << std::endl;

    // Nested structure: Group([ Group([A, B]), C ])
    Selector selA("jot/Box", {{"width", 1.0}});
    selA.output = "$out";
    Shape boxA = vfs.read<Shape>(selA);
    boxA.add_tag("find_me", true);
    boxA.add_tag("name", "A");

    Selector selB("jot/Box", {{"width", 1.0}});
    selB.output = "$out";
    Shape boxB = vfs.read<Shape>(selB);
    boxB.add_tag("name", "B");

    Selector selC("jot/Box", {{"width", 1.0}});
    selC.output = "$out";
    Shape boxC = vfs.read<Shape>(selC);
    boxC.add_tag("name", "C");

    Shape inner = Shape::group({boxA, boxB});
    Shape root = Shape::group({inner, boxC});
    
    CID root_cid = vfs.materialize(root);

    // Keep "find_me"
    Selector query("jot/has", {{"key", "find_me"}});
    Selector keep_sel("jot/keep", {{"$in", root_cid.value}, {"selector", query}});
    keep_sel.output = "$out";
    Shape result = vfs.read<Shape>(keep_sel);

    // Result should be Group([ Group([A]) ])
    if (result.components.size() != 1 || result.components[0].components.size() != 1 || result.components[0].components[0].tags["name"] != "A") {
        std::cerr << "FAIL: Recursive pruning failed." << std::endl;
        exit(1);
    }
    std::cout << "  SUCCESS" << std::endl;
}

int main() {
    MockVFS vfs("tag_test");
    register_all_ops(&vfs);
    
    test_set_get(vfs);
    test_keep_drop(vfs);
    test_recursive_pruning(vfs);
    
    return 0;
}
