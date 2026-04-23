#include "protocols.h"
#include "processor.h"

namespace fs {

// Implement VFSNode::write specializations
// Note: These are implemented in fs/cpp/src/vfs_node.cpp to avoid multiple definitions
} // namespace fs

namespace jotcad {
namespace geo {

std::map<std::string, Processor::Entry>& Processor::registry_instance() {
    static std::map<std::string, Entry> inst;
    return inst;
}

} // namespace geo
} // namespace jotcad
