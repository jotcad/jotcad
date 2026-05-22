#pragma once
#include "../core/protocols.h"
#include "../core/processor.h"
#include "../infra/fetch.h"
#include <vector>
#include <string>
#include <stdexcept>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct FontOp : P {
    static constexpr const char* path = "jot/Font";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::string& url) {
        auto bytes = Fetcher::fetch_url(url);
        bool valid = false;
        if (bytes.size() >= 4) {
            uint32_t magic = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
            if (magic == 0x00010000 || magic == 0x74727565 || magic == 0x4f54544f || magic == 0x774f4646 || magic == 0x774f4632) valid = true;
        }
        if (!valid) throw std::runtime_error("Invalid font format at " + url);
        vfs->write(fulfilling.with_output("$out"), bytes);
    }
    static std::vector<std::string> argument_keys() { return {"url"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Font"},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({{{"name", "url"}, {"type", "jot:string"}}})},
            {"outputs", {{"$out", {{"type", "jot:font"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ImageOp : P {
    static constexpr const char* path = "jot/Image";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::string& url) {
        auto bytes = Fetcher::fetch_url(url);
        bool valid = false;
        if (bytes.size() >= 3) {
            if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E) valid = true; // PNG
            if (bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) valid = true; // JPG
        }
        if (!valid) throw std::runtime_error("Invalid image format at " + url);
        vfs->write(fulfilling.with_output("$out"), bytes);
    }
    static std::vector<std::string> argument_keys() { return {"url"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Image"},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({{{"name", "url"}, {"type", "jot:string"}}})},
            {"outputs", {{"$out", {{"type", "jot:image"}}}}}
        };
    }
};

inline void assets_init(fs::VFSNode* vfs) {
    Processor::register_op<FontOp<>, std::string>(vfs, "jot/Font");
    Processor::register_op<ImageOp<>, std::string>(vfs, "jot/Image");
}

} // namespace geo
} // namespace jotcad
