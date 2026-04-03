#include "../include/vfs_client.h"
#include "../vendor/httplib.h"
#include <iostream>
#include <thread>
#include <atomic>

namespace jotcad {
namespace fs {

class RESTVFSClient : public VFSClient {
public:
    RESTVFSClient(const std::string& base_url, const std::string& peer_id) 
        : peer_id_(peer_id), closed_(false) {
        // Parse base_url to extract host/port/prefix
        // For simplicity, we assume http://host:port format
        size_t proto_end = base_url.find("://");
        std::string without_proto = (proto_end == std::string::npos) ? base_url : base_url.substr(proto_end + 3);
        size_t path_start = without_proto.find("/");
        
        host_port_ = (path_start == std::string::npos) ? without_proto : without_proto.substr(0, path_start);
        prefix_ = (path_start == std::string::npos) ? "" : without_proto.substr(path_start);
        
        client_ = std::make_unique<httplib::Client>(host_port_);
        // Ensure long timeouts for blocking reads
        client_->set_read_timeout(60, 0); 
    }

    ~RESTVFSClient() {
        closed_ = true;
        if (watch_thread_.joinable()) watch_thread_.join();
    }

    std::vector<uint8_t> read(const std::string& path, const json& parameters) override {
        json body = {{"path", path}, {"parameters", parameters}};
        auto res = client_->Post(prefix_ + "/read", headers(), body.dump(), "application/json");
        
        if (res && res->status == 200) {
            return std::vector<uint8_t>(res->body.begin(), res->body.end());
        }
        throw std::runtime_error("VFS Read failed: " + (res ? std::to_string(res->status) : "Connection error"));
    }

    void write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data) override {
        httplib::Headers h = headers();
        h.emplace("x-vfs-info", json({{"path", path}, {"parameters", parameters}}).dump());
        
        auto res = client_->Put(prefix_ + "/write", h, (const char*)data.data(), data.size(), "application/octet-stream");
        if (!res || res->status >= 300) {
            throw std::runtime_error("VFS Write failed");
        }
    }

    void link(const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params) override {
        json body = {
            {"source", {{"path", src_path}, {"parameters", src_params}}},
            {"target", {{"path", tgt_path}, {"parameters", tgt_params}}}
        };
        auto res = client_->Post(prefix_ + "/link", headers(), body.dump(), "application/json");
        if (!res || res->status >= 300) {
            throw std::runtime_error("VFS Link failed");
        }
    }

    bool lease(const std::string& path, const json& parameters, int duration_ms) override {
        json body = {{"path", path}, {"parameters", parameters}, {"duration", duration_ms}};
        auto res = client_->Post(prefix_ + "/lease", headers(), body.dump(), "application/json");
        if (res && res->status == 200) {
            return json::parse(res->body)["success"];
        }
        return false;
    }

    std::string status(const std::string& path, const json& parameters) override {
        json body = {{"path", path}, {"parameters", parameters}};
        auto res = client_->Post(prefix_ + "/status", headers(), body.dump(), "application/json");
        if (res && res->status == 200) {
            return json::parse(res->body)["state"];
        }
        return "MISSING";
    }

    std::string tickle(const std::string& path, const json& parameters) override {
        json body = {{"path", path}, {"parameters", parameters}};
        auto res = client_->Post(prefix_ + "/tickle", headers(), body.dump(), "application/json");
        if (res && res->status == 200) {
            return json::parse(res->body)["state"];
        }
        return "MISSING";
    }

    void watch(const std::string& path_pattern, WatchCallback callback) override {
        if (watch_thread_.joinable()) return;

        watch_thread_ = std::thread([this, callback]() {
            std::cout << "[C++ VFS] Watch thread starting for: " << peer_id_ << std::endl;
            httplib::Client watch_client(host_port_);
            watch_client.set_read_timeout(3600, 0);

            while (!closed_) {
                auto res = watch_client.Get(prefix_ + "/watch?peerId=" + peer_id_, headers(),
                    [&](const char *data, size_t data_length) {
                        std::string chunk(data, data_length);
                        // std::cout << "[C++ VFS] SSE Chunk: " << chunk << std::endl;
                        
                        size_t pos = 0;
                        while ((pos = chunk.find("data: ", pos)) != std::string::npos) {
                            size_t end = chunk.find("\n\n", pos);
                            if (end == std::string::npos) break;
                            
                            std::string json_str = chunk.substr(pos + 6, end - (pos + 6));
                            try {
                                json j = json::parse(json_str);
                                if (j.contains("type") && j["type"] == "COMMAND") {
                                    handle_command(j);
                                } else {
                                    VFSEvent ev = { j["path"], j["parameters"], j["state"], j["source"] };
                                    callback(ev);
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "[C++ VFS] JSON Parse Error: " << e.what() << " in " << json_str << std::endl;
                            }
                            pos = end + 2;
                        }
                        return !closed_;
                    });
                
                if (closed_) break;
                if (res.error() != httplib::Error::Success) {
                    std::cerr << "[C++ VFS] Watch error: " << httplib::to_string(res.error()) << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }

    void on_read(std::function<std::vector<uint8_t>(const std::string&, const json&)> handler) override {
        read_handler_ = handler;
    }

private:
    std::string peer_id_;
    std::string host_port_;
    std::string prefix_;
    std::unique_ptr<httplib::Client> client_;
    std::thread watch_thread_;
    std::atomic<bool> closed_;
    std::function<std::vector<uint8_t>(const std::string&, const json&)> read_handler_;

    httplib::Headers headers() {
        return {{"x-vfs-peer-id", peer_id_}};
    }

    void handle_command(const json& cmd) {
        if (cmd["op"] == "READ" && read_handler_) {
            std::string path = cmd["path"];
            json params = cmd["parameters"];
            std::string cmd_id = cmd["id"];
            auto data = read_handler_(path, params);
            client_->Post(prefix_ + "/reply/" + cmd_id, headers(), (const char*)data.data(), data.size(), "application/octet-stream");
        }
    }
};

std::unique_ptr<VFSClient> create_rest_client(const std::string& base_url, const std::string& peer_id) {
    return std::make_unique<RESTVFSClient>(base_url, peer_id);
}

} // namespace fs
} // namespace jotcad
