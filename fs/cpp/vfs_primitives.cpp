#include "vfs_node.h"
#include "cid.h"
#include <fstream>
#include <filesystem>

namespace fs {

// --- read(Selector) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel) {
    VFSRequest req; req.selector = sel;
    return read_impl(req).data;
}

template<> json VFSNode::read<json>(const Selector& sel) {
    VFSRequest req; req.selector = sel;
    auto res = read_impl(req);
    if (res.data.empty()) return json::object();
    try {
        return json::parse(res.data);
    } catch (...) {
        return json::object();
    }
}

template<> double VFSNode::read<double>(const Selector& sel) {
    auto data = read<json>(sel);
    if (data.is_number()) return data.get<double>();
    return 0.0;
}

template<> int VFSNode::read<int>(const Selector& sel) {
    auto data = read<json>(sel);
    if (data.is_number()) return data.get<int>();
    return 0;
}

// --- read(VFSRequest) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const VFSRequest& req) {
    return read_impl(req).data;
}

template<> json VFSNode::read<json>(const VFSRequest& req) {
    auto res = read_impl(req);
    if (res.data.empty()) return json::object();
    try {
        return json::parse(res.data);
    } catch (...) {
        return json::object();
    }
}

template<> double VFSNode::read<double>(const VFSRequest& req) {
    auto data = read<json>(req);
    if (data.is_number()) return data.get<double>();
    return 0.0;
}

template<> int VFSNode::read<int>(const VFSRequest& req) {
    auto data = read<json>(req);
    if (data.is_number()) return data.get<int>();
    return 0;
}

// --- read(CID) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const CID& cid) {
    VFSRequest req; req.cid = cid.value;
    return read_impl(req).data;
}

template<> json VFSNode::read<json>(const CID& cid) {
    VFSRequest req; req.cid = cid.value;
    auto res = read_impl(req);
    if (res.data.empty()) return json::object();
    try {
        return json::parse(res.data);
    } catch (...) {
        return json::object();
    }
}

template<> double VFSNode::read<double>(const CID& cid) {
    auto data = read<json>(cid);
    if (data.is_number()) return data.get<double>();
    return 0.0;
}

template<> int VFSNode::read<int>(const CID& cid) {
    auto data = read<json>(cid);
    if (data.is_number()) return data.get<int>();
    return 0;
}

// --- write implementations ---

Selector VFSNode::write(const Selector& sel, const std::vector<uint8_t>& data) { 
    write_bytes(sel, data); 
    return sel;
}

Selector VFSNode::write(const Selector& sel, const json& data) {
    std::string text = data.dump();
    std::vector<uint8_t> bytes(text.begin(), text.end());
    
    // Explicitly set encoding to json for JSON writes
    std::string cid = get_cid(sel);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)bytes.data(), bytes.size());

        json meta = {
            {"state", "AVAILABLE"},
            {"encoding", "json"},
            {"selector", sel.to_json()}
        };
        std::ofstream mos(mp);
        mos << meta.dump();
    }

    notify(sel.to_json(), {{"state", "AVAILABLE"}});
    return sel;
}

Selector VFSNode::write(const Selector& sel, const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    
    // Explicitly set encoding to string for string writes
    std::string cid = get_cid(sel);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)bytes.data(), bytes.size());

        json meta = {
            {"state", "AVAILABLE"},
            {"encoding", "string"},
            {"selector", sel.to_json()}
        };
        std::ofstream mos(mp);
        mos << meta.dump();
    }

    notify(sel.to_json(), {{"state", "AVAILABLE"}});
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
        
        json meta = {{"state", "AVAILABLE"}, {"encoding", "json"}};
        std::ofstream mos(mp);
        mos << meta.dump();
    }
    
    return CID{cid_str};
}

template<> CID VFSNode::materialize<std::string>(const std::string& data) {
    std::string cid_str = vfs_hash256_str(data);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid_str + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid_str + ".meta");
    
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write(data.data(), data.size());
        
        json meta = {{"state", "AVAILABLE"}, {"encoding", "string"}};
        std::ofstream mos(mp);
        mos << meta.dump();
    }
    
    return CID{cid_str};
}

} // namespace fs
