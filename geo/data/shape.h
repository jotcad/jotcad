#pragma once
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <iostream>
#include <json.hpp>
#include "vfs_node.h"
#include "../math/matrix.h"

namespace jotcad {
namespace geo {

using json = nlohmann::json;

/**
 * Operation: A semantic recipe for a JOT operation.
 * Inherits from Selector but is distinguishable at the type level.
 */
struct Operation : fs::Selector {
    Operation() = default;
    Operation(const std::string& p, const nlohmann::json& params) {
        path = p;
        parameters = params;
    }
    
    static Operation from_json(const json& j) {
        if (j.is_string()) return Operation(j.get<std::string>(), json::object());
        if (!j.contains("path")) return Operation("", json::object());
        return Operation(j.at("path").get<std::string>(), j.value("parameters", json::object()));
    }
};

/**
 * Shape: The foundational semantic container for JOT.
 */
struct Shape {
    std::optional<fs::CID> geometry;
    Matrix tf;
    nlohmann::json tags = nlohmann::json::object();
    std::vector<Shape> components;

    void add_tag(const std::string& key, const nlohmann::json& value) {
        tags[key] = value;
    }

    nlohmann::json to_json() const {
        nlohmann::json j = {{"tf", tf.to_vec()}, {"tags", tags}};
        if (geometry.has_value()) j["geometry"] = geometry->to_json();
        if (!components.empty()) {
            j["components"] = nlohmann::json::array();
            for (const auto& c : components) j["components"].push_back(c.to_json());
        }
        return j;
    }

    static Shape from_json(const nlohmann::json& j) {
        Shape s;
        try {
            if (j.contains("geometry") && !j.at("geometry").is_null()) {
                s.geometry = fs::CID::from_json(j.at("geometry"));
            }
            if (j.contains("tf")) {
                if (j.at("tf").is_string()) {
                    s.tf = Matrix::from_vec(j.at("tf").get<std::string>());
                } else if (j.at("tf").is_array()) {
                    // Compatibility: Convert legacy array
                    std::stringstream ss;
                    for (const auto& val : j.at("tf")) {
                        if (val.is_string()) ss << val.get<std::string>() << " ";
                        else if (val.is_number()) ss << val.get<double>() << "/1 ";
                    }
                    s.tf = Matrix::from_vec(ss.str());
                }
            }
            if (j.contains("tags") && j.at("tags").is_object()) {
                s.tags = j.at("tags");
            }
            if (j.contains("components") && j.at("components").is_array()) {
                for (const auto& c : j.at("components")) s.components.push_back(from_json(c));
            }
        } catch (const std::exception& e) {
            std::cerr << "[Shape::from_json] ERROR: " << e.what() << " in JSON: " << j.dump() << std::endl;
            throw;
        }
        return s;
    }

    static Shape group(const std::vector<Shape>& components) {
        Shape s;
        s.components = components;
        return s;
    }

    /**
     * Recursively applies a relative transformation matrix to this shape and all its children.
     * This maintains the "Independent Matrix" mandate where every component's tf is its absolute world-space matrix.
     */
    void apply_transform(const Matrix& m) {
        tf = m * tf;
        for (auto& c : components) {
            c.apply_transform(m);
        }
    }

    /**
     * Recursively sets a new absolute world-space matrix for this shape, maintaining the 
     * relative relationships of all its children.
     */
    void apply_absolute_transform(const Matrix& absolute) {
        Matrix delta = absolute * tf.inverse();
        apply_transform(delta);
    }
};

// ADL helpers for nlohmann::json
inline void from_json(const json& j, Shape& s) { s = Shape::from_json(j); }
inline void to_json(json& j, const Shape& s) { j = s.to_json(); }

inline void from_json(const json& j, Operation& op) { op = Operation::from_json(j); }
inline void to_json(json& j, const Operation& op) { j = op.to_json(); }

} // namespace geo
} // namespace jotcad
