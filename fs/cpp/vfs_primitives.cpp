#include "vfs_node.h"
#include "cid.h"
#include <fstream>
#include <filesystem>

namespace fs {

// --- read(Selector) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel) {
    VFSRequest req; req.selector = sel;
    return read_impl(req);
}

template<> json VFSNode::read<json>(const Selector& sel) {
    auto data = read<std::vector<uint8_t>>(sel);
    if (data.empty()) return json::object();
    try {
        return json::parse(data);
    } catch (...) {
        return json::object();
    }
}

// --- read(Selector, output) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel, const std::string& output) {
    VFSRequest req; req.selector = sel.with_output(output);
    return read_impl(req);
}

template<> json VFSNode::read<json>(const Selector& sel, const std::string& output) {
    auto data = read<std::vector<uint8_t>>(sel, output);
    if (data.empty()) return json::object();
    try {
        return json::parse(data);
    } catch (...) {
        return json::object();
    }
}

// --- read(VFSRequest) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const VFSRequest& req) {
    return read_impl(req);
}

template<> json VFSNode::read<json>(const VFSRequest& req) {
    auto data = read_impl(req);
    if (data.empty()) return json::object();
    try {
        return json::parse(data);
    } catch (...) {
        return json::object();
    }
}

// --- read(CID) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const CID& cid) {
    VFSRequest req; req.cid = cid.value;
    return read_impl(req);
}

template<> json VFSNode::read<json>(const CID& cid) {
    auto data = read<std::vector<uint8_t>>(cid);
    if (data.empty()) return json::object();
    try {
        return json::parse(data);
    } catch (...) {
        return json::object();
    }
}

// --- write implementations ---

template<> Selector VFSNode::write<std::vector<uint8_t>>(const Selector& sel, const std::vector<uint8_t>& data, const std::string& output) { 
    return write_bytes(sel, data, output); 
}

template<> Selector VFSNode::write<json>(const Selector& sel, const json& data, const std::string& output) {
    std::string text = data.dump();
    std::vector<uint8_t> bytes(text.begin(), text.end());
    return write_bytes(sel, bytes, output);
}

template<> Selector VFSNode::write<std::string>(const Selector& sel, const std::string& data, const std::string& output) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return write_bytes(sel, bytes, output);
}

template<> Selector VFSNode::write<std::vector<uint8_t>>(const Selector& sel, const std::vector<uint8_t>& data) { 
    return write_bytes(sel, data); 
}

template<> Selector VFSNode::write<json>(const Selector& sel, const json& data) {
    std::string text = data.dump();
    std::vector<uint8_t> bytes(text.begin(), text.end());
    return write_bytes(sel, bytes);
}

template<> Selector VFSNode::write<std::string>(const Selector& sel, const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return write_bytes(sel, bytes);
}

template<> CID VFSNode::write_anonymous<json>(const json& data) {
    std::string cid_str = vfs_hash256(encode_jcb(data));
    std::string text = data.dump();
    std::vector<uint8_t> bytes(text.begin(), text.end());

    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid_str + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid_str + ".meta");
    
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)bytes.data(), bytes.size());
        
        json meta = {{"state", "AVAILABLE"}, {"type", "json"}};
        std::ofstream mos(mp);
        mos << meta.dump();
    }
    
    return CID{cid_str};
}

template<> CID VFSNode::write_anonymous<std::string>(const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return write_anonymous(bytes);
}

} // namespace fs
