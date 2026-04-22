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
        try {
            // Operators receive (vfs, fulfilling_selector, ...args)
            if constexpr (std::is_same_v<decltype(Op::execute(vfs, req.selector, std::declval<Args>()...)), void>) {
                Op::execute(vfs, req.selector, decode<Args>(vfs, keys[Is], req.selector.parameters, Op::schema(), req.stack)...);
            } else {
                auto res = Op::execute(vfs, req.selector, decode<Args>(vfs, keys[Is], req.selector.parameters, Op::schema(), req.stack)...);
                // Fulfill the primary selector
                vfs->write<Shape>(req.selector, res, "$out");
            }
            
            // Return the produced artifact data
            return vfs->read<std::vector<uint8_t>>(req.selector);
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
        if constexpr (std::is_same_v<T, Shape>) {
            if (val.is_object() && val.contains("path")) {
                auto s_addr = val.get<fs::Selector>();
                std::cout << "[Processor::decode] Reifying Shape argument '" << key << "' from Selector: " << s_addr.path << std::endl;
                return vfs->read<Shape>(s_addr);
            }
            std::cout << "[Processor::decode] Decoding inline Shape for argument '" << key << "'" << std::endl;
            Shape s = Shape::from_json(val);
            if (!s.geometry.has_value() && s.components.empty()) {
                 std::cerr << "[Processor::decode] WARNING: Shape argument '" << key << "' has no geometry and no components." << std::endl;
            }
            return s;
        } else if constexpr (std::is_same_v<T, std::vector<Shape>>) {
            std::vector<Shape> results;
            if (!val.is_array()) {
                throw std::runtime_error("Argument '" + key + "' must be an array for std::vector<Shape>");
            }
            std::cout << "[Processor::decode] Decoding std::vector<Shape> for argument '" << key << "' (" << val.size() << " items)" << std::endl;
            for (const auto& item : val) {
                if (item.is_object() && item.contains("path")) {
                    auto s_addr = item.get<fs::Selector>();
                    std::cout << "[Processor::decode]   - Reifying element from Selector: " << s_addr.path << std::endl;
                    results.push_back(vfs->read<Shape>(s_addr));
                } else {
                    results.push_back(Shape::from_json(item));
                }
            }
            return results;
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
