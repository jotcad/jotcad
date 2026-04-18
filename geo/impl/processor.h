#pragma once

#include "../../fs/cpp/include/vfs_node.h"
#include "geometry.h"
#include "shape.h"
#include "protocols.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <json.hpp>
#include <type_traits>
#include <tuple>

namespace jotcad {
namespace fs {

template<>
inline jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const VFSRequest& req) {
    auto bytes = read_impl(req);
    jotcad::geo::Geometry geo;
    geo.decode_text(std::string(bytes.begin(), bytes.end()));
    return geo;
}

// Specializations for Shape
template<>
inline void VFSNode::write_specialized<jotcad::geo::Shape>(const std::string& path, const json& parameters, const jotcad::geo::Shape& data) {
    std::string s = data.to_json().dump();
    std::vector<uint8_t> bytes(s.begin(), s.end());
    write_local(get_cid(path, parameters), bytes, path, parameters);
}

template<>
inline std::string VFSNode::write_with_cid_specialized<jotcad::geo::Shape>(const std::string& path, const jotcad::geo::Shape& data) {
    std::string s = data.to_json().dump();
    std::vector<uint8_t> bytes(s.begin(), s.end());
    std::string hash = vfs_hash256(bytes);
    write_local(get_cid(path, {{"cid", hash}}), bytes, path, {{"cid", hash}});
    return hash;
}

// Specializations for Geometry
template<>
inline void VFSNode::write_specialized<jotcad::geo::Geometry>(const std::string& path, const json& parameters, const jotcad::geo::Geometry& data) {
    std::string s = data.encode_text();
    std::vector<uint8_t> bytes(s.begin(), s.end());
    write_local(get_cid(path, parameters), bytes, path, parameters);
}

template<>
inline std::string VFSNode::write_with_cid_specialized<jotcad::geo::Geometry>(const std::string& path, const jotcad::geo::Geometry& data) {
    std::string s = data.encode_text();
    std::vector<uint8_t> bytes(s.begin(), s.end());
    std::string hash = vfs_hash256(bytes);
    write_local(get_cid(path, {{"cid", hash}}), bytes, path, {{"cid", hash}});
    return hash;
}

} // namespace fs

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

    template <typename Op, typename Out, typename... T, size_t... Is, typename... Bound>
    static void logic_executor(jotcad::fs::VFSNode* vfs, const std::string& path, const json& params, const json& schema, const std::vector<std::string>& stack, std::index_sequence<Is...>, Out& out, Bound&&... bound_args) {
        auto keys = Op::argument_keys();
        Op::execute(vfs, decode<T>(vfs, keys[Is], params, schema, stack)..., std::forward<Bound>(bound_args)..., out);
    }

    template <typename Op, typename Out, typename... T, typename... Bound>
    static std::vector<uint8_t> logic_wrapper(jotcad::fs::VFSNode* vfs, const std::string& path, const json& params, const std::vector<std::string>& stack, Bound&&... bound_args) {
        auto schema = Op::schema();
        Out out;
        
        try {
            logic_executor<Op, Out, T...>(vfs, path, params, schema, stack, std::make_index_sequence<sizeof...(T)>{}, out, std::forward<Bound>(bound_args)...);
        } catch (const std::exception& e) {
            std::cerr << "[Processor] FATAL: Op " << path << " failed: " << e.what() << std::endl;
            throw;
        }
        
        if constexpr (std::is_same_v<Out, Shape>) return JotVfsProtocol::write_shape_obj(out);
        if constexpr (std::is_same_v<Out, std::vector<uint8_t>>) return out;
        if constexpr (std::is_same_v<Out, json>) { std::string s = out.dump(); return std::vector<uint8_t>(s.begin(), s.end()); }
        if constexpr (std::is_same_v<Out, std::string>) return std::vector<uint8_t>(out.begin(), out.end());
        return JotVfsProtocol::write_json(out);
    }

    template <typename Op, typename Out, typename... T>
    static void register_op(const std::string& path_override, const json& override_schema = json()) {
        Operation op;
        op.path = path_override;
        op.schema = override_schema.is_null() ? Op::schema() : override_schema;
        op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const json& params, const std::vector<std::string>& stack) {
            return logic_wrapper<Op, Out, T...>(vfs, path, params, stack);
        };
        registry()[op.path] = op;
    }

    template <typename Op, typename Out, typename... T>
    static void register_op() {
        register_op<Op, Out, T...>(Op::path);
    }

    template <typename T>
    static T decode(jotcad::fs::VFSNode* vfs, const std::string& key, const json& params, const json& schema, const std::vector<std::string>& stack) {
        json arg_schema = schema["arguments"].value(key, json::object());
        json val = params.contains(key) ? params.at(key) : arg_schema.value("default", json());
        
        if constexpr (std::is_same_v<T, Shape>) {
            if (val.is_object() && val.contains("path") && !val["path"].get<std::string>().empty()) {
                // Address: Reify from Mesh
                return Shape::from_json(vfs->read<json>({val.at("path"), val.value("parameters", json::object()), stack}));
            }
            // Value: Parse Literal
            return Shape::from_json(val);
        }
        else if constexpr (std::is_same_v<T, std::vector<Shape>>) {
            std::vector<Shape> results;
            if (val.is_array()) {
                for (auto& item : val) {
                   if (item.is_object() && item.contains("path") && !item["path"].get<std::string>().empty()) {
                       results.push_back(Shape::from_json(vfs->read<json>({item.at("path"), item.value("parameters", json::object()), stack})));
                   } else {
                       results.push_back(Shape::from_json(item));
                   }
                }
            } else if (!val.is_null()) {
                if (val.is_object() && val.contains("path") && !val["path"].get<std::string>().empty()) {
                    results.push_back(Shape::from_json(vfs->read<json>({val.at("path"), val.value("parameters", json::object()), stack})));
                } else {
                    results.push_back(Shape::from_json(val));
                }
            }
            return results;
        }
        else if constexpr (std::is_same_v<T, double>) return val.is_number() ? val.get<double>() : 0.0;
        else if constexpr (std::is_same_v<T, std::vector<double>>) {
            if (val.is_array()) return val.get<std::vector<double>>();
            if (val.is_number()) return {val.get<double>()};
            return {};
        }
        else if constexpr (std::is_same_v<T, std::string>) return val.is_string() ? val.get<std::string>() : "";
        return T();
    }

    static json hydrate(const json& recipe, const Shape& subject) {
        if (!recipe.is_object() || !recipe.contains("path")) return recipe;
        json hydrated = recipe;
        if (!hydrated.contains("parameters")) hydrated["parameters"] = json::object();
        if (hydrated["parameters"].contains("$in")) {
            hydrated["parameters"]["$in"] = hydrate(hydrated["parameters"]["$in"], subject);
        } else {
            hydrated["parameters"]["$in"] = subject.to_json();
        }
        return hydrated;
    }
};

} // namespace geo
} // namespace jotcad
