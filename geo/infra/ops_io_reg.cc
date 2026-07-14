#include "stl_op.h"
#include "obj_op.h"
#include "pdf_op.h"
#include "png_op.h"
#include "relief_op.h"
#include "outline_op.h"
#include "asset_ops.h"

namespace jotcad {
namespace geo {
void register_io_ops(fs::VFSNode* vfs) {
    stl_init(vfs);
    obj_init(vfs);
    pdf_init(vfs);
    png_init(vfs);
    relief_init(vfs);
    outline_init(vfs);
    assets_init(vfs);
}
}
}
