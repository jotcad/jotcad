#include "conform_op.h"
#include "emboss_op.h"
#include "decal_op.h"
#include "deform_op.h"
#include "rig/rig_op.h"

namespace jotcad {
namespace geo {
void register_mapping_ops(fs::VFSNode* vfs) {
    conform_init(vfs);
    emboss_init(vfs);
    decal_init(vfs);
    deform_init(vfs);
    rig::rig_init(vfs);
}
}
}
