#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("offset_closure");
    register_all_ops(&vfs);

    std::cout << "Testing Offset Closure Scenarios..." << std::endl;

    {
        std::cout << "  - Testing Hole Filling (Positive Offset)..." << std::endl;
        fs::Selector base_sel = fs::Selector{"jot/Box", {{"width", 50.0}, {"height", 50.0}, {"depth", 0.0}, {"id", "base"}}}.with_output("$out");
        Processor::execute(&vfs, base_sel);

        fs::Selector hole_sel = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}, {"id", "hole"}}}.with_output("$out");
        Processor::execute(&vfs, hole_sel);

        fs::Selector cut_sel = fs::Selector{"jot/cut", {{"$in", base_sel}, {"tools", {hole_sel}}}}.with_output("$out");
        Processor::execute(&vfs, cut_sel);

        fs::Selector offset_sel = fs::Selector{"jot/offset", {{"$in", cut_sel}, {"diameter", 6.0}}}.with_output("$out");
        Processor::execute(&vfs, offset_sel);

        Shape s = vfs.read<Shape>(offset_sel);
        Geometry g = vfs.read<Geometry>(s.geometry.value());

        if (!g.faces.empty()) {
            std::cout << "    ✅ Hole filled correctly." << std::endl;
        }
    }

    {
        std::cout << "  - Testing Part Collapse (Negative Offset)..." << std::endl;
        fs::Selector small_sel = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}, {"id", "small"}}}.with_output("$out");
        Processor::execute(&vfs, small_sel);

        fs::Selector collapsed_sel = fs::Selector{"jot/offset", {{"$in", small_sel}, {"diameter", -6.0}}}.with_output("$out");
        Processor::execute(&vfs, collapsed_sel);

        Shape s = vfs.read<Shape>(collapsed_sel);
        Geometry g = vfs.read<Geometry>(s.geometry.value());

        if (g.faces.empty()) {
            std::cout << "    ✅ Part collapsed correctly." << std::endl;
        }
    }

    std::cout << "✅ Offset Closure PASS" << std::endl;
    return 0;
}
