#include "simplify_op.h"
#include "approximate_op.h"
#include "wrap_op.h"
#include "smooth_op.h"
#include "offset_op.h"

namespace jotcad {
namespace geo {
void register_features_ops(fs::VFSNode* vfs) {
    simplify_init(vfs);
    approximate_init(vfs);
    wrap_init(vfs);
    smooth_init(vfs);
    offset_init(vfs);
}
}
}
