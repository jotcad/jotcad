#include "test_base.h"
#include <iostream>

using namespace jotcad::geo;

int main() {
    MockVFS vfs("repro_pdf");
    register_all_ops(&vfs);

    // 1. Create a loop of 3 points
    Shape p1; p1.tf = Matrix::translate(0, 0, 0);
    Shape p2; p2.tf = Matrix::translate(10, 0, 0);
    Shape p3; p3.tf = Matrix::translate(0, 10, 0);
    
    Shape group;
    group.components = {p1, p2, p3};
    
    fs::Selector loop_sel = fs::Selector{"jot/loop", {{"$in", group}}}.with_output("$out");
    Processor::execute(&vfs, loop_sel);
    Shape loop_shape = vfs.read<Shape>(loop_sel);
    
    // Verify loop has segments
    Geometry loop_geo = vfs.read<Geometry>(loop_shape.geometry.value());
    std::cout << "Loop segments: " << loop_geo.segments.size() << std::endl;
    
    // 2. Offset
    fs::Selector offset_sel = fs::Selector{"jot/offset", {{"$in", loop_shape}, {"diameter", 5.0}}}.with_output("$out");
    Processor::execute(&vfs, offset_sel);
    Shape offset_shape = vfs.read<Shape>(offset_sel);
    
    // Verify offset geometry
    if (offset_shape.geometry.has_value()) {
        Geometry offset_geo = vfs.read<Geometry>(offset_shape.geometry.value());
        std::cout << "Offset faces: " << offset_geo.faces.size() << std::endl;
        std::cout << "Offset segments: " << offset_geo.segments.size() << std::endl;
        std::cout << "Offset vertices: " << offset_geo.vertices.size() << std::endl;
    } else {
        std::cout << "Offset has NO geometry value" << std::endl;
    }

    // 3. PDF
    fs::Selector pdf_sel = fs::Selector{"jot/pdf", {{"$in", offset_shape}, {"path", "test.pdf"}}}.with_output("$out");
    Processor::execute(&vfs, pdf_sel);
    std::vector<uint8_t> pdf = vfs.read<std::vector<uint8_t>>(pdf_sel);
    
    std::cout << "PDF size: " << pdf.size() << " bytes" << std::endl;
    if (!pdf.empty()) {
        std::cout << "PDF Header: " << std::string((char*)pdf.data(), std::min((size_t)5, pdf.size())) << std::endl;
    }

    return 0;
}
