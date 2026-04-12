#pragma once

#include "../../fs/cpp/include/vfs_node.h"
#include "../../fs/cpp/include/vfs_client.h"
#include <cstdint>

namespace jotcad {
namespace geo {

using json = nlohmann::json;

/**
 * NodeClientShim: Implements the legacy VFSClient interface on top of the new VFSNode.
 * This allows all existing C++ geometry ops (like hexagon_op) to work without changes.
 */
class NodeClientShim : public jotcad::fs::VFSClient {
public:
    NodeClientShim(jotcad::fs::VFSNode& node) : node_(node) {}

    std::vector<uint8_t> read(const std::string& path, const json& parameters = json::object(), const std::vector<std::string>& stack = {}) override {
        jotcad::fs::VFSNode::VFSRequest req;
        req.path = path;
        req.parameters = parameters;
        req.stack = stack;
        return node_.read(req);
    }

    void write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data) override {
        node_.write(path, parameters, data);
    }

    void link(const std::string&, const json&, const std::string&, const json&) override {}
    bool lease(const std::string&, const json&, int) override { return true; }
    std::string status(const std::string&, const json&) override { return "AVAILABLE"; }
    void write_state(const std::string&, const json&, const std::string&, const std::string&) override {}
    void declare(const std::string&, const json&) override {}
    std::string tickle(const std::string&, const json&) override { return "AVAILABLE"; }
    std::string get_cid(const std::string& path, const json& parameters) override { return ""; }
    void watch(const std::string&, jotcad::fs::WatchCallback) override {}
    void on_read(std::function<std::vector<uint8_t>(const std::string&, const json&)>) override {}

private:
    jotcad::fs::VFSNode& node_;
};

} // namespace geo
} // namespace jotcad
