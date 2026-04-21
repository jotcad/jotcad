#include "../include/vfs_node.h"
#include "../include/cid.h"

namespace fs {

// --- 2-ARG SPECIALIZATIONS (Define first to prevent implicit instantiation by 1-arg versions) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel, const std::string& output) {
    return read_impl(sel, 0, {}, output);
}

template<> json VFSNode::read<json>(const Selector& sel, const std::string& output) {
    auto data = read_impl(sel, 0, {}, output);
    try {
        return decode_jcb(data);
    } catch (const JCBParseException& e) {
        std::string text(data.begin(), data.end());
        return json::parse(text);
    }
}

template<> Selector VFSNode::write<std::vector<uint8_t>>(const Selector& sel, const std::vector<uint8_t>& data, const std::string& output) { 
    return write_bytes(sel, data, output); 
}

template<> Selector VFSNode::write<json>(const Selector& sel, const json& data, const std::string& output) {
    return write_bytes(sel, encode_jcb(data), output);
}

template<> Selector VFSNode::write<std::string>(const Selector& sel, const std::string& data, const std::string& output) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return write_bytes(sel, bytes, output);
}

// --- 1-ARG SPECIALIZATIONS ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel) {
    return read_impl(sel);
}

template<> json VFSNode::read<json>(const Selector& sel) {
    return this->read<json>(sel, "$out");
}

template<> Selector VFSNode::write<std::vector<uint8_t>>(const Selector& sel, const std::vector<uint8_t>& data) { 
    return write_bytes(sel, data); 
}

template<> Selector VFSNode::write<json>(const Selector& sel, const json& data) {
    return write_bytes(sel, encode_jcb(data));
}

template<> Selector VFSNode::write<std::string>(const Selector& sel, const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return write_bytes(sel, bytes);
}

} // namespace fs
