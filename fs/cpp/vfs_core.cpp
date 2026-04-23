#include "vfs_node.h"
#include "cid.h"
#include "vendor/httplib.h"
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

    svr->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, X-Requested-With, X-VFS-Peer-Id, X-VFS-Reply-To, X-VFS-Id"}
    });

    svr->Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
    });

    svr->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(json({{"status", "OK"}, {"id", config_.id}}).dump(), "application/json");
    });

    svr->Get("/catalog", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(get_catalog().dump(), "application/json");
    });

    svr->Post("/register", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peerId;
        std::string url;

        // 1. Try parsing JSON body
        if (!req.body.empty()) {
            try {
                json body = json::parse(req.body);
                if (body.contains("id")) peerId = body["id"];
                else if (body.contains("peerId")) peerId = body["peerId"];
                if (body.contains("url")) url = body["url"];
            } catch (...) {}
        }

        // 2. Try query parameters if JSON failed
        if (peerId.empty() || url.empty()) {
            auto get_param = [&](const std::string& key) {
                if (req.has_param(key)) return req.get_param_value(key);
                size_t pos = req.target.find("?");
                if (pos != std::string::npos) {
                    std::string query = req.target.substr(pos + 1);
                    size_t kpos = query.find(key + "=");
                    if (kpos != std::string::npos) {
                        size_t vpos = kpos + key.length() + 1;
                        size_t vend = query.find("&", vpos);
                        std::string val = query.substr(vpos, vend != std::string::npos ? vend - vpos : std::string::npos);
                        return val;
                    }
                }
                return std::string("");
            };
            if (peerId.empty()) {
                peerId = get_param("peerId");
                if (peerId.empty()) peerId = get_param("id");
            }
            if (url.empty()) url = get_param("url");
        }

        if (!peerId.empty()) {
            if (!url.empty()) {
                add_peer(url);
                res.set_content(json({{"id", config_.id}, {"reachability", "DIRECT"}}).dump(), "application/json");
            } else {
                // Browser or restricted peer: acknowledge ID, signal they must poll (/listen)
                res.set_content(json({{"id", config_.id}, {"reachability", "REVERSE"}}).dump(), "application/json");
            }
        } else {
            res.status = 400;
            res.set_content("Missing peerId", "text/plain");
        }
    });

    svr->Post("/read", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            Selector sel = Selector::from_json(body.at("selector"));
            std::string output = body.value("output", "");
            std::vector<std::string> stack = body.value("stack", std::vector<std::string>{});
            long long expiresAt = body.value("expiresAt", 0LL);
            
            auto data = read_impl(sel, 0, stack, output, expiresAt);
            res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            res.status = e.code;
            res.set_content(e.what(), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr->Post("/subscribe", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::cout << "[REST " << config_.id << "] POST /subscribe " << body.at("selector").at("path") << std::endl;
            subscribe(body.at("selector"), body.at("expiresAt"), body.value("stack", std::vector<std::string>{}));
            res.status = 200;
        } catch (const std::exception& e) {
            std::cerr << "[REST " << config_.id << "] /subscribe Error: " << e.what() << std::endl;
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

    svr->Post("/listen", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peer_id = req.get_header_value("x-vfs-peer-id");
        if (peer_id.empty()) {
            res.status = 400;
            res.set_content("Missing x-vfs-peer-id", "text/plain");
            return;
        }
        // std::cout << "[REST " << config_.id << "] /listen from " << peer_id << std::endl;
        register_reverse_peer(peer_id, res);
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

std::vector<uint8_t> VFSNode::read_impl(const Selector& sel, int depth, std::vector<std::string> stack, const std::string& output, long long expiresAt) {
    if (expiresAt > 0) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > expiresAt) {
            throw VFSException("Request expired", 404);
        }
    }

    // If output is provided in call, override the selector's output
    Selector target_sel = sel;
    if (!output.empty()) target_sel.output = output;

    std::string cid;
    if (target_sel.path.empty() && target_sel.parameters.contains("cid")) {
        cid = target_sel.parameters["cid"].get<std::string>();
    } else {
        cid = get_cid(target_sel);
    }
    std::cout << "[VFS DEBUG] read_impl cid: " << cid << " path: " << target_sel.path << std::endl;

    if (has_local(cid)) {
        // Check if it's a LINK in .meta
        bool isLink = false;
        Selector linkTarget;

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");
            if (std::filesystem::exists(mp)) {
                std::ifstream in(mp);
                json meta;
                in >> meta;
                if (meta.value("state", "") == "LINK" && meta.contains("target")) {
                    isLink = true;
                    linkTarget = Selector::from_json(meta["target"]);
                }
            }
        }

        if (isLink) {
            return read_impl(linkTarget, depth + 1, stack, "", expiresAt);
        }
        
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::filesystem::path dp = std::filesystem::path(config_.storage_dir) / (cid + ".data");
        if (std::filesystem::exists(dp)) {
            return get_local(cid);
        }
    }

    if (depth < 3) {
        for (const auto& neighbor : config_.neighbors) {
            bool already_in_stack = false;
            for (const auto& s : stack) if (s == config_.id) already_in_stack = true;
            if (already_in_stack) continue;

            std::vector<std::string> next_stack = stack;
            next_stack.push_back(config_.id);

            httplib::Client cli(neighbor);
            json body = {{"selector", target_sel}, {"stack", next_stack}, {"expiresAt", expiresAt}};
            auto res = cli.Post("/read", body.dump(), "application/json");
            if (res && res->status == 200) {
                std::vector<uint8_t> data(res->body.begin(), res->body.end());
                return data;
            }
        }
    }

    std::function<std::vector<uint8_t>(const VFSRequest&)> handler;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (handlers_.count(target_sel.path)) {
            handler = handlers_[target_sel.path];
        }
    }

    if (handler) {
        VFSRequest req = {target_sel, stack};
        auto data = handler(req);
        // By convention, operators fulfill their assigned selector.
        // If they were called with a specific output, they write to that.
        write_bytes(target_sel, data);
        return data;
    }

    throw VFSException("Content not found for CID: " + cid, 404);
}

