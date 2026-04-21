#include "vfs_node.h"
#include "geometry.h"
#include "shape.h"

namespace fs {

// --- 2-ARG SPECIALIZATIONS (Define first to prevent implicit instantiation by 1-arg versions) ---

template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const Selector& sel, const std::string& output) {
    auto data = read_impl(sel, 0, {}, output);
    std::string text(data.begin(), data.end());
    jotcad::geo::Geometry g;
    g.decode_text(text);
    return g;
}

template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const Selector& sel, const std::string& output) {
    auto j = this->read<nlohmann::json>(sel, output);
    return jotcad::geo::Shape::from_json(j);
}

template<> Selector VFSNode::write<jotcad::geo::Geometry>(const Selector& sel, const jotcad::geo::Geometry& data, const std::string& output) {
    return write<std::string>(sel, data.encode_text(), output);
}

template<> Selector VFSNode::write<jotcad::geo::Shape>(const Selector& sel, const jotcad::geo::Shape& data, const std::string& output) {
    return write<json>(sel, data.to_json(), output);
}

// --- 1-ARG SPECIALIZATIONS ---

template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const Selector& sel) {
    return this->read<jotcad::geo::Geometry>(sel, "$out");
}

template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const Selector& sel) {
    return this->read<jotcad::geo::Shape>(sel, "$out");
}

template<> Selector VFSNode::write<jotcad::geo::Geometry>(const Selector& sel, const jotcad::geo::Geometry& data) {
    return write<std::string>(sel, data.encode_text());
}

template<> Selector VFSNode::write<jotcad::geo::Shape>(const Selector& sel, const jotcad::geo::Shape& data) {
    return write<json>(sel, data.to_json());
}

} // namespace fs
