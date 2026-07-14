#include "undercut_op.h"
#include "part_line_op.h"
#include "section_op.h"

namespace jotcad {
namespace geo {
void register_tooling_ops(fs::VFSNode* vfs) {
    undercut_init(vfs);
    part_line_init(vfs);
    section_init(vfs);
}
}
}
