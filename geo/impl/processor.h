#pragma once
#include "protocols.h"
#include "../../fs/cpp/include/vfs_node.h"
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

    struct Entry {
        std::string path;
        json schema;
        std::function<std::vector<uint8_t>(fs::VFSNode*, const fs::VFSNode::VFSRequest&)> handler;
    };

    static std::map<std::string, Entry>& registry_instance();

    template <typename Op, typename... Args>
    static void register_op(const std::string& path) {
        Entry e;
        e.path = path;
        e.schema = Op::schema();
        e.handler = [](fs::VFSNode* vfs, const fs::VFSNode::VFSRequest& req) -> std::vector<uint8_t> {
            return dispatch<Op, Args...>(vfs, req, Op::argument_keys(), std::make_index_sequence<sizeof...(Args)>{});
        };
        registry_instance()[path] = e;
    }

    template <typename Op, typename... Args, size_t... Is>
    static std::vector<uint8_t> dispatch(fs::VFSNode* vfs, const fs::VFSNode::VFSRequest& req, const std::vector<std::string>& keys, std::index_sequence<Is...>) {
        // Operators receive (vfs, fulfilling_selector, ...args)
        Op::execute(vfs, req.selector, decode<Args>(vfs, keys[Is], req.selector.parameters, Op::schema(), req.stack)...);
        
        // Return the produced artifact data
        return vfs->read<std::vector<uint8_t>>(req.selector);
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
        if constexpr (std::is_same_v<T, Shape>) {
            if (val.is_object() && val.contains("path")) {
                return vfs->read<Shape>(val.get<fs::Selector>());
            }
            Shape s = Shape::from_json(val);
            if (!s.geometry.has_value()) throw std::runtime_error("Shape argument missing geometry: " + key);
            return s;
        } else if constexpr (std::is_same_v<T, std::vector<Shape>>) {
            std::vector<Shape> results;
            if (val.is_array()) {
                for (const auto& item : val) {
                    if (item.is_object() && item.contains("path")) {
                        results.push_back(vfs->read<Shape>(item.get<fs::Selector>()));
                    } else {
                        Shape s = Shape::from_json(item);
                        if (!s.geometry.has_value()) throw std::runtime_error("Shape array element missing geometry");
                        results.push_back(s);
                    }
                }
            }
            return results;
        } else if constexpr (std::is_same_v<T, fs::Selector>) {
            return val.get<fs::Selector>();
        } else {
            return val.get<T>();
        }
    }

    static json hydrate(const json& selector, const Shape& in) {
        json out = selector;
        if (out.contains("parameters") && out["parameters"].is_object()) {
            for (auto& [key, value] : out["parameters"].items()) {
                if (value == "$in") out["parameters"][key] = in.to_json();
            }
        }
        return out;
    }
};

} // namespace geo
} // namespace jotcad
