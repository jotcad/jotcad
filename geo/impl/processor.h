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

    // Serve is decommissioned in favor of VFSNode in ops.cc
};

} // namespace geo
} // namespace jotcad
