#pragma once
#include <string>
#include <functional>
#include <json.hpp>
#include "vfs_node.h"
#include "selector.h"

namespace jotcad {
namespace geo {

// 1. Is this a computational operation node (Selector AST)?
inline bool is_operation(const nlohmann::json& node) {
    return node.is_object() && node.contains("path") && node["path"].is_string();
}

// 2. Is this slot empty / unbound / waiting for an argument?
inline bool is_unbound_slot(const nlohmann::json& val) {
    return val.is_null() || (val.is_string() && val.get<std::string>() == "$in");
}

// 3. Does this parameter slot already contain a concrete provided value?
inline bool has_value(const nlohmann::json& params, const std::string& key) {
    if (!params.is_object() || !params.contains(key)) return false;
    return !is_unbound_slot(params[key]);
}

// 4. Is this an unresolved expression (a CID pointing to an unbound recipe in the VFS)?
inline bool is_unresolved_expression(fs::VFSNode* vfs, const nlohmann::json& val, nlohmann::json& out_selector) {
    if (!val.is_string() || val.get<std::string>().size() != 64) return false;
    try {
        auto res = vfs->read<fs::VFSResult>(fs::CID::from_json(val));
        if (res.metadata.contains("selector")) {
            out_selector = res.metadata["selector"];
            return true;
        }
    } catch (...) {}
    return false;
}

/**
 * Recursively traverses a computational recipe and overrides its innermost
 * subject ($in) with the provided target value.
 */
inline fs::Selector bind_recipe(fs::VFSNode* vfs, const fs::Selector& recipe, const nlohmann::json& target_val) {
    nlohmann::json ast = recipe.to_json();

    std::function<void(nlohmann::json&)> override_innermost_subject = [&](nlohmann::json& node) {
        if (!is_operation(node)) {
            node = target_val;
            return;
        }

        auto& params = node["parameters"];
        if (!params.is_object()) {
            params = nlohmann::json::object();
        }

        // 1. If this node has no $in, or $in is an unbound slot, set it directly
        if (!params.contains("$in") || is_unbound_slot(params["$in"])) {
            params["$in"] = target_val;
            return;
        }

        // 2. If $in is a nested sub-operation object, descend and override its innermost subject
        if (is_operation(params["$in"])) {
            override_innermost_subject(params["$in"]);
            return;
        }

        // 3. If $in is a CID containing a recipe expression, expand, descend, and override
        nlohmann::json expanded_selector;
        if (is_unresolved_expression(vfs, params["$in"], expanded_selector)) {
            override_innermost_subject(expanded_selector);
            params["$in"] = expanded_selector;
            return;
        }

        // 4. Otherwise, $in contains a concrete subject/placeholder: override it with target_val!
        params["$in"] = target_val;
    };

    override_innermost_subject(ast);
    return fs::Selector::from_json(ast);
}

} // namespace geo
} // namespace jotcad
