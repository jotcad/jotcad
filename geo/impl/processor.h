#pragma once
#include "protocols.h"
#include "../../fs/cpp/vfs_node.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace jotcad {
namespace geo {

/**
 * Processor: The schema-aware execution engine for JOT operators.
 */
struct Processor {
    using json = nlohmann::json;

    template <typename Op, typename... Args>
    static void register_op(fs::VFSNode* vfs, const std::string& path) {
        auto handler = [vfs](const fs::VFSNode::VFSRequest& req) {
            dispatch<Op, Args...>(vfs, req, Op::argument_keys(), std::make_index_sequence<sizeof...(Args)>{});
        };
        vfs->register_op(path, handler, Op::schema());
    }

    /**
     * Executes an operator by path using the registered handler on the provided VFS.
     * This avoids needing the template headers at the call site.
     */
    static void execute(fs::VFSNode* vfs, const fs::Selector& selector, const std::vector<std::string>& stack = {}) {
        fs::VFSNode::VFSRequest req;
        fs::Selector s = selector;
        req.selector = s;
        req.stack = stack;
        vfs->read<std::vector<uint8_t>>(req);
    }

    template <typename Op, typename... Args, size_t... Is>
    static void dispatch(fs::VFSNode* vfs, const fs::VFSNode::VFSRequest& req, const std::vector<std::string>& keys, std::index_sequence<Is...>) {
        try {
            // Execute Operator (fulfills its own ports)
            Op::execute(vfs, req.selector, decode<Args>(vfs, keys[Is], req.selector.parameters, Op::schema(), req.stack)...);
        } catch (const std::exception& e) {
            std::cerr << "[Processor] Dispatch Error in " << Op::path << ": " << e.what() << std::endl;
            throw;
        }
    }

    template <typename T>
    static T decode(fs::VFSNode* vfs, const std::string& key, const json& params, const json& schema, const std::vector<std::string>& stack) {
        if (!params.contains(key)) {
             if (schema.at("arguments").contains(key) && schema.at("arguments").at(key).contains("default")) {
                 return schema.at("arguments").at(key).at("default").get<T>();
             }
             throw std::runtime_error("Missing required argument: " + key);
        }

        const auto& val = params[key];
        const auto& arg_schema = schema.at("arguments").at(key);

        if constexpr (std::is_same_v<T, Shape>) {
            if (val.is_string() && val.get<std::string>().size() == 64) {
                return vfs->read<Shape>(fs::CID{val.get<std::string>()});
            }
            if (val.is_object() && val.contains("path")) {
                return vfs->read<Shape>(val.get<fs::Selector>());
            }
            return Shape::from_json(val);
        } else if constexpr (std::is_same_v<T, std::vector<Shape>>) {
            std::vector<Shape> results;
            if (!val.is_array()) throw std::runtime_error("Argument '" + key + "' must be an array for std::vector<Shape>");
            for (const auto& item : val) {
                if (item.is_object() && item.contains("path")) {
                    results.push_back(vfs->read<Shape>(item.get<fs::Selector>()));
                } else {
                    results.push_back(Shape::from_json(item));
                }
            }
            return results;
        } else if constexpr (std::is_same_v<T, fs::Selector>) {
            if (val.is_object() && val.contains("path")) {
                return val.get<fs::Selector>();
            }
            throw std::runtime_error("Argument '" + key + "' must be a Selector");
        } else {
            return val.get<T>();
        }
    }
};

} // namespace geo
} // namespace jotcad
