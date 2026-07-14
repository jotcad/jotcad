#include "cut_op.h"
#include "stamp_op.h"
#include "join_op.h"
#include "clip_op.h"
#include "fuse_op.h"
#include "disjoint_op.h"
#include "clean_op.h"
#include "sew_op.h"
#include "stitch_op.h"

namespace jotcad {
namespace geo {
void register_booleans_ops(fs::VFSNode* vfs) {
    cut_init(vfs);
    stamp_init(vfs);
    join_init(vfs);
    clip_init(vfs);
    fuse_init(vfs);
    disjoint_init(vfs);
    clean_init(vfs);
    sew_init(vfs);
    stitch_init(vfs);
}
}
}
