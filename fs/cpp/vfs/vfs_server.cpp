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
        {"Access-Control-Allow-Headers", "Content-Type, X-Requested-With, X-VFS-Peer-Id, X-VFS-Reply-To, X-VFS-Id, X-VFS-Info, X-VFS-Op, X-VFS-Selector, X-VFS-Encoding, X-VFS-Stack, X-VFS-Expires"},
        {"Access-Control-Expose-Headers", "X-VFS-Info, X-VFS-Id, X-VFS-Peer-Id, X-VFS-Op, X-VFS-Selector, X-VFS-Encoding, X-VFS-Stack, X-VFS-Expires, X-VFS-Reply-To"}
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

    svr->Post("/register", [this, is_ssl](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string peerId;
            std::string url;

            if (!req.body.empty()) {
                try {
                    json body = json::parse(req.body);
                    if (body.contains("id")) peerId = body["id"];
                    else if (body.contains("peerId")) peerId = body["peerId"];
                    if (body.contains("url")) url = body["url"];
                } catch (...) {}
            }

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
                            return query.substr(vpos, vend != std::string::npos ? vend - vpos : std::string::npos);
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

            json response_body = {{"id", config_.id}, {"transports", {"http", "ws"}}};
            std::string ws_proto = is_ssl ? "wss" : "ws";
            std::string host = req.get_header_value("Host");
            if (host.empty()) host = "localhost:" + std::to_string(config_.port);
            response_body["wsUrl"] = ws_proto + "://" + host + "/vfs-ws";

            if (!peerId.empty()) {
                if (!url.empty()) {
                    add_peer(url);
                    response_body["reachability"] = "DIRECT";
                } else {
                    bool exists = false;
                    {
                        std::lock_guard<std::mutex> lock(peer_mutex_);
                        if (peers_.count(peerId)) exists = true;
                    }
                    if (!exists) add_connection(std::make_shared<ReverseConnection>(peerId));
                    response_body["reachability"] = "REVERSE";
                }
                res.set_content(response_body.dump(), "application/json");
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

    svr->Post("/read_selector", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            VFSRequest vreq;
            vreq.op = "READ_SELECTOR";
            json body;
            if (!req.body.empty()) {
                try { body = json::parse(req.body); } catch (...) {}
            }

            std::string sel_raw = req.get_header_value("X-VFS-Selector");
            if (sel_raw.empty() && !body.is_null()) {
                if (body.contains("selector")) {
                    if (body["selector"].is_string()) sel_raw = body["selector"].get<std::string>();
                    else sel_raw = body["selector"].dump();
                }
            }
            try { vreq.selector = Selector::from_json(json::parse(sel_raw)); } catch (...) {
                try { vreq.selector = Selector::from_json(decode_jcb(base64_decode(sel_raw))); } catch (...) {}
            }
            vreq.selector.validate();

            std::string stack_str = req.get_header_value("X-VFS-Stack");
            if (!stack_str.empty()) {
                std::stringstream ss(stack_str);
                std::string item;
                while (std::getline(ss, item, ',')) vreq.stack.push_back(item);
            } else if (!body.is_null() && body.contains("stack")) {
                vreq.stack = body["stack"].get<std::vector<std::string>>();
            }
            if (vreq.stack.empty() && req.has_header("x-vfs-id")) vreq.stack.push_back(req.get_header_value("x-vfs-id"));

            std::string expires_str = req.get_header_value("X-VFS-Expires");
            if (!expires_str.empty()) vreq.expiresAt = std::stoll(expires_str);
            else if (!body.is_null() && body.contains("expiresAt")) vreq.expiresAt = body["expiresAt"].get<long long>();
            
            vreq.followLinks = true;
            
            auto result = read_selector_impl(vreq);
            
            res.set_header("x-vfs-info", result.metadata.dump());
            res.set_header("X-VFS-Encoding", result.metadata.value("encoding", "json"));
            res.set_content((const char*)result.data.data(), result.data.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            json ctx = {{"vfsId", config_.id}, {"status", e.code}, {"action", "POST /read_selector"}};
            res.status = e.code;
            res.set_content(ctx.dump() + "\n" + json({{"error", e.what()}}).dump(), "application/x-jsonlines");
        } catch (const std::exception& e) {
            json ctx = {{"vfsId", config_.id}, {"status", 500}, {"action", "POST /read_selector (std)"}};
            res.status = 500;
            res.set_content(ctx.dump() + "\n" + json({{"error", e.what()}}).dump(), "application/x-jsonlines");
        }
    });

    svr->Post("/read_cid", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            VFSRequest vreq;
            vreq.op = "READ_CID";
            json body = json::parse(req.body);
            if (body.contains("cid")) vreq.cid = body["cid"].get<std::string>();
            else throw VFSException("Missing cid in read_cid request", 400);
            vreq.resolutionStack = body.value("resolutionStack", std::vector<std::string>{});

            std::string stack_str = req.get_header_value("X-VFS-Stack");
            if (!stack_str.empty()) {
                std::stringstream ss(stack_str);
                std::string item;
                while (std::getline(ss, item, ',')) vreq.stack.push_back(item);
            } else if (body.contains("stack")) {
                vreq.stack = body["stack"].get<std::vector<std::string>>();
            }
            if (vreq.stack.empty() && req.has_header("x-vfs-id")) vreq.stack.push_back(req.get_header_value("x-vfs-id"));

            std::string expires_str = req.get_header_value("X-VFS-Expires");
            if (!expires_str.empty()) vreq.expiresAt = std::stoll(expires_str);
            else if (body.contains("expiresAt")) vreq.expiresAt = body["expiresAt"].get<long long>();
            
            vreq.followLinks = true;
            
            auto result = read_cid_impl(vreq);
            
            res.set_header("x-vfs-info", result.metadata.dump());
            res.set_header("X-VFS-Encoding", result.metadata.value("encoding", "json"));
            res.set_content((const char*)result.data.data(), result.data.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            json ctx = {{"vfsId", config_.id}, {"status", e.code}, {"action", "POST /read_cid"}};
            res.status = e.code;
            res.set_content(ctx.dump() + "\n" + json({{"error", e.what()}}).dump(), "application/x-jsonlines");
        } catch (const std::exception& e) {
            json ctx = {{"vfsId", config_.id}, {"status", 500}, {"action", "POST /read_cid (std)"}};
            res.status = 500;
            res.set_content(ctx.dump() + "\n" + json({{"error", e.what()}}).dump(), "application/x-jsonlines");
        }
    });

    svr->Post("/subscribe", [this](const httplib::Request& req, httplib::Response& res) {
        std::string source_id = req.get_header_value("x-vfs-id");
        try {
            Selector s;
            long long expiresAt = 0;
            std::vector<std::string> stack;

            if (req.has_header("X-VFS-Selector")) {
                std::string sel_raw = req.get_header_value("X-VFS-Selector");
                try { s = Selector::from_json(json::parse(sel_raw)); } catch (...) {
                    try { s = Selector::from_json(decode_jcb(base64_decode(sel_raw))); } catch (...) {}
                }
                std::string exp_str = req.get_header_value("X-VFS-Expires");
                if (!exp_str.empty()) expiresAt = std::stoll(exp_str);
                std::string stack_str = req.get_header_value("X-VFS-Stack");
                if (!stack_str.empty()) {
                    std::stringstream ss(stack_str);
                    std::string item;
                    while (std::getline(ss, item, ',')) stack.push_back(item);
                }
            } else {
                json body = json::parse(req.body);
                s = Selector::from_json(body.at("selector"));
                expiresAt = body.at("expiresAt");
                stack = body.value("stack", std::vector<std::string>{});
            }
            
            std::cout << "[REST " << config_.id << "] <- SUBSCRIBE: " << s.path << " from " << (source_id.empty() ? "unknown" : source_id) << std::endl;
            subscribe(s.to_json(), expiresAt, stack);
            res.status = 200;
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr->Post("/notify", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json selector_json;
            json payload;
            std::vector<std::string> stack;

            if (req.has_header("X-VFS-Selector")) {
                std::string sel_raw = req.get_header_value("X-VFS-Selector");
                Selector s;
                try { s = Selector::from_json(json::parse(sel_raw)); } catch (...) {
                    try { s = Selector::from_json(decode_jcb(base64_decode(sel_raw))); } catch (...) {}
                }
                selector_json = s.to_json();
                
                std::string stack_str = req.get_header_value("X-VFS-Stack");
                if (!stack_str.empty()) {
                    std::stringstream ss(stack_str);
                    std::string item;
                    while (std::getline(ss, item, ',')) stack.push_back(item);
                }

                std::string encoding = req.get_header_value("X-VFS-Encoding");
                if (encoding == "bytes") payload = std::vector<uint8_t>(req.body.begin(), req.body.end());
                else payload = json::parse(req.body);
            } else {
                json body = json::parse(req.body);
                selector_json = body.at("selector");
                payload = body.at("payload");
                stack = body.value("stack", std::vector<std::string>{});
            }

            if (!selector_json.is_null()) {
                Selector s = Selector::from_json(selector_json);
                std::cout << "[REST " << config_.id << "] <- NOTIFY: " << s.path << std::endl;
            }
            notify(selector_json, payload, stack);
            res.status = 200;
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json({{"error", e.what()}}).dump(), "application/json");
        }
    });

    svr->Post("/listen", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peer_id = req.get_header_value("x-vfs-peer-id");
        if (peer_id.empty()) { res.status = 400; res.set_content("Missing x-vfs-peer-id", "text/plain"); return; }
        register_reverse_peer(peer_id, res);
    });

    svr->WebSocket("/vfs-ws", [this](const httplib::Request&, httplib::ws::WebSocket& ws) {
        bool identified = false;
        std::string peerId;
        json expecting_binary_header;

        std::string msg;
        while (true) {
            auto res = ws.read(msg);
            if (res == httplib::ws::ReadResult::Fail) break;

            if (res == httplib::ws::ReadResult::Binary) {
                if (expecting_binary_header.empty()) continue;
                std::vector<uint8_t> data(msg.begin(), msg.end());
                handle_binary_frame(expecting_binary_header, data);
                expecting_binary_header.clear();
                continue;
            }

            try {
                json frame = json::parse(msg);

                std::string type = frame.value("type", "");

                if (!identified) {
                    if (type == "IDENTIFY" && frame.contains("peerId")) {
                        peerId = frame["peerId"];
                        identified = true;
                        ws.send(json({{"type", "ACK"}, {"peerId", config_.id}}).dump());
                        auto ws_conn = std::make_shared<WSReverseConnection>(peerId, &ws);
                        ws_conn->node = this;
                        upgrade_peer_to_ws(peerId, ws_conn);
                    } else { ws.close(); break; }
                    continue;
                }

                if (frame.value("hasBinary", false)) {
                    expecting_binary_header = frame;
                } else {
                    handle_ws_frame(frame);
                }
            } catch (const std::exception& e) {
                std::cerr << "[WS " << config_.id << "] Frame error: " << e.what() << std::endl;
            }
        }
    });

    while (server_ptr_) {
        std::cout << "[VFSNode " << config_.id << "] Internal server listening on 0.0.0.0:" << config_.port << "..." << std::endl;
        if (svr->listen("0.0.0.0", config_.port)) break;
        if (!server_ptr_) break;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    std::cout << "[VFSNode " << config_.id << "] Server has stopped." << std::endl;
}

void VFSNode::stop() {
    if (server_ptr_) {
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
        server_ptr_ = nullptr;
        svr->stop();
    }
}

} // namespace fs
