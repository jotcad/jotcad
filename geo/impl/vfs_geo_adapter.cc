#include "vfs_node.h"
#include "geometry.h"
#include "shape.h"

namespace fs {

// --- read(Selector) ---

template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const Selector& sel) {
    auto data = read<std::vector<uint8_t>>(sel);
    std::string text(data.begin(), data.end());
    jotcad::geo::Geometry g;
    g.decode_text(text);
    return g;
}

template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const Selector& sel) {
    auto j = this->read<nlohmann::json>(sel);
    return jotcad::geo::Shape::from_json(j);
}

// --- read(Selector, output) ---

template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const Selector& sel, const std::string& output) {
    auto data = read<std::vector<uint8_t>>(sel, output);
    std::string text(data.begin(), data.end());
    jotcad::geo::Geometry g;
    g.decode_text(text);
    return g;
}

template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const Selector& sel, const std::string& output) {
    auto j = this->read<nlohmann::json>(sel, output);
    return jotcad::geo::Shape::from_json(j);
}

// --- read(VFSRequest) ---

template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const VFSRequest& req) {
    auto data = read<std::vector<uint8_t>>(req);
    std::string text(data.begin(), data.end());
    jotcad::geo::Geometry g;
    g.decode_text(text);
    return g;
}

template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const VFSRequest& req) {
    auto j = this->read<nlohmann::json>(req);
    return jotcad::geo::Shape::from_json(j);
}

// --- read(CID) ---

template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const CID& cid) {
    auto data = read<std::vector<uint8_t>>(cid);
    std::string text(data.begin(), data.end());
    jotcad::geo::Geometry g;
    g.decode_text(text);
    return g;
}

template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const CID& cid) {
    auto j = this->read<nlohmann::json>(cid);
    return jotcad::geo::Shape::from_json(j);
}

// --- write implementations ---

template<> Selector VFSNode::write<jotcad::geo::Geometry>(const Selector& sel, const jotcad::geo::Geometry& data, const std::string& output) {
    return write<std::string>(sel, data.encode_text(), output);
}

template<> Selector VFSNode::write<jotcad::geo::Shape>(const Selector& sel, const jotcad::geo::Shape& data, const std::string& output) {
    return write<json>(sel, data.to_json(), output);
}

template<> Selector VFSNode::write<jotcad::geo::Geometry>(const Selector& sel, const jotcad::geo::Geometry& data) {
    return write<std::string>(sel, data.encode_text());
}

template<> Selector VFSNode::write<jotcad::geo::Shape>(const Selector& sel, const jotcad::geo::Shape& data) {
    return write<json>(sel, data.to_json());
}

template<> CID VFSNode::write_anonymous<jotcad::geo::Geometry>(const jotcad::geo::Geometry& data) {
    return write_anonymous<std::string>(data.encode_text());
}

} // namespace fs
