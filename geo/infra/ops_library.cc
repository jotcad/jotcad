#include "hexagon_op.h"
#include "box_op.h"
#include "disk_op.h"
#include "arc_op.h"
#include "orb_op.h"
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
#include "disjoint_op.h"
#include "pdf_op.h"
#include "png_op.h"
#include "rule_op.h"
#include "move_op.h"
#include "fill_op.h"
#include "sew_op.h"
#include "plane_op.h"
#include "extrude_op.h"
#include "faces_op.h"
#include "scale_op.h"
#include "spin_op.h"
#include "simplify_op.h"
#include "hull_op.h"
#include "sweep_op.h"
#include "section_op.h"
#include "place_op.h"
#include "grow_op.h"
#include "wrap_op.h"
#include "smooth_op.h"
#include "separate_op.h"
#include "deform_op.h"
#include "text_op.h"
#include "trace_op.h"
#include "transform_ops.h"
#include "asset_ops.h"
#include "stitch_op.h"

namespace jotcad {
namespace geo {

void register_all_ops(fs::VFSNode* vfs) {
    hexagon_init(vfs);
    box_init(vfs);
    disk_init(vfs);
    arc_init(vfs);
    orb_init(vfs);
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
    disjoint_init(vfs);
    pdf_init(vfs);
    png_init(vfs);
    rule_init(vfs);
    move_init(vfs);
    fill_init(vfs);
    sew_init(vfs);
    plane_init(vfs);
    extrude_init(vfs);
    faces_init(vfs);
    scale_init(vfs);
    spin_init(vfs);
    simplify_init(vfs);
    hull_init(vfs);
    sweep_init(vfs);
    section_init(vfs);
    place_init(vfs);
    grow_init(vfs);
    wrap_init(vfs);
    smooth_init(vfs);
    separate_init(vfs);
    deform_init(vfs);
    assets_init(vfs);
    text_init(vfs);
    trace_init(vfs);
    transform_ops_init(vfs);
    stitch_init(vfs);
}

} // namespace geo
} // namespace jotcad
