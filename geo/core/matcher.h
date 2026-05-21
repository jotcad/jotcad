#pragma once
#include "protocols.h"
#include "../../fs/cpp/vfs_node.h"

namespace jotcad {
namespace geo {

/**
 * Matcher: Geometric metadata query engine.
 */
struct Matcher {
    static bool matches(const Shape& s, const fs::Selector& sel) {
        if (sel.path == "jot/has" || sel.path == "has") {
            std::string key = sel.parameters.value("key", "");
            if (key.empty()) return false;
            if (!s.tags.contains(key)) return false;
            if (sel.parameters.contains("value") && !sel.parameters.at("value").is_null()) {
                auto val = sel.parameters.at("value");
                auto tag_val = s.tags.at(key);
                
                // Flexible comparison (string vs number)
                if (val.is_number() && tag_val.is_string()) {
                    try { return std::stod(tag_val.get<std::string>()) == val.get<double>(); } catch(...) { return false; }
                }
                if (val.is_string() && tag_val.is_number()) {
                    try { return std::stod(val.get<std::string>()) == tag_val.get<double>(); } catch(...) { return false; }
                }
                
                return tag_val == val;
            }
            return true;
        }
        
        return false;
    }
};

} // namespace geo
} // namespace jotcad
