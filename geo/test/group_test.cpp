#include "test_base.h"
#include "../group_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Group..." << std::endl;
    MockVFS vfs;
    
    Shape s1; s1.tags["id"] = 1;
    Shape s2; s2.tags["id"] = 2;
    
    Shape out;
    GroupOp<>::execute(&vfs, {s1}, {s2}, out);
    
    assert(out.components.size() == 2);
    assert(out.components[0].tags["id"] == 1);
    assert(out.components[1].tags["id"] == 2);
    
    std::cout << "✅ Group PASS" << std::endl;
    return 0;
}
