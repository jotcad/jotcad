#pragma once

#include "../../fs/cpp/include/vfs_node.h"
#include "geometry.h"
#include "shape.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <json.hpp>
#include <type_traits>
#include <tuple>

namespace jotcad {
namespace geo {

class Processor {
public:
    using json = nlohmann::json;
    using LogicHandler = std::function<std::vector<uint8_t>(jotcad::fs::VFSNode*, const std::string&, const json&, const std::vector<std::string>&)>;
    
    struct Operation {
        std::string path;
        LogicHandler logic;
        json schema;
    };

    static std::map<std::string, Operation>& registry() {
        static std::map<std::string, Operation> reg;
        return reg;
    }

    static void register_op(const std::string& path, LogicHandler logic, const json& schema) {
        Operation op; op.path = path; op.logic = logic; op.schema = schema;
        registry()[path] = op;
    }

    /**
     * UNIFIED DISPATCHER (The Bridge)
     * Maps argument_keys() from the request JSON to the execute() signature.
     * Supports additional 'bound_args' for internal variants.
     */
    template <typename Op, typename... T, typename... Bound>
    static std::vector<uint8_t> logic_wrapper(jotcad::fs::VFSNode* vfs, const std::string& path, const json& params, const std::vector<std::string>& stack, Bound&&... bound_args) {
        auto keys = Op::argument_keys();
        auto schema = Op::schema();
        Shape out;
        
        std::cout << "[Processor] Logic Wrapper: " << path << " with " << sizeof...(T) << " args" << std::endl;

        try {
            // Specialize for common patterns (In + Bound + Out)
            if constexpr (sizeof...(T) == 1) {
                auto a1 = decode<typename std::tuple_element<0, std::tuple<T...>>::type>(vfs, keys[0], params, schema, stack);
                Op::execute(vfs, a1, std::forward<Bound>(bound_args)..., out);
            } else if constexpr (sizeof...(T) == 2) {
                auto a1 = decode<typename std::tuple_element<0, std::tuple<T...>>::type>(vfs, keys[0], params, schema, stack);
                auto a2 = decode<typename std::tuple_element<1, std::tuple<T...>>::type>(vfs, keys[1], params, schema, stack);
                Op::execute(vfs, a1, a2, std::forward<Bound>(bound_args)..., out);
            } else if constexpr (sizeof...(T) == 3) {
                auto a1 = decode<typename std::tuple_element<0, std::tuple<T...>>::type>(vfs, keys[0], params, schema, stack);
                auto a2 = decode<typename std::tuple_element<1, std::tuple<T...>>::type>(vfs, keys[1], params, schema, stack);
                auto a3 = decode<typename std::tuple_element<2, std::tuple<T...>>::type>(vfs, keys[2], params, schema, stack);
                Op::execute(vfs, a1, a2, a3, std::forward<Bound>(bound_args)..., out);
            }
        } catch (const std::exception& e) {
            std::cerr << "[Processor] FATAL: Op " << path << " failed: " << e.what() << std::endl;
            throw;
        }
        
        std::cout << "[Processor] Op " << path << " finished successfully" << std::endl;
        return JotVfsProtocol::write_shape_obj(out);
    }

    /**
     * THE PRIMARY REGISTRATION PATH
     * T... are the types of the arguments (including inputs) BEFORE the output Shape& references.
     */
    template <typename Op, typename... T>
    static void register_op(const std::string& path_override, const json& override_schema = json()) {
        Operation op;
        op.path = path_override;
        op.schema = override_schema.is_null() ? Op::schema() : override_schema;
        op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const json& params, const std::vector<std::string>& stack) {
            return logic_wrapper<Op, T...>(vfs, path, params, stack);
        };
        registry()[op.path] = op;
    }

    template <typename Op, typename... T>
    static void register_op() {
        register_op<Op, T...>(Op::path);
    }

    template <typename T>
    static T decode(jotcad::fs::VFSNode* vfs, const std::string& key, const json& params, const json& schema, const std::vector<std::string>& stack) {
        json arg_schema = schema["arguments"].value(key, json::object());
        json val = params.contains(key) ? params.at(key) : arg_schema.value("default", json());

        bool is_input = schema.contains("inputs") && schema["inputs"].contains(key);
        
        if constexpr (std::is_same_v<T, Shape>) {
            if (is_input && val.is_object() && val.contains("path")) {
                return Shape::from_json(vfs->read<json>({val.at("path"), val.value("parameters", json::object()), stack}));
            }
            return Shape::from_json(val);
        }
        else if constexpr (std::is_same_v<T, std::vector<Shape>>) {
            std::vector<Shape> results;
            json resolved = val;
            if (is_input && val.is_object() && val.contains("path")) {
                resolved = vfs->read<json>({val.at("path"), val.value("parameters", json::object()), stack});
            }
            
            if (resolved.is_object() && resolved.contains("geometry")) {
                auto geo = resolved["geometry"];
                if (geo.value("path", "") == "jot/group") {
                    for (auto& item : geo["parameters"]["items"]) results.push_back(Shape::from_json(item));
                } else {
                    results.push_back(Shape::from_json(resolved));
                }
            } else if (resolved.is_array()) {
                for (auto& item : resolved) results.push_back(Shape::from_json(item));
            } else if (!resolved.is_null()) {
                results.push_back(Shape::from_json(resolved));
            }
            return results;
        }
        else if constexpr (std::is_same_v<T, double>) {
            return val.template get<double>();
        }
        else if constexpr (std::is_same_v<T, std::vector<double>>) {
            if (val.is_array()) return val.template get<std::vector<double>>();
            if (val.is_number()) return {val.template get<double>()};
            return {};
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return val.template get<std::string>();
        }
        return T();
    }
};

} // namespace geo
} // namespace jotcad
