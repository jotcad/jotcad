#include "../vfs_node.h"
#include "../cid.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../vendor/httplib.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace fs {

void VFSNode::listen() {
    httplib::Server* svr = nullptr;
    bool is_ssl = !config_.cert_path.empty() && !config_.key_path.empty();

    if (is_ssl) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        svr = new httplib::SSLServer(config_.cert_path.c_str(), config_.key_path.c_str());
        std::cout << "[VFSNode " << config_.id << "] Using SSL with cert: " << config_.cert_path << std::endl;
#else
        std::cerr << "[VFSNode " << config_.id << "] CRITICAL: SSL requested but CPPHTTPLIB_OPENSSL_SUPPORT is not defined. Falling back to HTTP." << std::endl;
        svr = new httplib::Server();
        is_ssl = false;
#endif
    } else {
        svr = new httplib::Server();
    }
    server_ptr_ = svr;

    svr->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, X-Requested-With, X-VFS-Peer-Id, X-VFS-Reply-To, X-VFS-Id, X-VFS-Info"},
        {"Access-Control-Expose-Headers", "X-VFS-Info, X-VFS-Id, X-VFS-Peer-Id"}
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
        try {
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
                    bool exists = false;
                    {
                        std::lock_guard<std::mutex> lock(peer_mutex_);
                        if (peers_.count(peerId)) exists = true;
                    }
                    if (!exists) {
                        add_connection(std::make_shared<ReverseConnection>(peerId));
                    }
                    res.set_content(json({{"id", config_.id}, {"reachability", "REVERSE"}}).dump(), "application/json");
                }
            } else {
                res.status = 400;
                res.set_content("Missing peerId", "text/plain");
            }
        } catch (const std::exception& e) {
            std::cerr << "[VFSNode " << config_.id << "] /register CRITICAL ERROR: " << e.what() << std::endl;
            res.status = 500;
            res.set_content(std::string("Internal Server Error: ") + e.what(), "text/plain");
        }
    });

    svr->Post("/read", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            VFSRequest vreq;
            if (body.contains("cid") && body["cid"].is_string()) {
                vreq.cid = body["cid"].get<std::string>();
            } else if (body.contains("selector")) {
                vreq.selector = Selector::from_json(body["selector"]);
                vreq.selector.validate(); // CRITICAL: FAIL VISIBLY
            } else {
                throw VFSException("Missing cid or selector in read request", 400);
            }

            vreq.stack = body.value("stack", std::vector<std::string>{});
            vreq.resolutionStack = body.value("resolutionStack", std::vector<std::string>{});
            if (vreq.stack.empty() && req.has_header("x-vfs-id")) {
                vreq.stack.push_back(req.get_header_value("x-vfs-id"));
            }
            vreq.expiresAt = body.value("expiresAt", 0LL);
            vreq.followLinks = body.value("followLinks", true);
            
            auto result = read_impl(vreq);
            
            // Base64 encode the metadata directly
            std::string info_str = result.metadata.dump();
            std::vector<uint8_t> info_bytes(info_str.begin(), info_str.end());
            std::string info_b64 = fs::base64_encode(info_bytes);

            res.set_header("x-vfs-info", info_b64);
            res.set_content((const char*)result.data.data(), result.data.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            json ctx = {{"vfsId", config_.id}, {"status", e.code}, {"action", "POST /read"}};
            std::string msg = e.what();
            if (msg.empty() || msg[0] != '{') msg = json({{"error", msg}}).dump();
            res.status = e.code;
            res.set_content(ctx.dump() + "\n" + msg, "application/x-jsonlines");
        } catch (const std::exception& e) {
            json ctx = {{"vfsId", config_.id}, {"status", 500}, {"action", "POST /read (std)"}};
            res.status = 500;
            res.set_content(ctx.dump() + "\n" + json({{"error", e.what()}}).dump(), "application/x-jsonlines");
        }
    });

    svr->Post("/subscribe", [this](const httplib::Request& req, httplib::Response& res) {
        std::string source_id = req.get_header_value("x-vfs-id");
        try {
            json body = json::parse(req.body);
            // Protocol Violation Check: This will throw if selector is malformed
            Selector s = Selector::from_json(body.at("selector"));
            
            std::cout << "[REST " << config_.id << "] <- SUBSCRIBE: " << s.path << " from " << (source_id.empty() ? "unknown" : source_id) << std::endl;
            subscribe(s.to_json(), body.at("expiresAt"), body.value("stack", std::vector<std::string>{}));
            res.status = 200;
        } catch (const std::exception& e) {
            json ctx = {{"vfsId", config_.id}, {"status", 500}, {"action", "POST /subscribe"}};
            res.status = 500;
            res.set_content(ctx.dump() + "\n" + json({{"error", e.what()}}).dump(), "application/x-jsonlines");
        }
    });

    svr->Post("/notify", [this](const httplib::Request& req, httplib::Response& res) {
        std::string source_id = req.get_header_value("x-vfs-id");
        try {
            json body = json::parse(req.body);
            json selector_json = body.at("selector");
            if (!selector_json.is_null()) {
                // Protocol Violation Check: This will throw if selector is malformed
                Selector s = Selector::from_json(selector_json);
                std::cout << "[REST " << config_.id << "] <- NOTIFY: " << s.path << " from " << (source_id.empty() ? "unknown" : source_id) << std::endl;
            }
            notify(selector_json, body.at("payload"), body.value("stack", std::vector<std::string>{}));
            res.status = 200;
        } catch (const std::exception& e) {
            json ctx = {{"vfsId", config_.id}, {"status", 500}, {"action", "POST /notify"}};
            res.status = 500;
            res.set_content(ctx.dump() + "\n" + json({{"error", e.what()}}).dump(), "application/x-jsonlines");
        }
    });

    svr->Post("/listen", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peer_id = req.get_header_value("x-vfs-peer-id");
        if (peer_id.empty()) {
            res.status = 400;
            res.set_content("Missing x-vfs-peer-id", "text/plain");
            return;
        }
        register_reverse_peer(peer_id, res);
    });

    int attempts = 0;
    while (server_ptr_) {
        std::cout << "[VFSNode " << config_.id << "] Internal server listening on 0.0.0.0:" << config_.port << "..." << std::endl;
        if (svr->listen("0.0.0.0", config_.port)) {
            // Success! listen() blocks until stop() is called.
            break; 
        }

        if (!server_ptr_) break; // Server was stopped during bind attempt

        attempts++;
        int delay_sec = (attempts < 10) ? 2 : 60;
        std::cerr << "[VFSNode " << config_.id << "] Bind failed (attempt " << attempts << "). Retrying in " << delay_sec << "s..." << std::endl;
        
        // Sleep in small increments to respond to stop() signal during backoff
        for (int i = 0; i < delay_sec * 2 && server_ptr_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    std::cout << "[VFSNode " << config_.id << "] Server has stopped." << std::endl;
}

void VFSNode::stop() {
    if (server_ptr_) {
        // Signal-safe shutdown: Avoid logging using config_.id here as it might be corrupted/freed
        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            for (auto const& [id, peer] : peers_) {
                auto rev = std::dynamic_pointer_cast<ReverseConnection>(peer);
                if (rev) {
                    std::lock_guard<std::mutex> rlock(rev->mutex);
                    rev->current_poll_id++; 
                    rev->cv.notify_all();
                }
            }
        }

        auto svr = (httplib::Server*)server_ptr_;
        server_ptr_ = nullptr; // Clear before stop to signal listen loop
        svr->stop();
    }
}

} // namespace fs
