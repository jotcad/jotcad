#pragma once

#include "vendor/json.hpp"
#include <string>

namespace fs {

using json = nlohmann::json;

/**
 * CID: Strongly-typed Content Identifier (SHA-256 hash).
 * 
 * Provides type safety to ensure that raw strings are not accidentally 
 * used where a content address is expected.
 */
struct CID {
    std::string value;

    bool is_empty() const { return value.empty(); }
    bool operator==(const CID& other) const { return value == other.value; }
    bool operator!=(const CID& other) const { return !(*this == other); }
    bool operator<(const CID& other) const { return value < other.value; }

    json to_json() const { return value; }
    static CID from_json(const json& j) {
        if (j.is_string()) return {j.get<std::string>()};
        return {""};
    }
};

inline void to_json(json& j, const CID& cid) { j = cid.to_json(); }
inline void from_json(const json& j, CID& cid) { cid = CID::from_json(j); }

} // namespace fs
