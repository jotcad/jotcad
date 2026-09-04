#include "vfs_node.h"
#include "cid.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs {

// --- read(Selector) ---
static VFSNode::VFSRequest make_context_selector_request(const Selector& sel) {
    VFSNode::VFSRequest req;
    req.selector = sel;
    req.op = "READ_SELECTOR";
    const auto* ctx = VFSNode::get_current_request_context();
    if (ctx) {
        req.timeoutMs = ctx->timeoutMs;
        req.stack = ctx->stack;
        req.resolutionStack = ctx->resolutionStack;
    }
    return req;
}

template <> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel) {
    VFSRequest req = make_context_selector_request(sel);
    return read_selector_impl(req).data;
}

template <> json VFSNode::read<json>(const Selector& sel) {
    VFSRequest req = make_context_selector_request(sel);
    auto res = read_selector_impl(req);
    if (res.data.empty()) throw VFSException("Empty payload reading json for Selector: " + sel.path, 404);
    std::string enc = res.metadata.value("encoding", "");
    if (enc != "json") {
        throw VFSException("Encoding mismatch reading json for Selector: " + sel.path + " (expected 'json', found '" + enc + "')", 400);
    }
    try {
        return json::parse(res.data);
    } catch (const std::exception& e) {
        throw VFSException("Invalid JSON payload reading Selector: " + sel.path + " (" + e.what() + ")", 500);
    }
}

template<> double VFSNode::read<double>(const Selector& sel) {
    auto j = read<json>(sel);
    if (j.is_number()) return j.get<double>();
    throw VFSException("Expected number reading double for Selector: " + sel.path, 400);
}

template<> int VFSNode::read<int>(const Selector& sel) {
    auto j = read<json>(sel);
    if (j.is_number()) return j.get<int>();
    throw VFSException("Expected number reading int for Selector: " + sel.path, 400);
}

template<> std::string VFSNode::read<std::string>(const Selector& sel) {
    VFSRequest req = make_context_selector_request(sel);
    auto res = read_selector_impl(req);
    if (res.data.empty()) throw VFSException("Empty payload reading string for Selector: " + sel.path, 404);
    std::string enc = res.metadata.value("encoding", "");
    if (enc != "string") {
        throw VFSException("Encoding mismatch reading string for Selector: " + sel.path + " (expected 'string', found '" + enc + "')", 400);
    }
    return std::string(res.data.begin(), res.data.end());
}

template<> VFSResult VFSNode::read<VFSResult>(const Selector& sel) {
    VFSRequest req = make_context_selector_request(sel);
    return read_selector_impl(req);
}

// --- read(VFSRequest) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const VFSRequest& req) {
    return req.is_cid() ? read_cid_impl(req).data : read_selector_impl(req).data;
}

template<> json VFSNode::read<json>(const VFSRequest& req) {
    auto res = req.is_cid() ? read_cid_impl(req) : read_selector_impl(req);
    if (res.data.empty()) throw VFSException("Empty payload reading json for request", 404);
    std::string enc = res.metadata.value("encoding", "");
    if (enc != "json") {
        throw VFSException("Encoding mismatch reading json for request (expected 'json', found '" + enc + "')", 400);
    }
    try {
        return json::parse(res.data);
    } catch (const std::exception& e) {
        throw VFSException("Invalid JSON payload reading request (" + std::string(e.what()) + ")", 500);
    }
}

template<> double VFSNode::read<double>(const VFSRequest& req) {
    auto j = read<json>(req);
    if (j.is_number()) return j.get<double>();
    throw VFSException("Expected number reading double for request", 400);
}

template<> int VFSNode::read<int>(const VFSRequest& req) {
    auto j = read<json>(req);
    if (j.is_number()) return j.get<int>();
    throw VFSException("Expected number reading int for request", 400);
}

template<> std::string VFSNode::read<std::string>(const VFSRequest& req) {
    auto res = req.is_cid() ? read_cid_impl(req) : read_selector_impl(req);
    if (res.data.empty()) throw VFSException("Empty payload reading string for request", 404);
    std::string enc = res.metadata.value("encoding", "");
    if (enc != "string") {
        throw VFSException("Encoding mismatch reading string for request (expected 'string', found '" + enc + "')", 400);
    }
    return std::string(res.data.begin(), res.data.end());
}

template<> VFSResult VFSNode::read<VFSResult>(const VFSRequest& req) {
    return req.is_cid() ? read_cid_impl(req) : read_selector_impl(req);
}

// --- read(CID) ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const CID& cid) {
    VFSRequest req; req.cid = cid.value; req.op = "READ_CID";
    return read_cid_impl(req).data;
}

template<> json VFSNode::read<json>(const CID& cid) {
    VFSRequest req; req.cid = cid.value; req.op = "READ_CID";
    auto res = read_cid_impl(req);
    if (res.data.empty()) throw VFSException("Empty payload reading json for CID: " + cid.value, 404);
    std::string enc = res.metadata.value("encoding", "");
    if (enc != "json") {
        throw VFSException("Encoding mismatch reading json for CID: " + cid.value + " (expected 'json', found '" + enc + "')", 400);
    }
    try {
        return json::parse(res.data);
    } catch (const std::exception& e) {
        throw VFSException("Invalid JSON payload reading CID: " + cid.value + " (" + e.what() + ")", 500);
    }
}

template<> double VFSNode::read<double>(const CID& cid) {
    auto j = read<json>(cid);
    if (j.is_number()) return j.get<double>();
    throw VFSException("Expected number reading double for CID: " + cid.value, 400);
}

template<> int VFSNode::read<int>(const CID& cid) {
    auto j = read<json>(cid);
    if (j.is_number()) return j.get<int>();
    throw VFSException("Expected number reading int for CID: " + cid.value, 400);
}

template<> std::string VFSNode::read<std::string>(const CID& cid) {
    VFSRequest req; req.cid = cid.value; req.op = "READ_CID";
    auto res = read_cid_impl(req);
    if (res.data.empty()) throw VFSException("Empty payload reading string for CID: " + cid.value, 404);
    std::string enc = res.metadata.value("encoding", "");
    if (enc != "string") {
        throw VFSException("Encoding mismatch reading string for CID: " + cid.value + " (expected 'string', found '" + enc + "')", 400);
    }
    return std::string(res.data.begin(), res.data.end());
}

template<> VFSResult VFSNode::read<VFSResult>(const CID& cid) {
    VFSRequest req; req.cid = cid.value; req.op = "READ_CID";
    return read_cid_impl(req);
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

template<> CID VFSNode::materialize<std::vector<uint8_t>>(const std::vector<uint8_t>& data) {
    std::string cid_str = vfs_hash256(data);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid_str + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid_str + ".meta");
    
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)data.data(), data.size());
        
        json meta = {{"state", "AVAILABLE"}, {"encoding", "bytes"}};
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
