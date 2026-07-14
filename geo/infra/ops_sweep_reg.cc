#ifndef CGAL_NEF_TRACEN
#define CGAL_NEF_TRACEN(t) (static_cast<void>(0))
#endif
#include <CGAL/Nef_polyhedron_3.h>
#include "extrude_op.h"
#include "spin_op.h"
#include "sweep_op.h"
#include "hull_op.h"
#include "convex_op.h"
#include "grow_op.h"
#include "rule_op.h"
#include "fill_op.h"
#include "dilate_op.h"

namespace jotcad {
namespace geo {
void register_sweep_ops(fs::VFSNode* vfs) {
    extrude_init(vfs);
    spin_init(vfs);
    sweep_init(vfs);
    hull_init(vfs);
    convex_init(vfs);
    grow_init(vfs);
    rule_init(vfs);
    fill_init(vfs);
    dilate_init(vfs);
}
}
}
