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

    static void register_op(const Operation& op) {
        registry()[op.path] = op;
    }

    template <typename Op>
    static void register_op() {
        Operation op;
        op.path = Op::path;
        op.schema = Op::schema();
        op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const json& params, const std::vector<std::string>& stack) {
            return Op::logic(vfs, path, params, stack);
        };
        registry()[op.path] = op;
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
            
            // If the resolved object is a Shape, check its geometry for op/group
            if (resolved.is_object() && resolved.contains("geometry")) {
                auto geo = resolved["geometry"];
                if (geo.value("path", "") == "op/group") {
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

    static std::vector<json> cartesian_batch(const json& params, const std::vector<std::string>& keys) {
        std::vector<json> results = {{}};
        for (const auto& key : keys) {
            std::vector<json> next_results;
            if (params.contains(key)) {
                auto val = params.at(key);
                if (val.is_array()) {
                    for (const auto& combo : results) {
                        for (const auto& item : val) {
                            json next = combo;
                            next[key] = item;
                            next_results.push_back(next);
                        }
                    }
                } else {
                    for (const auto& combo : results) {
                        json next = combo;
                        next[key] = val;
                        next_results.push_back(next);
                    }
                }
            } else {
                next_results = results;
            }
            results = next_results;
        }
        return results;
    }
};

} // namespace geo
} // namespace jotcad
