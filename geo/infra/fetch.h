#pragma once
#include "../../fs/cpp/vendor/httplib.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace jotcad {
namespace geo {

class Fetcher {
public:
    static std::vector<uint8_t> fetch_url(const std::string& url) {
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
        cli.set_connection_timeout(5, 0);

        auto res = cli.Get(path_str.c_str());
        if (res && res->status == 200) {
            return std::vector<uint8_t>(res->body.begin(), res->body.end());
        }
        throw std::runtime_error("Failed to download from " + url + " (Status: " + (res ? std::to_string(res->status) : "Failed") + ")");
    }
};

} // namespace geo
} // namespace jotcad
