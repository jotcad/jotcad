#include "hexagon_op.h"
#include "box_op.h"
#include "disk_op.h"
#include "arc_op.h"
#include "orb_op.h"
#include "cone_op.h"
#include "triangle_op.h"
#include "plane_op.h"
#include "points_op.h"
#include "path_op.h"
#include "corners_op.h"
#include "text_op.h"
#include "trace_op.h"

namespace jotcad {
namespace geo {
void register_primitives_ops(fs::VFSNode* vfs) {
    hexagon_init(vfs);
    box_init(vfs);
    disk_init(vfs);
    arc_init(vfs);
    orb_init(vfs);
    cone_init(vfs);
    triangle_init(vfs);
    plane_init(vfs);
    points_init(vfs);
    path_init(vfs);
    corners_init(vfs);
    text_init(vfs);
    trace_init(vfs);
}
}
}
