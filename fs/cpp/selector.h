#pragma once

#include "vendor/json.hpp"
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
        json j = {{"path", path}, {"parameters", parameters}}; 
        if (!output.empty()) j["output"] = output;
        return j;
    }

    static Selector from_json(const json& j) {
        if (j.is_string()) return {j.get<std::string>(), json::object(), ""};
        Selector s;
        if (j.contains("path")) s.path = j.at("path").get<std::string>();
        if (j.contains("parameters") && j.at("parameters").is_object()) s.parameters = j.at("parameters");
        if (j.contains("output") && j.at("output").is_string()) s.output = j.at("output").get<std::string>();
        return s;
    }

    Selector with_output(const std::string& out) const {
        Selector s = *this;
        s.output = out;
        return s;
    }
};

// nlohmann::json integration for Selector
inline void to_json(json& j, const Selector& s) { j = s.to_json(); }
inline void from_json(const json& j, Selector& s) {
    if (j.is_string()) { 
        s.path = j.get<std::string>(); 
        s.parameters = json::object(); 
        s.output = "";
    } else {
        if (j.contains("path")) s.path = j.at("path").get<std::string>();
        if (j.contains("parameters") && j.at("parameters").is_object()) s.parameters = j.at("parameters");
        else s.parameters = json::object();
        if (j.contains("output") && j.at("output").is_string()) s.output = j.at("output").get<std::string>();
        else s.output = "";
    }
}

} // namespace fs
