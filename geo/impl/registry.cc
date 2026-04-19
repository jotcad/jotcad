#include "protocols.h"
#include "processor.h"

namespace fs {

// Implement VFSNode::read specializations
template <>
jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const Selector& sel) {
    auto data = read_impl(sel);
    jotcad::geo::Geometry g;
    g.decode_text(std::string(data.begin(), data.end()));
    return g;
}

template <>
jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const Selector& sel) {
    auto data = read_impl(sel);
    return jotcad::geo::Shape::from_json(nlohmann::json::parse(std::string(data.begin(), data.end())));
}

// Implement VFSNode::write specializations
template <>
Selector VFSNode::write<jotcad::geo::Geometry>(const Selector& sel, const jotcad::geo::Geometry& geo) {
    if (!sel.path.empty()) {
        throw std::runtime_error("Geometry must always be content-addressed; do not provide a path to vfs->write<Geometry>.");
    }
    std::string text = geo.encode_text();
    std::vector<uint8_t> data(text.begin(), text.end());
    
    // CID calculation using the fs utility
    std::string hash = vfs_hash256(data);
    Selector artifact = {"geo/mesh", {{"cid", hash}}};
    write_bytes(artifact, data);
    return artifact;
}

template <>
Selector VFSNode::write<jotcad::geo::Shape>(const Selector& sel, const jotcad::geo::Shape& shape) {
    if (sel.path.empty()) {
        throw std::runtime_error("Shape must have an explicit address; did you forget to pass 'fulfilling' to vfs->write<Shape>?");
    }
    std::string text = shape.to_json().dump();
    std::vector<uint8_t> data(text.begin(), text.end());
    write_bytes(sel, data);
    return sel;
}

} // namespace fs

namespace jotcad {
namespace geo {

std::map<std::string, Processor::Entry>& Processor::registry_instance() {
    static std::map<std::string, Entry> inst;
    return inst;
}

} // namespace geo
} // namespace jotcad
