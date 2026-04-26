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

Selector VFSNode::write(const Selector& sel, const std::vector<uint8_t>& data) { 
    write_bytes(sel, data); 
    return sel;
}

Selector VFSNode::write(const Selector& sel, const json& data) {
    std::string text = data.dump();
    std::vector<uint8_t> bytes(text.begin(), text.end());
    write_bytes(sel, bytes);
    return sel;
}

Selector VFSNode::write(const Selector& sel, const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    write_bytes(sel, bytes);
    return sel;
}

template<> CID VFSNode::materialize<json>(const json& data) {
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

template<> CID VFSNode::materialize<std::string>(const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return materialize(bytes);
}

} // namespace fs
