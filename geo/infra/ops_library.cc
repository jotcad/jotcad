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
#include "at_op.h"
#include "cut_op.h"
#include "join_op.h"
#include "clip_op.h"
#include "fuse_op.h"
#include "pdf_op.h"
#include "png_op.h"
#include "rule_op.h"
#include "move_op.h"

namespace jotcad {
namespace geo {

void register_all_ops(fs::VFSNode* vfs) {
    hexagon_init(vfs);
    box_init(vfs);
    triangle_init(vfs);
    offset_init(vfs);
    outline_init(vfs);
    points_init(vfs);
    nth_init(vfs);
    rotate_init(vfs);
    color_init(vfs);
    group_init(vfs);
    path_init(vfs);
    corners_init(vfs);
    on_init(vfs);
    at_init(vfs);
    cut_init(vfs);
    join_init(vfs);
    clip_init(vfs);
    fuse_init(vfs);
    pdf_init(vfs);
    png_init(vfs);
    rule_init(vfs);
    move_init(vfs);
}

} // namespace geo
} // namespace jotcad
