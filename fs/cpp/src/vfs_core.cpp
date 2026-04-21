#include "../include/vfs_node.h"
#include "../include/cid.h"
#include "../vendor/httplib.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <future>
#include <sstream>

namespace fs {

VFSNode::VFSNode(const Config& config) : config_(config) {
    if (config_.storage_dir.empty()) {
        config_.storage_dir = ".vfs_storage_" + config_.id;
    }
    std::filesystem::create_directories(config_.storage_dir);
}

VFSNode::~VFSNode() {
    stop();
}

void VFSNode::register_op(const std::string& path, OpHandler handler, const json& schema) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[path] = handler;
    schemas_[path] = schema;
}

void VFSNode::notify_schema() {
}

void VFSNode::listen() {
    auto svr = new httplib::Server();
    server_ptr_ = svr;

    svr->Get("/catalog", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(get_catalog().dump(), "application/json");
    });

    svr->Post("/read", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            Selector sel = Selector::from_json(body.at("selector"));
            std::string output = body.value("output", "$out");
            std::vector<std::string> stack = body.value("stack", std::vector<std::string>{});
            
            auto data = read_impl(sel, 0, stack, output);
            res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            res.status = e.code;
            res.set_content(e.what(), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr->Post("/notify", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            notify(body.at("selector"), body.at("payload"), body.value("stack", std::vector<std::string>{}));
            res.status = 200;
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr->listen("0.0.0.0", config_.port);
}

void VFSNode::stop() {
    if (server_ptr_) {
        auto svr = (httplib::Server*)server_ptr_;
        svr->stop();
        delete svr;
        server_ptr_ = nullptr;
    }
}

std::vector<uint8_t> VFSNode::read_impl(const Selector& sel, int depth, std::vector<std::string> stack, const std::string& output) {
    std::string cid;

    if (sel.path.empty()) {
        if (sel.parameters.contains("cid")) {
            cid = sel.parameters["cid"].get<std::string>();
        } else {
            throw VFSException("Selector has no path and no CID parameter", 400);
        }
    } else {
        std::string addrKey = get_cid(sel);
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (addrKey + ".meta");
        if (std::filesystem::exists(mp)) {
            std::ifstream in(mp);
            json meta;
            in >> meta;
            if (meta.contains("ports") && meta["ports"].contains(output)) {
                cid = meta["ports"][output].get<std::string>();
            } else if (output == "$out" && meta.contains("cid")) {
                cid = meta["cid"].get<std::string>();
            }
        }
    }

    if (!cid.empty() && has_local(cid)) {
        return get_local(cid);
    }

    if (depth < 3) {
        for (const auto& neighbor : config_.neighbors) {
            bool already_in_stack = false;
            for (const auto& s : stack) if (s == config_.id) already_in_stack = true;
            if (already_in_stack) continue;

            std::vector<std::string> next_stack = stack;
            next_stack.push_back(config_.id);

            httplib::Client cli(neighbor);
            json body = {{"selector", sel}, {"stack", next_stack}, {"output", output}};
            auto res = cli.Post("/read", body.dump(), "application/json");
            if (res && res->status == 200) {
                std::vector<uint8_t> data(res->body.begin(), res->body.end());
                return data;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (handlers_.count(sel.path)) {
            VFSRequest req = {sel, stack};
            auto data = handlers_[sel.path](req);
            write_bytes(sel, data, "$out");
            
            if (output != "$out") {
                return read_impl(sel, depth + 1, stack, output);
            }
            return data;
        }
    }

    throw VFSException("Content not found for port: " + output, 404);
}

std::string VFSNode::get_cid(const Selector& sel) {
    return vfs_hash256_str(encode_safe(sel.to_json()));
}

bool VFSNode::has_local(const std::string& cid) {
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    return std::filesystem::exists(p);
}

std::vector<uint8_t> VFSNode::get_local(const std::string& cid) {
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::ifstream in(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

Selector VFSNode::write_bytes(const Selector& sel, const std::vector<uint8_t>& data) {
    return write_bytes(sel, data, "$out");
}

Selector VFSNode::write_bytes(const Selector& sel, const std::vector<uint8_t>& data, const std::string& output) {
    std::string cid = vfs_hash256(data);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)data.data(), data.size());
    }
    if (!sel.path.empty()) {
        std::string addrKey = get_cid(sel);
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (addrKey + ".meta");
        json meta = {{"path", sel.path}, {"parameters", sel.parameters}, {"state", "AVAILABLE"}};
        if (std::filesystem::exists(mp)) {
            std::ifstream in(mp);
            json existing;
            try { in >> existing; if (existing.contains("ports")) meta["ports"] = existing["ports"]; } catch(...) {}
        }
        if (!meta.contains("ports")) meta["ports"] = json::object();
        meta["ports"][output] = cid;
        meta["cid"] = meta["ports"].contains("$out") ? meta["ports"]["$out"].get<std::string>() : cid;

        std::ofstream os(mp);
        os << meta.dump();
    }
    Selector out = sel;
    out.parameters["cid"] = cid;
    return out;
}

void VFSNode::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    std::string sel_str = encode_safe(selector);
    {
        std::lock_guard<std::mutex> lock(interest_mutex_);
        if (interests_.count(sel_str)) {
            auto& peers = interests_[sel_str];
            auto it = peers.begin();
            while (it != peers.end()) {
                std::string peer_url = it->first;
                std::thread([peer_url, selector, payload, stack, this]() {
                    std::vector<std::string> next_stack = stack;
                    next_stack.push_back(config_.id);
                    httplib::Client cli(peer_url);
                    cli.Post("/notify", json({{"selector", selector}, {"payload", payload}, {"stack", next_stack}}).dump(), "application/json");
                }).detach();
                ++it;
            }
        }
    }
}

void VFSNode::register_reverse_peer(const std::string& peer_id, httplib::Response& res) {}

json VFSNode::get_catalog() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    json catalog = json::object();
    for (auto const& [path, schema] : schemas_) {
        catalog[path] = schema;
    }
    return {{"catalog", catalog}, {"id", config_.id}};
}

json VFSNode::get_neighbors() {
    std::lock_guard<std::mutex> lock(peer_mutex_);
    json neighbors = json::array();
    for (auto const& [id, url] : peers_) {
        neighbors.push_back({{"id", id}, {"url", url}, {"reachability", "DIRECT"}});
    }
    return neighbors;
}

void VFSNode::add_peer(const std::string& url) {
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        if (connecting_.count(url)) return;
        connecting_.insert(url);
    }
    httplib::Client cli(url);
    cli.set_connection_timeout(std::chrono::seconds(1));
    if (auto res = cli.Get("/health")) {
        if (res->status == 200) {
            try {
                json body = json::parse(res->body);
                std::string id = body["id"];
                {
                    std::lock_guard<std::mutex> lock(peer_mutex_);
                    peers_[id] = url;
                }
                std::cout << "[VFSNode " << config_.id << "] Peer Added: " << id << " (" << url << ")" << std::endl;
                notify({{"path", "sys/topo"}}, {{"type", "TOPOLOGY_UPDATE"}, {"peer", config_.id}, {"neighbors", get_neighbors()}}, {});
            } catch (...) {}
        }
    }
}

void VFSNode::link(const Selector& src, const Selector& tgt) {
    // Basic link logic: create a meta file for src that points to tgt
    std::string srcKey = get_cid(src);
    std::string tgtKey = get_cid(tgt);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (srcKey + ".meta");
    json meta = {{"path", src.path}, {"parameters", src.parameters}, {"state", "LINK"}, {"target", tgt.to_json()}};
    std::ofstream os(mp);
    os << meta.dump();
}

} // namespace fs
