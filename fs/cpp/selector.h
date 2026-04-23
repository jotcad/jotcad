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

    json to_json() const { 
        // A base selector has NO output field.
        // A selector targeting a port MUST have a non-empty output string.
        json j = {{"path", path}}; 
        if (!parameters.empty()) j["parameters"] = parameters;
        if (!output.empty()) j["output"] = output;
        return j;
    }

    static Selector from_json(const json& j) {
        if (j.is_string()) return {j.get<std::string>(), json::object(), ""};
        Selector s;
        if (j.contains("path")) s.path = j.at("path").get<std::string>();
        else s.path = "";

        if (j.contains("parameters") && j.at("parameters").is_object()) s.parameters = j.at("parameters");
        else s.parameters = json::object();

        if (j.contains("output")) {
            if (!j.at("output").is_string()) throw VFSException("Selector output must be a string", 400);
            s.output = j.at("output").get<std::string>();
            if (s.output.empty()) {
                throw VFSException("Schema Violation: Selector output cannot be empty. Omit the field for base operations.", 400);
            }
        } else {
            s.output = "";
        }
        
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
