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

            std::vector<uint8_t> req_body(req.body.begin(), req.body.end());
            json rec_header;
            std::vector<uint8_t> rec_payload;
            if (!decode_record(req_body, rec_header, rec_payload)) {
                throw VFSException("Malformed VfsRecord", 400);
            }
            vreq.selector = Selector::from_json(rec_header.at("selector"));
            vreq.selector.validate();
            vreq.stack = rec_header.value("stack", std::vector<std::string>{});
            vreq.expiresAt = rec_header.value("expiresAt", 0LL);
            
            vreq.followLinks = true;
            
            auto result = read_selector_impl(vreq);
            
            json resp_header = {
                {"info", result.metadata},
                {"encoding", result.metadata.value("encoding", "json")}
            };
            std::vector<uint8_t> record_bytes = encode_record(resp_header, result.data);
            res.status = 200;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            json ctx = {{"vfsId", config_.id}, {"status", e.code}, {"action", "read_selector"}};
            std::string msg = e.what();
            if (msg.empty() || msg[0] != '{') msg = json({{"error", msg}}).dump();
            std::string full_error = ctx.dump() + "\n" + msg;

            json resp_header = {
                {"info", json({{"error", full_error}, {"state", "ERROR"}})},
                {"encoding", "json"}
            };
            std::vector<uint8_t> record_bytes = encode_record(resp_header);
            res.status = e.code;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        } catch (const std::exception& e) {
            json ctx = {{"vfsId", config_.id}, {"status", 500}, {"action", "read_selector (std)"}};
            std::string full_error = ctx.dump() + "\n" + json({{"error", e.what()}}).dump();

            json resp_header = {
                {"info", json({{"error", full_error}, {"state", "ERROR"}})},
                {"encoding", "json"}
            };
            std::vector<uint8_t> record_bytes = encode_record(resp_header);
            res.status = 500;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        }
    });

    svr->Post("/read_cid", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            VFSRequest vreq;
            vreq.op = "READ_CID";

            std::vector<uint8_t> req_body(req.body.begin(), req.body.end());
            json rec_header;
            std::vector<uint8_t> rec_payload;
            if (!decode_record(req_body, rec_header, rec_payload)) {
                throw VFSException("Malformed VfsRecord", 400);
            }
            vreq.cid = rec_header.at("cid").get<std::string>();
            vreq.resolutionStack = rec_header.value("resolutionStack", std::vector<std::string>{});
            vreq.stack = rec_header.value("stack", std::vector<std::string>{});
            vreq.expiresAt = rec_header.value("expiresAt", 0LL);
            
            vreq.followLinks = true;
            
            auto result = read_cid_impl(vreq);
            
            json resp_header = {
                {"info", result.metadata},
                {"encoding", result.metadata.value("encoding", "json")}
            };
            std::vector<uint8_t> record_bytes = encode_record(resp_header, result.data);
            res.status = 200;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            json ctx = {{"vfsId", config_.id}, {"status", e.code}, {"action", "read_cid"}};
            std::string msg = e.what();
            if (msg.empty() || msg[0] != '{') msg = json({{"error", msg}}).dump();
            std::string full_error = ctx.dump() + "\n" + msg;

            json resp_header = {
                {"info", json({{"error", full_error}, {"state", "ERROR"}})},
                {"encoding", "json"}
            };
            std::vector<uint8_t> record_bytes = encode_record(resp_header);
            res.status = e.code;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        } catch (const std::exception& e) {
            json ctx = {{"vfsId", config_.id}, {"status", 500}, {"action", "read_cid (std)"}};
            std::string full_error = ctx.dump() + "\n" + json({{"error", e.what()}}).dump();

            json resp_header = {
                {"info", json({{"error", full_error}, {"state", "ERROR"}})},
                {"encoding", "json"}
            };
            std::vector<uint8_t> record_bytes = encode_record(resp_header);
            res.status = 500;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        }
    });

    svr->Post("/subscribe", [this](const httplib::Request& req, httplib::Response& res) {
        std::string source_id = req.get_header_value("x-vfs-id");
        try {
            Selector s;
            long long expiresAt = 0;
            std::vector<std::string> stack;

            std::vector<uint8_t> req_body(req.body.begin(), req.body.end());
            json rec_header;
            std::vector<uint8_t> rec_payload;
            if (!decode_record(req_body, rec_header, rec_payload)) {
                throw VFSException("Malformed VfsRecord", 400);
            }
            s = Selector::from_json(rec_header.at("selector"));
            expiresAt = rec_header.value("expiresAt", 0LL);
            stack = rec_header.value("stack", std::vector<std::string>{});
            
            std::cout << "[REST " << config_.id << "] <- SUB: " << s.path << " from " << (source_id.empty() ? "unknown" : source_id) << std::endl;
            subscribe(s.to_json(), expiresAt, stack);
            
            json resp_header = {{"info", {{"state", "AVAILABLE"}}}, {"encoding", "json"}};
            std::vector<uint8_t> record_bytes = encode_record(resp_header);
            res.status = 200;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        } catch (const std::exception& e) {
            json resp_header = {
                {"info", json({{"error", e.what()}, {"state", "ERROR"}})},
                {"encoding", "json"}
            };
            std::vector<uint8_t> record_bytes = encode_record(resp_header);
            res.status = 500;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        }
    });

    svr->Post("/notify", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json selector_json;
            json payload;
            std::vector<std::string> stack;

            std::vector<uint8_t> req_body(req.body.begin(), req.body.end());
            json rec_header;
            std::vector<uint8_t> rec_payload;
            if (!decode_record(req_body, rec_header, rec_payload)) {
                throw VFSException("Malformed VfsRecord", 400);
            }
            selector_json = rec_header.at("selector");
            std::string encoding = rec_header.value("encoding", "json");
            if (encoding == "bytes") {
                payload = json::binary(rec_payload);
            } else {
                payload = json::parse(std::string(rec_payload.begin(), rec_payload.end()));
            }
            stack = rec_header.value("stack", std::vector<std::string>{});

            if (!selector_json.is_null()) {
                Selector s = Selector::from_json(selector_json);
                std::cout << "[REST " << config_.id << "] <- PUB: " << s.path << std::endl;
            }
            notify(selector_json, payload, stack);
            
            json resp_header = {{"info", {{"state", "AVAILABLE"}}}, {"encoding", "json"}};
            std::vector<uint8_t> record_bytes = encode_record(resp_header);
            res.status = 200;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        } catch (const std::exception& e) {
            json resp_header = {
                {"info", json({{"error", e.what()}, {"state", "ERROR"}})},
                {"encoding", "json"}
            };
            std::vector<uint8_t> record_bytes = encode_record(resp_header);
            res.status = 500;
            res.set_content((const char*)record_bytes.data(), record_bytes.size(), "application/octet-stream");
        }
    });

    svr->Post("/listen", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peer_id = req.get_header_value("x-vfs-peer-id");
        if (peer_id.empty()) {
            std::vector<uint8_t> req_body(req.body.begin(), req.body.end());
            json rec_header;
            std::vector<uint8_t> rec_payload;
            if (decode_record(req_body, rec_header, rec_payload)) {
                peer_id = rec_header.value("peerId", "");
            }
        }
        if (peer_id.empty()) {
            res.status = 400;
            res.set_content("Missing x-vfs-peer-id", "text/plain");
            return;
        }
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
                handle_binary_frame(peerId, expecting_binary_header, data);
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
                    handle_ws_frame(peerId, frame);
                }
            } catch (const std::exception& e) {
                std::cerr << "[WS " << config_.id << "] Frame error from " << peerId << ": " << e.what() << std::endl;
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
