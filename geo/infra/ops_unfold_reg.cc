#include "unfold_op.h"
#include "pack_op.h"

namespace jotcad {
namespace geo {
void register_unfold_ops(fs::VFSNode* vfs) {
    unfold_init(vfs);
    pack_init(vfs);
}
}
}
