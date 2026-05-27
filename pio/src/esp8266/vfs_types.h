#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <stdexcept>

namespace fs {

using json = nlohmann::json;

class VFSException : public std::runtime_error {
public:
    int code;
    VFSException(const std::string& msg, int c = 500) : std::runtime_error(msg), code(c) {}
};

class JCBParseException : public std::runtime_error {
public:
    JCBParseException(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * CID: Strongly-typed Content Identifier (SHA-256 hash).
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
    }

    void validate() const {
        if (parameters.is_null() || !parameters.is_object()) {
            throw VFSException("Schema Violation: Selector parameters must be an object.", 400);
        }
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
        s.path = j.value("path", "");
        s.parameters = j.value("parameters", json::object());
        if (s.parameters.is_null()) s.parameters = json::object();
        s.output = j.value("output", "");
        
        s.validate();
        return s;
    }
};

inline void to_json(json& j, const Selector& s) { j = s.to_json(); }
inline void from_json(const json& j, Selector& s) { s = Selector::from_json(j); }

// Utilities
std::vector<uint8_t> encode_jcb(const json& j);
json decode_jcb(const std::vector<uint8_t>& buf);
std::string base64_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64_decode(const std::string& base64);
std::string vfs_hash256(const std::vector<uint8_t>& data);
std::string vfs_hash256_str(const std::string& input);

} // namespace fs
