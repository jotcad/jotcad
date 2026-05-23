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
    Selector hasA("jot/has", {{"$in", group_cid.value}, {"key", "id"}, {"value", "A"}});
    hasA.output = "$out";
    Shape hasA_shape = vfs.read<Shape>(hasA);
    CID hasA_cid = vfs.materialize(hasA_shape);

    Selector keepA("jot/keep", {{"$in", group_cid.value}, {"selector", hasA_cid.value}});
    keepA.output = "$out";
    Shape keptA = vfs.read<Shape>(keepA);

    if (keptA.components.size() != 1 || keptA.components[0].tags["id"] != "A") {
        std::cerr << "FAIL: Keep logic failed." << std::endl;
        exit(1);
    }

    // 4. Drop A
    Selector dropA("jot/drop", {{"$in", group_cid.value}, {"selector", hasA_cid.value}});
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
    Selector query("jot/has", {{"$in", root_cid.value}, {"key", "find_me"}});
    query.output = "$out";
    Shape query_shape = vfs.read<Shape>(query);
    CID query_cid = vfs.materialize(query_shape);

    Selector keep_sel("jot/keep", {{"$in", root_cid.value}, {"selector", query_cid.value}});
    keep_sel.output = "$out";
    Shape result = vfs.read<Shape>(keep_sel);

    std::cout << "[DEBUG] result components size: " << result.components.size() << std::endl;
    if (result.components.size() > 0) {
        std::cout << "[DEBUG] result[0] components size: " << result.components[0].components.size() << std::endl;
        for (const auto& child : result.components[0].components) {
            std::cout << "[DEBUG] child name: " << child.tags.value("name", "unnamed") << std::endl;
        }
    }

    // Result should be Group([ Group([A]) ])
    if (result.components.size() != 1 || result.components[0].components.size() != 1 || result.components[0].components[0].tags["name"] != "A") {
        std::cerr << "FAIL: Recursive pruning failed." << std::endl;
        exit(1);
    }
    std::cout << "  SUCCESS" << std::endl;
}

void test_item_ops(MockVFS& vfs) {
    std::cout << "Testing Item and item Ops..." << std::endl;
    
    // 1. Create a box
    Selector box_sel("jot/Box", {{"width", 10.0}});
    box_sel.output = "$out";
    Shape box = vfs.read<Shape>(box_sel);
    CID box_cid = vfs.materialize(box);

    // 2. Wrap as an Item constructor: Item('bracket', [box])
    Selector item_constructor("jot/Item", {{"name", "bracket"}, {"shapes", std::vector<Shape>{box}}});
    item_constructor.output = "$out";
    Shape item1 = vfs.read<Shape>(item_constructor);
    
    if (item1.tags["type"] != "item" || item1.tags["item:name"] != "bracket") {
        std::cerr << "FAIL: Item constructor tags not set correctly." << std::endl;
        exit(1);
    }
    if (item1.components.size() != 1) {
        std::cerr << "FAIL: Item constructor did not include shapes." << std::endl;
        exit(1);
    }

    // 3. Wrap using method: box.item('bracket_method')
    Selector item_method("jot/item", {{"$in", box_cid.value}, {"name", "bracket_method"}, {"shapes", std::vector<Shape>()}});
    item_method.output = "$out";
    Shape item2 = vfs.read<Shape>(item_method);
    
    if (item2.tags["type"] != "item" || item2.tags["item:name"] != "bracket_method") {
        std::cerr << "FAIL: Item method tags not set correctly." << std::endl;
        exit(1);
    }
    if (item2.components.size() != 1) {
        std::cerr << "FAIL: Item method did not wrap the subject." << std::endl;
        exit(1);
    }
    
    std::cout << "  SUCCESS" << std::endl;
}

