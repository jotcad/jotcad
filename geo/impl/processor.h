#pragma once

#include "../../fs/cpp/include/vfs_client.h"
#include "geometry.h"
#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <functional>

namespace jotcad {
namespace geo {

class Processor {
public:
    // LogicHandler signature: (VFSClient*, Path, Params, Stack)
    using LogicHandler = std::function<std::vector<uint8_t>(jotcad::fs::VFSClient*, const std::string&, const nlohmann::json&, const std::vector<std::string>&)>;
    
    struct Operation {
        std::string path;
        LogicHandler logic;
        nlohmann::json schema;
        std::string ux_path;
        std::string ux_code;
    };

    static std::map<std::string, Operation>& registry() {
        static std::map<std::string, Operation> reg;
        return reg;
    }

    static void register_op(const Operation& op) {
        registry()[op.path] = op;
    }

    static std::vector<uint8_t> resolve_geometry(jotcad::fs::VFSClient* vfs, const nlohmann::json& selector, const std::vector<std::string>& stack = {}) {
        std::string path = selector.at("path");
        nlohmann::json params = selector.value("parameters", nlohmann::json::object());

        auto data = vfs->read(path, params, stack);
        if (data.empty()) return {};

        std::string text(data.begin(), data.end());

        try {
            auto j = nlohmann::json::parse(text);
            if (j.contains("geometry")) {
                std::string geo_url = j["geometry"];
                if (geo_url.compare(0, 5, "vfs:/") == 0) {
                    std::string geo_path = geo_url.substr(5);
                    // Resolve the mesh data recursively using the Shape's parameters
                    return resolve_geometry(vfs, {{"path", geo_path}, {"parameters", j.value("parameters", params)}}, stack);
                }
            }
        } catch (...) {}

        return data;
    }
};

} // namespace geo
} // namespace jotcad
