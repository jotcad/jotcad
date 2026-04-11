#include "impl/processor.h"
#include "hexagon_op.h"
#include "box_op.h"
#include "triangle_op.h"
#include "offset_op.h"
#include "outline_op.h"
#include "pdf_op.h"
#include <iostream>

using namespace jotcad::geo;

int main(int argc, char** argv) {
    std::cout << "[Ops] Initializing Registry..." << std::endl;

    // Each op registers itself, its schema, and its UX
    hexagon_init();
    box_init();
    triangle_init();
    offset_init();
    outline_init();
    pdf_init();

    std::cout << "[Ops] Registry complete. Starting Comprehensive Geometry Service..." << std::endl;

    return Processor::serve(argc, argv);
}
