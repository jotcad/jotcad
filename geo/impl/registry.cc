#include "protocols.h"
#include "processor.h"

namespace fs {

// Implement VFSNode::read specializations
template <>
jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const Selector& sel) {
    auto data = read_impl(sel);
    jotcad::geo::Geometry g;
    // Geometry is terminal JOT text
    g.decode_text(std::string(data.begin(), data.end()));
    return g;
}

template <>
jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const Selector& sel) {
    // VFSNode::read<json> already handles Safe/JCB/JSON decoding
    return jotcad::geo::Shape::from_json(this->read<nlohmann::json>(sel));
}

// Implement VFSNode::write specializations
template <>
Selector VFSNode::write<jotcad::geo::Geometry>(const Selector& sel, const jotcad::geo::Geometry& geo) {
    if (!sel.path.empty()) {
        throw std::runtime_error("Geometry must always be content-addressed; do not provide a path to vfs->write<Geometry>.");
    }
    std::string text = geo.encode_text();
    // Geometry is stored as JOT text, hash the text directly
    std::vector<uint8_t> data(text.begin(), text.end());
    return write_bytes(sel, data);
}

template <>
Selector VFSNode::write<jotcad::geo::Shape>(const Selector& sel, const jotcad::geo::Shape& shape) {
    if (sel.path.empty()) {
        throw std::runtime_error("Shape must have an explicit address; did you forget to pass 'fulfilling' to vfs->write<Shape>?");
    }
    nlohmann::json j = shape.to_json();
    // VFSNode::write<json> will handle JCB encoding for us
    return write<nlohmann::json>(sel, j);
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
