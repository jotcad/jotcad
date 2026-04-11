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
        : base_url_(base_url), peer_id_(peer_id) {}

    /**
     * Synchronously read an artifact. Blocks until AVAILABLE.
     */
    std::vector<uint8_t> read(const std::string& path, const json& parameters = json::object(), const std::vector<std::string>& stack = {}) override {
        httplib::Client cli(base_url_);
        cli.set_read_timeout(30, 0);

        json body = {
            {"path", path},
            {"parameters", parameters},
            {"stack", stack}
        };

        auto res = cli.Post("/read", body.dump(), "application/json");
        if (res && res->status == 200) {
            return std::vector<uint8_t>(res->body.begin(), res->body.end());
        }
        return {};
    }

    void write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data) override {
        httplib::Client cli(base_url_);
        cli.Post("/write", (const char*)data.data(), data.size(), "application/octet-stream");
    }

    void link(const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params) override {}
    bool lease(const std::string& path, const json& parameters, int duration_ms) override { return true; }
    std::string status(const std::string& path, const json& parameters = json::object()) override { return "AVAILABLE"; }
    void write_state(const std::string& path, const json& parameters, const std::string& state, const std::string& source = "") override {}
    void declare(const std::string& path, const json& schema) override {}
    std::string tickle(const std::string& path, const json& parameters = json::object()) override { return "AVAILABLE"; }
    std::string get_cid(const std::string& path, const json& parameters = json::object()) override { return ""; }
    void watch(const std::string& path, WatchCallback callback) override {}
    void on_read(std::function<std::vector<uint8_t>(const std::string&, const json&)> handler) override {}

private:
    std::string base_url_;
    std::string peer_id_;
};

std::unique_ptr<VFSClient> create_rest_client(const std::string& base_url, const std::string& peer_id) {
    return std::make_unique<RESTVFSClient>(base_url, peer_id);
}

} // namespace fs
} // namespace jotcad