std::string VFSNode::get_cid(const Selector& sel) {
    return vfs_hash256_str(encode_safe(sel.to_json()));
}

bool VFSNode::has_local(const std::string& cid) {
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");
    return std::filesystem::exists(p) || std::filesystem::exists(mp);
}

std::vector<uint8_t> VFSNode::get_local(const std::string& cid) {
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

Selector VFSNode::write_bytes(const Selector& sel, const std::vector<uint8_t>& data) {
    return write_bytes(sel, data, "");
}

Selector VFSNode::write_bytes(const Selector& sel, const std::vector<uint8_t>& data, const std::string& output) {
    Selector target_sel = sel;
    if (!output.empty()) target_sel.output = output;

    std::string cid = get_cid(target_sel);
    std::cout << "[VFS DEBUG] write_bytes cid: " << cid << " path: " << target_sel.path << std::endl;
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)data.data(), data.size());

        json meta = {
            {"state", "AVAILABLE"},
            {"path", target_sel.path},
            {"parameters", target_sel.parameters},
            {"output", target_sel.output}
        };
        std::ofstream mos(mp);
        mos << meta.dump();
    }
    
    // Formal VFS Links: If we fulfill $out, also link the base selector to it
    if (!output.empty() && output == "$out" && sel.output.empty()) {
        link(sel, target_sel);
    }

    Selector out = target_sel;
    out.parameters["cid"] = cid;
    return out;
}

template<> CID VFSNode::write_anonymous<std::vector<uint8_t>>(const std::vector<uint8_t>& data) {
    std::string cid_str = vfs_hash256(data);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid_str + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid_str + ".meta");
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    std::ofstream os(p, std::ios::binary);
    os.write((const char*)data.data(), data.size());

    json meta = {{"state", "AVAILABLE"}};
    std::ofstream mos(mp);
    mos << meta.dump();
    
    return CID{cid_str};
}

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const CID& cid) {
    if (has_local(cid.value)) {
        return get_local(cid.value);
    }
    // For now, we only support local read by CID. 
    // In a full mesh, this might trigger a mesh-wide CID search.
    throw VFSException("CID not found locally: " + cid.value, 404);
}

void VFSNode::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    if (std::find(stack.begin(), stack.end(), config_.id) != stack.end()) return;
    std::string sel_str = encode_safe(selector);
    
    std::lock_guard<std::mutex> lock(interest_mutex_);
    if (interests_.count(sel_str)) {
        auto& peers = interests_[sel_str];
        for (auto const& [peer_id, expiresAt] : peers) {
            if (std::find(stack.begin(), stack.end(), peer_id) != stack.end()) continue;

            std::string url;
            {
                std::lock_guard<std::mutex> plock(peer_mutex_);
                if (peers_.count(peer_id)) url = peers_[peer_id];
            }

            std::vector<std::string> next_stack = stack;
            next_stack.push_back(config_.id);
            json body = {{"selector", selector}, {"payload", payload}, {"stack", next_stack}};

            if (!url.empty()) {
                std::thread([url, body]() {
                    httplib::Client cli(url);
                    cli.Post("/notify", body.dump(), "application/json");
                }).detach();
            } else {
                std::lock_guard<std::mutex> rlock(reverse_mutex_);
                reverse_queues_[peer_id].push_back({{"type", "NOTIFY"}, {"selector", selector}, {"payload", payload}, {"stack", next_stack}});
            }
        }
    }
}

void VFSNode::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    std::string sel_str = encode_safe(selector);
    std::string peer_id = stack.empty() ? "unknown" : stack.back();
    
    {
        std::lock_guard<std::mutex> lock(interest_mutex_);
        interests_[sel_str][peer_id] = expiresAt;
        interest_selectors_[sel_str] = selector;
    }

    if (selector.at("path") == "sys/schema") {
        json catalog = get_catalog();
        notify(selector, {{"type", "CATALOG_ANNOUNCEMENT"}, {"catalog", catalog["catalog"]}, {"provider", config_.id}}, {});
    }
}

void VFSNode::register_reverse_peer(const std::string& peer_id, httplib::Response& res) {
    std::unique_lock<std::mutex> lock(reverse_mutex_);
    if (!reverse_queues_[peer_id].empty()) {
        json cmd = reverse_queues_[peer_id].front();
        reverse_queues_[peer_id].erase(reverse_queues_[peer_id].begin());
        res.set_content(cmd.dump(), "application/json");
        return;
    }
    res.status = 204;
}

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
