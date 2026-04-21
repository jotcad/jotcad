#pragma once

#include "vendor/json.hpp"
#include <string>
#include <vector>

namespace fs {

using json = nlohmann::json;

class JCBParseException : public std::runtime_error {
public:
    JCBParseException(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * encode_jcb: Deterministic Binary Encoding (JotCAD Canonical Binary)
 */
std::vector<uint8_t> encode_jcb(const json& j);

/**
 * decode_jcb: Decodes JCB bytes back into json.
 */
json decode_jcb(const std::vector<uint8_t>& buf);

/**
 * encode_safe: json -> JCB -> Base64
 */
std::string encode_safe(const json& j);

/**
 * decode_safe: Base64 -> JCB -> json
 */
json decode_safe(const std::string& base64);

/**
 * base64_encode: Raw bytes to Base64 string.
 */
std::string base64_encode(const std::vector<uint8_t>& data);

/**
 * base64_decode: Base64 string to raw bytes.
 */
std::vector<uint8_t> base64_decode(const std::string& base64);

/**
 * vfs_hash256: SHA-256 hash of raw bytes.
 */
std::string vfs_hash256(const std::vector<uint8_t>& data);

/**
 * vfs_hash256_str: SHA-256 hash of a string.
 */
std::string vfs_hash256_str(const std::string& input);

} // namespace fs
