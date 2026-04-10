#include "../include/vfs_client.h"
#include "../vendor/httplib.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

namespace jotcad {
namespace fs {

class RESTVFSClient : public VFSClient {
public:
    RESTVFSClient(const std::string& base_url, const std::string& peer_id) 
        : peer_id_(peer_id), closed_(false) {
        size_t proto_end = base_url.find("://");
        std::string without_proto = (proto_end == std::string::npos) ? base_url : base_url.substr(proto_end + 3);
        size_t path_start = without_proto.find("/");
        
        host_port_ = (path_start == std::string::npos) ? without_proto : without_proto.substr(0, path_start);
        prefix_ = (path_start == std::string::npos) ? "" : without_proto.substr(path_start);
        
        client_ = std::make_unique<httplib::Client>(host_port_);
        client_->set_read_timeout(60, 0); 
        client_->set_connection_timeout(5, 0);
    }

    ~RESTVFSClient() {
        closed_ = true;
        if (watch_thread_.joinable()) watch_thread_.join();
    }

    std::vector<uint8_t> read(const std::string& path, const json& parameters) override {
        json body = {{"path", path}, {"parameters", parameters}};
        auto res = post_with_retry("/read", body.dump(), "application/json");
        return std::vector<uint8_t>(res.begin(), res.end());
    }

    void write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data) override {
        httplib::Headers h = headers();
        h.emplace("x-vfs-info", json({{"path", path}, {"parameters", parameters}}).dump());
        
        httplib::Client client(host_port_);
        client.set_read_timeout(60, 0);
        client.set_connection_timeout(5, 0);

        auto res = client.Put(prefix_ + "/write", h, (const char*)data.data(), data.size(), "application/octet-stream");
        if (!res || res->status >= 300) {
            throw std::runtime_error("VFS Write failed: " + (res ? std::to_string(res->status) : "No Response"));
        }
    }

    void link(const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params) override {
        json body = {
            {"source", {{"path", src_path}, {"parameters", src_params}}},
            {"target", {{"path", tgt_path}, {"parameters", tgt_params}}}
        };
        post_with_retry("/link", body.dump(), "application/json");
    }

    bool lease(const std::string& path, const json& parameters, int duration_ms) override {
        json body = {{"path", path}, {"parameters", parameters}, {"duration", duration_ms}};
        try {
            auto res = post_with_retry("/lease", body.dump(), "application/json");
            return json::parse(res)["success"];
        } catch (...) {
            return false;
        }
    }

    std::string status(const std::string& path, const json& parameters) override {
        json body = {{"path", path}, {"parameters", parameters}};
        try {
            auto res = post_with_retry("/status", body.dump(), "application/json");
            return json::parse(res)["state"];
        } catch (...) {
            return "MISSING";
        }
    }

    void write_state(const std::string& path, const json& parameters, const std::string& state, const std::string& source) override {
        json body = {{"path", path}, {"parameters", parameters}, {"state", state}};
        if (!source.empty()) body["source"] = source;
        post_with_retry("/state", body.dump(), "application/json");
    }

    void declare(const std::string& path, const json& schema) override {
        json body = {{"path", path}, {"schema", schema}};
        post_with_retry("/declare", body.dump(), "application/json");
    }

    std::string tickle(const std::string& path, const json& parameters) override {
        json body = {{"path", path}, {"parameters", parameters}};
        try {
            auto res = post_with_retry("/tickle", body.dump(), "application/json");
            return json::parse(res)["state"];
        } catch (...) {
            return "MISSING";
        }
    }

    std::string get_cid(const std::string& path, const json& parameters) override {
        json body = {{"path", path}, {"parameters", parameters}};
        try {
            auto res = post_with_retry("/cid", body.dump(), "application/json");
            return json::parse(res)["cid"];
        } catch (const std::exception& e) {
            std::cerr << "[C++ VFS] get_cid failed: " << e.what() << std::endl;
            return "";
        }
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
                                std::cerr << "[C++ VFS] JSON Parse Error: " << e.what() << std::endl;
                            }
                            pos = end + 2;
                        }
                        return !closed_;
                    });
                
                if (closed_) break;
                if (res.error() != httplib::Error::Success) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
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

    std::string post_with_retry(const std::string& rel_path, const std::string& body, const std::string& content_type) {
        int retries = 10;
        std::string last_error = "Unknown";
        std::string full_url = prefix_ + rel_path;
        
        while (retries > 0) {
            // Create a fresh client for every request to ensure thread-safety
            httplib::Client client(host_port_);
            client.set_read_timeout(60, 0);
            client.set_connection_timeout(5, 0);

            auto res = client.Post(full_url, headers(), body, content_type);
            if (res) {
                if (res->status < 300) {
                    return res->body;
                }
                last_error = "Status " + std::to_string(res->status) + ": " + res->body;
            } else {
                last_error = "Connection Failed (Error: " + httplib::to_string(res.error()) + ")";
            }
            
            if (closed_) break;
            std::cerr << "[C++ VFS] POST " << rel_path << " failed (" << last_error << "), retrying... (" << retries << " left)" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            retries--;
        }
        std::cerr << "[C++ VFS] FATAL: All retries failed for " << rel_path << std::endl;
        throw std::runtime_error("VFS Request failed: " + rel_path + " (" + last_error + ")");
    }

    void handle_command(const json& cmd) {
        if (cmd["op"] == "READ" && read_handler_) {
            std::string path = cmd["path"];
            json params = cmd["parameters"];
            std::string cmd_id = cmd["id"];
            
            // Execute in a separate thread to prevent deadlocking the SSE watch thread
            std::thread([this, path, params, cmd_id]() {
                try {
                    auto data = read_handler_(path, params);
                    
                    // Use a dedicated client for the reply to avoid thread-safety issues with the main client
                    httplib::Client reply_client(host_port_);
                    reply_client.set_read_timeout(10, 0);
                    auto res = reply_client.Post(prefix_ + "/reply/" + cmd_id, headers(), (const char*)data.data(), data.size(), "application/octet-stream");
                    if (res) {
                        if (res->status >= 300) {
                            std::cerr << "[C++ VFS] Reply for " << cmd_id << " failed with status " << res->status << ": " << res->body << std::endl;
                        } else {
                            // std::cout << "[C++ VFS] Reply for " << cmd_id << " sent successfully" << std::endl;
                            // Tiny settle time to ensure Hub finishes writing before we trigger the next step
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    } else {
                        std::cerr << "[C++ VFS] Reply for " << cmd_id << " failed: Connection Error (" << httplib::to_string(res.error()) << ")" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[C++ VFS] Error fulfilling command " << cmd_id << ": " << e.what() << std::endl;
                }
            }).detach();
        }
    }
};

std::unique_ptr<VFSClient> create_rest_client(const std::string& base_url, const std::string& peer_id) {
    return std::make_unique<RESTVFSClient>(base_url, peer_id);
}

} // namespace fs
} // namespace jotcad
