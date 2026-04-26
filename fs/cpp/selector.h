#pragma once

#include "vendor/json.hpp"
#include "vfs_exception.h"
#include <string>

namespace fs {

using json = nlohmann::json;

/**
 * Selector: The universal address for mesh content.
 */
struct Selector {
    std::string path;
    json parameters = json::object();
    std::string output;

    Selector() = default;
    Selector(std::string p, json params = json::object(), std::string out = "") 
        : path(std::move(p)), parameters(std::move(params)), output(std::move(out)) {
        if (parameters.is_null()) parameters = json::object();
        if (!path.empty()) validate();
    }

    void validate() const {
        if (parameters.is_null()) {
            throw VFSException("Schema Violation: Selector parameters cannot be null. Must be an object.", 400);
        }
        if (!parameters.is_object()) {
            throw VFSException("Schema Violation: Selector parameters must be an object.", 400);
        }
        // Path is required for all computational selectors. 
        // Direct CID reads bypass Selector validation.
        if (path.empty()) {
            throw VFSException("Schema Violation: Selector path cannot be empty.", 400);
        }
    }

    json to_json() const { 
        validate();
        json j = {{"path", path}, {"parameters", parameters}}; 
        if (!output.empty()) j["output"] = output;
        return j;
    }

    static Selector from_json(const json& j) {
        if (j.is_string()) return {j.get<std::string>(), json::object(), ""};
        
        Selector s;
        if (j.contains("path") && j.at("path").is_string()) {
            s.path = j.at("path").get<std::string>();
        } else {
            s.path = "";
        }

        if (j.contains("parameters") && j.at("parameters").is_object()) {
            s.parameters = j.at("parameters");
        } else if (j.contains("parameters") && j.at("parameters").is_null()) {
            s.parameters = json::object(); // Normalization during parsing is allowed
        } else {
            s.parameters = json::object();
        }

        if (j.contains("output")) {
            if (!j.at("output").is_string()) throw VFSException("Selector output must be a string", 400);
            s.output = j.at("output").get<std::string>();
        } else {
            s.output = "";
        }
        
        s.validate();
        return s;
    }

    Selector with_output(const std::string& out) const {
        if (!out.empty()) {
            Selector s = *this;
            s.output = out;
            return s;
        }
        // If we want the base selector, return with "" (which will be omitted in to_json)
        Selector s = *this;
        s.output = "";
        return s;
    }
};

// nlohmann::json integration for Selector
inline void to_json(json& j, const Selector& s) { j = s.to_json(); }
inline void from_json(const json& j, Selector& s) {
    s = Selector::from_json(j);
}

} // namespace fs
