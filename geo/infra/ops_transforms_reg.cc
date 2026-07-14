#include "move_op.h"
#include "rotate_op.h"
#include "scale_op.h"
#include "group_op.h"
#include "at_op.h"
#include "on_op.h"
#include "snap_op.h"
#include "place_op.h"
#include "color_op.h"
#include "material_op.h"
#include "rainbow_op.h"
#include "tag_ops.h"
#include "filter_ops.h"
#include "faces_op.h"
#include "separate_op.h"
#include "measure_ops.h"
#include "sort_ops.h"
#include "bb_op.h"
#include "obb_op.h"
#include "nth_op.h"
#include "transform_ops.h"
#include "item_ops.h"

namespace jotcad {
namespace geo {
void register_transforms_ops(fs::VFSNode* vfs) {
    move_init(vfs);
    rotate_init(vfs);
    scale_init(vfs);
    group_init(vfs);
    at_init(vfs);
    on_init(vfs);
    snap_init(vfs);
    place_init(vfs);
    color_init(vfs);
    material_init(vfs);
    rainbow_init(vfs);
    tag_ops_init(vfs);
    filter_ops_init(vfs);
    faces_init(vfs);
    separate_init(vfs);
    measure_init(vfs);
    bb_init(vfs);
    obb_init(vfs);
    nth_init(vfs);
    item_init(vfs);
    transform_ops_init(vfs);
    selection_init(vfs);
}
}
}
