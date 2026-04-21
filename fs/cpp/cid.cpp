#include "cid.h"
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <cstring>
#include <map>
#include <stdexcept>

namespace fs {

static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string res;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    for (auto const& byte : data) {
        char_array_3[i++] = byte;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for(i = 0; (i <4) ; i++) res += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        for (j = 0; (j < i + 1); j++) res += base64_chars[char_array_4[j]];
        while((i++ < 3)) res += '=';
    }
    return res;
}

std::vector<uint8_t> base64_decode(const std::string& s) {
    int in_len = s.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<uint8_t> res;

    while (in_len-- && ( s[in_] != '=') && (isalnum(s[in_]) || (s[in_] == '+') || (s[in_] == '/'))) {
        char_array_4[i++] = s[in_]; in_++;
        if (i == 4) {
            for (i = 0; i <4; i++) char_array_4[i] = base64_chars.find(char_array_4[i]);
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; (i < 3); i++) res.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = i; j <4; j++) char_array_4[j] = 0;
        for (j = 0; j <4; j++) char_array_4[j] = base64_chars.find(char_array_4[j]);
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        for (j = 0; (j < i - 1); j++) res.push_back(char_array_3[j]);
    }
    return res;
}

static void encode_jcb_impl(const json& j, std::vector<uint8_t>& buf) {
    if (j.is_null()) {
        buf.push_back(0x01);
    } else if (j.is_boolean()) {
        buf.push_back(0x02);
        buf.push_back(j.get<bool>() ? 1 : 0);
    } else if (j.is_number()) {
        buf.push_back(0x03);
        double d = j.get<double>();
        uint64_t u;
        std::memcpy(&u, &d, 8);
        for (int i = 7; i >= 0; --i) buf.push_back((u >> (i * 8)) & 0xFF);
    } else if (j.is_string()) {
        buf.push_back(0x04);
        std::string s = j.get<std::string>();
        uint32_t len = (uint32_t)s.size();
        for (int i = 3; i >= 0; --i) buf.push_back((len >> (i * 8)) & 0xFF);
        buf.insert(buf.end(), s.begin(), s.end());
    } else if (j.is_array()) {
        buf.push_back(0x05);
        uint32_t len = (uint32_t)j.size();
        for (int i = 3; i >= 0; --i) buf.push_back((len >> (i * 8)) & 0xFF);
        for (const auto& item : j) encode_jcb_impl(item, buf);
    } else if (j.is_object()) {
        buf.push_back(0x06);
        std::map<std::string, json> m = j.get<std::map<std::string, json>>();
        uint32_t len = (uint32_t)m.size();
        for (int i = 3; i >= 0; --i) buf.push_back((len >> (i * 8)) & 0xFF);
        for (auto const& [k, v] : m) {
            encode_jcb_impl(json(k), buf);
            encode_jcb_impl(v, buf);
        }
    }
}

std::vector<uint8_t> encode_jcb(const json& j) {
    std::vector<uint8_t> buf;
    encode_jcb_impl(j, buf);
    return buf;
}

json decode_jcb_impl(const std::vector<uint8_t>& buf, size_t& offset) {
    if (offset >= buf.size()) throw JCBParseException("JCB Decode: EOF");
    uint8_t tag = buf[offset++];
    if (tag == 0x01) return nullptr;
    if (tag == 0x02) {
        if (offset >= buf.size()) throw JCBParseException("JCB Decode: Boolean EOF");
        return buf[offset++] == 1;
    }
    if (tag == 0x03) {
        if (offset + 8 > buf.size()) throw JCBParseException("JCB Decode: Number EOF");
        uint64_t u = 0;
        for (int i = 7; i >= 0; --i) u |= ((uint64_t)buf[offset++] << (i * 8));
        double d; std::memcpy(&d, &u, 8);
        return d;
    }
    if (tag == 0x04) {
        if (offset + 4 > buf.size()) throw JCBParseException("JCB Decode: String Length EOF");
        uint32_t len = 0;
        for (int i = 3; i >= 0; --i) len |= ((uint32_t)buf[offset++] << (i * 8));
        if (offset + len > buf.size()) throw JCBParseException("JCB Decode: String Data EOF");
        std::string s((const char*)&buf[offset], len);
        offset += len;
        return s;
    }
    if (tag == 0x05) {
        if (offset + 4 > buf.size()) throw JCBParseException("JCB Decode: Array Length EOF");
        uint32_t len = 0;
        for (int i = 3; i >= 0; --i) len |= ((uint32_t)buf[offset++] << (i * 8));
        json arr = json::array();
        for (uint32_t i = 0; i < len; i++) arr.push_back(decode_jcb_impl(buf, offset));
        return arr;
    }
    if (tag == 0x06) {
        if (offset + 4 > buf.size()) throw JCBParseException("JCB Decode: Object Length EOF");
        uint32_t len = 0;
        for (int i = 3; i >= 0; --i) len |= ((uint32_t)buf[offset++] << (i * 8));
        json obj = json::object();
        for (uint32_t i = 0; i < len; i++) {
            json k_json = decode_jcb_impl(buf, offset);
            if (!k_json.is_string()) throw JCBParseException("JCB Decode: Non-string key in object");
            std::string k = k_json.get<std::string>();
            obj[k] = decode_jcb_impl(buf, offset);
        }
        return obj;
    }
    throw JCBParseException("JCB Decode: Invalid Tag " + std::to_string(tag));
}

json decode_jcb(const std::vector<uint8_t>& buf) {
    size_t offset = 0;
    json j = decode_jcb_impl(buf, offset);
    if (offset != buf.size()) throw JCBParseException("JCB Decode: Trailing data");
    return j;
}

std::string encode_safe(const json& j) {
    return base64_encode(encode_jcb(j));
}

json decode_safe(const std::string& base64) {
    return decode_jcb(base64_decode(base64));
}

std::string vfs_hash256(const std::vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hash, &sha256);
    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

std::string vfs_hash256_str(const std::string& input) {
    std::vector<uint8_t> data(input.begin(), input.end());
    return vfs_hash256(data);
}

} // namespace fs
