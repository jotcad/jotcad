#include "../../fs/cpp/vfs_node.h"

namespace jotcad {
namespace geo {

// Forward declarations of split registration functions
void register_primitives_ops(fs::VFSNode* vfs);
void register_booleans_ops(fs::VFSNode* vfs);
void register_transforms_ops(fs::VFSNode* vfs);
void register_features_ops(fs::VFSNode* vfs);
void register_mapping_ops(fs::VFSNode* vfs);
void register_sweep_ops(fs::VFSNode* vfs);
void register_unfold_ops(fs::VFSNode* vfs);
void register_tooling_ops(fs::VFSNode* vfs);
void register_io_ops(fs::VFSNode* vfs);

void register_all_ops(fs::VFSNode* vfs) {
    register_primitives_ops(vfs);
    register_booleans_ops(vfs);
    register_transforms_ops(vfs);
    register_features_ops(vfs);
    register_mapping_ops(vfs);
    register_sweep_ops(vfs);
    register_unfold_ops(vfs);
    register_tooling_ops(vfs);
    register_io_ops(vfs);
}

void register_ops(fs::VFSNode* vfs) {
    register_all_ops(vfs);
}

} // namespace geo
} // namespace jotcad
