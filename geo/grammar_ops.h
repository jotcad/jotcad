#pragma once

#include "impl/processor.h"
#include <sstream>
#include <vector>
#include <set>
#include <iomanip>
#include <thread>
#include <chrono>

namespace jotcad {
namespace geo {

static std::string to_string(const std::vector<uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

static std::vector<uint8_t> to_bytes(const std::string& s) {
    try {
        if (s.size() > (size_t)1024 * 1024 * 1024) {
             throw std::length_error("String size exceeds 1GB for vector conversion");
        }
        return std::vector<uint8_t>(s.begin(), s.end());
    } catch (const std::exception& e) {
        std::cerr << "[to_bytes] CRITICAL ERROR: " << e.what() << " (size: " << s.size() << ")" << std::endl;
        throw;
    }
}

static std::string read_to_string(jotcad::fs::VFSClient* vfs, const nlohmann::json& selector, const std::vector<std::string>& stack = {}) {
    std::string path = selector.at("path");
    nlohmann::json params = selector.value("parameters", nlohmann::json::object());
    
    auto data = vfs->read(path, params, stack);
    if (data.empty()) return "";

    std::string text = to_string(data);
    
    // Check if result is Shape JSON
    try {
        auto j = nlohmann::json::parse(text);
        if (j.contains("geometry")) {
            std::string geo_url = j["geometry"];
            if (geo_url.compare(0, 5, "vfs:/") == 0) {
                std::string geo_path = geo_url.substr(5);
                // Resolve the mesh data
                return read_to_string(vfs, {{"path", geo_path}, {"parameters", params}}, stack);
            }
        }
    } catch (...) {
        // Not JSON or doesn't have geometry, return raw text (e.g. OBJ)
    }

    return text;
}

static void points_init() {
    Processor::Operation op;
    op.path = "op/points";
    op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::string geo = read_to_string(vfs, params.at("$in"), stack);
        if (geo.empty()) return to_bytes("");

        std::stringstream ss(geo);
        std::string line;
        std::stringstream out;
        out << std::fixed << std::setprecision(6);
        std::set<std::string> unique_points;
        while (std::getline(ss, line)) {
            if (line.compare(0, 2, "v ") == 0) {
                if (unique_points.find(line) == unique_points.end()) {
                    unique_points.insert(line);
                    out << line << "\n";
                }
            }
        }
        return to_bytes(out.str());
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "shape"}}}
        }},
        {"inputs", {
            {"$in", {{"type", "shape"}}}
        }},
        {"outputs", {
            {"$out", {{"type", "geometry"}}}
        }}
    };
    Processor::register_op(op);
}

static void nth_init() {
    Processor::Operation op;
    op.path = "op/nth";
    op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::string geo = read_to_string(vfs, params.at("$in"), stack);
        auto indices = params.at("indices").get<std::vector<int>>();
        if (geo.empty()) return to_bytes("");

        std::stringstream ss(geo);
        std::string line;
        std::vector<std::string> items;
        while (std::getline(ss, line)) {
            if (!line.empty()) items.push_back(line);
        }
        
        std::stringstream out;
        for (int idx : indices) {
            if (idx >= 0 && idx < items.size()) {
                out << items[idx] << "\n";
            }
        }
        return to_bytes(out.str());
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "shape"}}},
            {"indices", {{"type", "array"}, {"items", {{"type", "integer"}}}}}
        }},
        {"inputs", {
            {"$in", {{"type", "shape"}}}
        }},
        {"outputs", {
            {"$out", {{"type", "geometry"}}}
        }}
    };
    Processor::register_op(op);
}

static void group_init() {
    Processor::Operation op;
    op.path = "op/group";
    op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        auto sources = params.at("$in").get<std::vector<nlohmann::json>>();
        std::stringstream out;
        for (const auto& s : sources) {
            out << read_to_string(vfs, s, stack) << "\n";
        }
        
        vfs->write("geo/mesh", params, to_bytes(out.str()));
        nlohmann::json shape = {
            {"geometry", "vfs:/geo/mesh"},
            {"parameters", params},
            {"tags", {{"type", "group"}}}
        };
        return to_bytes(shape.dump());
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "array"}, {"items", {{"type", "shape"}}}}}
        }},
        {"inputs", {
            {"$in", {{"type", "array"}, {"items", {{"type", "shape"}}}}}
        }},
        {"outputs", {
            {"$out", {{"type", "shape"}}}
        }}
    };
    Processor::register_op(op);
}

static void loop_init() {
    auto create_op = [](const std::string& name, bool closed) {
        Processor::Operation op;
        op.path = name;
        op.logic = [closed](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
            std::string geo = read_to_string(vfs, params.at("$in"), stack);
            if (geo.empty()) return to_bytes("");

            std::stringstream ss(geo);
            std::string line;
            std::vector<std::string> points;
            while (std::getline(ss, line)) {
                if (line.compare(0, 2, "v ") == 0) points.push_back(line);
            }
            
            if (points.size() < 2) return to_bytes(geo);

            std::stringstream out;
            for (const auto& p : points) out << p << "\n";
            for (size_t i = 0; i < points.size() - 1; ++i) {
                out << "l " << (i + 1) << " " << (i + 2) << "\n";
            }
            if (closed && points.size() > 2) {
                out << "l " << points.size() << " 1\n";
            }
            
            vfs->write("geo/mesh", params, to_bytes(out.str()));
            nlohmann::json shape = {
                {"geometry", "vfs:/geo/mesh"},
                {"parameters", params},
                {"tags", {{"type", "path"}}}
            };
            return to_bytes(shape.dump());
        };
        op.schema = {
            {"arguments", {
                {"$in", {{"type", "shape"}}}
            }},
            {"inputs", {
                {"$in", {{"type", "shape"}}}
            }},
            {"outputs", {
                {"$out", {{"type", "shape"}}}
            }}
        };
        return op;
    };

    Processor::register_op(create_op("op/link", false));
    Processor::register_op(create_op("op/loop", true));
}

static void grammar_init() {
    points_init();
    nth_init();
    group_init();
    loop_init();
}

} // namespace geo
} // namespace jotcad
