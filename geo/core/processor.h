#pragma once
#include "protocols.h"
#include "../../fs/cpp/vfs_node.h"
#include "../math/interval.h"
#include <string>
#include <vector>
#include <map>
#include <functional>

#include <optional>

namespace jotcad {
namespace geo {

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

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
        json s = Op::schema();
        s["path"] = path; // Force schema path to match registration key
        vfs->register_op(path, handler, s);
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
            Op::execute(vfs, req.selector, decode<Args>(vfs, keys[Is], req.selector.parameters, Op::schema(), req.stack, Op::path)...);
        } catch (const std::exception& e) {
            std::string msg = "[Processor] " + std::string(Op::path) + " Error: " + e.what();
            std::cerr << msg << std::endl;
            throw std::runtime_error(msg);
        }
    }

    template <typename T>
    static T decode(fs::VFSNode* vfs, const std::string& key, const json& params, const json& schema, const std::vector<std::string>& stack, const std::string& opPath = "internal") {
        auto wrap_err = [&](const std::string& msg) {
            return std::runtime_error("Operator '" + opPath + "' Argument '" + key + "' failure: " + msg);
        };

        if constexpr (is_optional<T>::value) {
            if (!params.contains(key)) return std::nullopt;
            return decode<typename T::value_type>(vfs, key, params, schema, stack, opPath);
        } else {
            if (!params.contains(key)) {
                // Find default in array-based arguments
                if (schema.contains("arguments") && schema.at("arguments").is_array()) {
                    for (const auto& arg : schema.at("arguments")) {
                        if (arg.contains("name") && arg.at("name") == key && arg.contains("default")) {
                            try {
                                return arg.at("default").get<T>();
                            } catch (const std::exception& e) {
                                throw wrap_err(std::string("Default value invalid: ") + e.what());
                            }
                        }
                    }
                }
                throw wrap_err("Missing required argument");
            }

            const auto& val = params[key];

            try {
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
                    if (!val.is_array()) throw std::runtime_error("Type must be an array for std::vector<Shape>");
                    for (const auto& item : val) {
                        if (item.is_string() && item.get<std::string>().size() == 64) {
                            results.push_back(vfs->read<Shape>(fs::CID{item.get<std::string>()}));
                        } else if (item.is_object() && item.contains("path")) {
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
                    throw std::runtime_error("Type must be a Selector");
                } else if constexpr (std::is_same_v<T, Interval>) {
                    return Interval::from_json(val);
                } else {
                    return val.get<T>();
                }
            } catch (const std::exception& e) {
                throw wrap_err(e.what());
            }
        }
    }
};

} // namespace geo
} // namespace jotcad