void test_in_item_ops(MockVFS& vfs) {
    std::cout << "Testing inItem scope limiter Op..." << std::endl;
    
    // 1. Create a nested item hierarchy: Group([ Item('bracket', [Box(10).set('color', 'red')]), Box(20).set('color', 'blue') ])
    Selector box_sel1("jot/Box", {{"width", 10.0}});
    box_sel1.output = "$out";
    Shape box1 = vfs.read<Shape>(box_sel1);
    box1.tags["color"] = "red";

    Selector item_constructor("jot/Item", {{"name", "bracket"}, {"shapes", std::vector<Shape>{box1}}});
    item_constructor.output = "$out";
    Shape item = vfs.read<Shape>(item_constructor);

    Selector box_sel2("jot/Box", {{"width", 20.0}});
    box_sel2.output = "$out";
    Shape box2 = vfs.read<Shape>(box_sel2);
    box2.tags["color"] = "blue";

    Shape assembly = Shape::group({item, box2});
    CID assembly_cid = vfs.materialize(assembly);

    // 2. Extract item using inItem('bracket')
    Selector in_item_sel("jot/inItem", {{"$in", assembly_cid.value}, {"name", "bracket"}});
    in_item_sel.output = "$out";
    Shape extracted = vfs.read<Shape>(in_item_sel);

    if (extracted.components.size() != 1 || extracted.components[0].tags["type"] != "item" || extracted.components[0].tags["item:name"] != "bracket") {
        std::cerr << "FAIL: inItem did not extract the named item correctly." << std::endl;
        exit(1);
    }
    
    // 3. Test chaining: inItem('bracket').has('color', 'red')
    CID extracted_cid = vfs.materialize(extracted);
    Selector chain_sel("jot/has", {{"$in", extracted_cid.value}, {"key", "color"}, {"value", "red"}});
    chain_sel.output = "$out";
    Shape chain_res = vfs.read<Shape>(chain_sel);

    // Should return the red component inside the bracket item
    if (chain_res.components.empty() || chain_res.components[0].components[0].tags["color"] != "red") {
        std::cerr << "FAIL: Chained tag filtering inside extracted inItem failed." << std::endl;
        exit(1);
    }

    std::cout << "  SUCCESS" << std::endl;
}

void test_has_atomic_items(MockVFS& vfs) {
    std::cout << "Testing that has() does not recurse through nested items..." << std::endl;

    // 1. Create a nested item hierarchy: Item('assembly_item', [ Item('bracket', [Box(10).set('color', 'red')]), Box(20).set('color', 'red') ])
    Selector box_sel1("jot/Box", {{"width", 10.0}});
    box_sel1.output = "$out";
    Shape box1 = vfs.read<Shape>(box_sel1);
    box1.tags["color"] = "red";

    Selector item_constructor("jot/Item", {{"name", "bracket"}, {"shapes", std::vector<Shape>{box1}}});
    item_constructor.output = "$out";
    Shape item = vfs.read<Shape>(item_constructor);

    Selector box_sel2("jot/Box", {{"width", 20.0}});
    box_sel2.output = "$out";
    Shape box2 = vfs.read<Shape>(box_sel2);
    box2.tags["color"] = "red";

    // Wrap both inside a parent assembly item
    Selector parent_constructor("jot/Item", {{"name", "assembly_item"}, {"shapes", std::vector<Shape>{item, box2}}});
    parent_constructor.output = "$out";
    Shape parent_item = vfs.read<Shape>(parent_constructor);
    CID parent_cid = vfs.materialize(parent_item);

    // 2. Query has('color', 'red') at the assembly item level
    Selector has_sel("jot/has", {{"$in", parent_cid.value}, {"key", "color"}, {"value", "red"}});
    has_sel.output = "$out";
    Shape res = vfs.read<Shape>(has_sel);

    // CRITICAL EXPECTATION:
    // CRITICAL EXPECTATION:
    // - The red box (box2) is a direct component of assembly_item, so it MUST match.
    // - The red box (box1) is INSIDE the nested rigid item 'bracket'. Since has() does not recurse inside nested items, it MUST NOT match!
    // - The root 'bracket' item itself does not have a "color": "red" tag.
    // - So only box2 should be in the returned components inside the parent item!
    if (res.components.size() != 1 || res.components[0].tags.value("color", "") != "red" || res.components[0].geometry != box2.geometry) {
        std::cerr << "FAIL: has() recursed inside the nested rigid item when it should have treated it as atomic! Components size: " << res.components.size() << std::endl;
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
    test_item_ops(vfs);
    test_in_item_ops(vfs);
    test_has_atomic_items(vfs);
    
    return 0;
}
