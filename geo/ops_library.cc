#include "hexagon_op.h"
#include "box_op.h"
#include "triangle_op.h"
#include "offset_op.h"
#include "outline_op.h"
#include "points_op.h"
#include "nth_op.h"
#include "rotate_op.h"
#include "color_op.h"
#include "group_op.h"
#include "path_op.h"
#include "corners_op.h"
#include "on_op.h"
#include "cut_op.h"

namespace jotcad {
namespace geo {

void register_all_ops() {
    hexagon_init();
    box_init();
    triangle_init();
    offset_init();
    outline_init();
    points_init();
    nth_init();
    rotate_init();
    color_init();
    group_init();
    path_init();
    corners_init();
    on_init();
    cut_init();
}

} // namespace geo
} // namespace jotcad
