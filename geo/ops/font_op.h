#pragma once
#include "../core/protocols.h"
#include "../core/processor.h"
#include "../../fs/cpp/vendor/httplib.h"
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct FontOp : P {
    static constexpr const char* path = "jot/Font";

    static bool is_valid_font(const std::vector<uint8_t>& data) {
        if (data.size() < 4) return false;
        if (data[0] == 0x00 && data[1] == 0x01 && data[2] == 0x00 && data[3] == 0x00) return true; // TTF
        if (data[0] == 'O' && data[1] == 'T' && data[2] == 'T' && data[3] == 'O') return true;     // OTF
        if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == 'F') return true;     // WOFF
        if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == '2') return true;     // WOFF2
        if (data[0] == 't' && data[1] == 'r' && data[2] == 'u' && data[3] == 'e') return true;     // Apple TTF
        return false;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::string& url) {
        std::string host, path_str = "/", proto = "http";
        size_t proto_pos = url.find("://");
        std::string base = url;
        if (proto_pos != std::string::npos) {
            proto = url.substr(0, proto_pos);
            base = url.substr(proto_pos + 3);
        }
        size_t slash_pos = base.find('/');
        if (slash_pos != std::string::npos) {
            host = base.substr(0, slash_pos);
            path_str = base.substr(slash_pos);
        } else { host = base; }

        std::string scheme_host = proto + "://" + host;
        httplib::Client cli(scheme_host);
        cli.set_follow_location(true);
        cli.enable_server_certificate_verification(false);
        cli.set_keep_alive(true);
        cli.set_connection_timeout(5, 0); // 5 seconds
        auto res = cli.Get(path_str.c_str());

        
        if (res && res->status == 200) {
            std::vector<uint8_t> data(res->body.begin(), res->body.end());
            if (is_valid_font(data)) {
                vfs->write_bytes(fulfilling.with_output("$out"), data);
            } else {
                throw std::runtime_error("[FontOp] REJECTED: Magic Mismatch at " + url);
            }
        } else {
            throw std::runtime_error("[FontOp] Failed to download from " + url + " (Status: " + (res ? std::to_string(res->status) : "Failed") + ")");
        }
    }

    static std::vector<std::string> argument_keys() { return {"url"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Font"},
            {"description", "Downloads and validates a font file from a URL."},
            {"arguments", {{{"name", "url"}, {"type", "jot:string"}}}},
            {"outputs", {{"$out", {{"type", "jot:font"}}}}}
        };
    }
};

inline void font_init(fs::VFSNode* vfs) {
    Processor::register_op<FontOp<>, std::string>(vfs, "jot/Font");
}

} // namespace geo
} // namespace jotcad
