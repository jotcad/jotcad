#include "test_base.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("move_test");
    register_all_ops(&vfs);

    std::cout << "Testing Move Operators..." << std::endl;

    // 1. Move
    Selector box1 = Selector{"jot/Box", {{"width", 10.0}}}.with_output("$out");
    Selector move1 = Selector{"jot/move", {{"$in", box1}, {"offset", {{10.0, 20.0, 0.0}}}}}.with_output("$out");
    Processor::execute(&vfs, move1);
    Shape s1 = vfs.read<Shape>(move1);
    
    std::cout << "  - move(10, 20): " << s1.tf.to_vec() << std::endl;
    // The coefficients are: r11 r12 r13 tx  r21 r22 r23 ty  r31 r32 r33 tz  0 0 0 1
    // tx is at index 3 (4th element), ty is at index 7 (8th element)
    std::stringstream ss(s1.tf.to_vec());
    std::vector<std::string> parts;
    std::string p;
    while(ss >> p) parts.push_back(p);
    if ((parts[3] != "10/1" && parts[3] != "10") || (parts[7] != "20/1" && parts[7] != "20")) return 1;

    // 2. moveX / mx
    Selector mx1 = Selector{"jot/mx", {{"$in", box1}, {"x", {5.0}}}}.with_output("$out");
    Processor::execute(&vfs, mx1);
    Shape s2 = vfs.read<Shape>(mx1);
    std::cout << "  - mx(5): " << s2.tf.to_vec() << std::endl;
    std::stringstream ss2(s2.tf.to_vec());
    parts.clear();
    while(ss2 >> p) parts.push_back(p);
    if (parts[3] != "5/1" && parts[3] != "5") return 1;

    // 3. Linear Array (mx with multiple values)
    Selector mx_array = Selector{"jot/mx", {{"$in", box1}, {"x", {0.0, 10.0, 20.0}}}}.with_output("$out");
    Processor::execute(&vfs, mx_array);
    Shape s_array = vfs.read<Shape>(mx_array);
    std::cout << "  - mx([0, 10, 20]) components: " << s_array.components.size() << std::endl;
    if (s_array.components.size() != 3) return 1;
    if (s_array.components[2].tf.to_vec().find("20") == std::string::npos) return 1;

    // 4. m (alias)
    Selector m1 = Selector{"jot/m", {{"$in", box1}, {"offset", {{-2.0, 0.0, 0.0}}}}}.with_output("$out");
    Processor::execute(&vfs, m1);
    Shape s3 = vfs.read<Shape>(m1);
    std::cout << "  - m(x=-2): " << s3.tf.to_vec() << std::endl;
    std::stringstream ss3(s3.tf.to_vec());
    parts.clear();
    while(ss3 >> p) parts.push_back(p);
    if (parts[3] != "-2/1" && parts[3] != "-2") return 1;

    std::cout << "✨ Move Operators PASS" << std::endl;
    return 0;
}
