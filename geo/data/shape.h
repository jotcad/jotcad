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

    std::string role() const {
        if (tags.contains("role") && tags.at("role").is_string()) return tags.at("role").get<std::string>();
        if (tags.value("gap", false)) return "gap"; // Legacy fallback
        return "";
    }

    bool is_gap() const {
        return role() == "gap";
    }

    bool is_ghost() const {
        return role() == "ghost";
    }

    bool is_mask() const {
        return role() == "mask";
    }

    bool is_mark() const {
        return role() == "mark";
    }

    bool is_real() const {
        return role() == "";
    }

    bool has_positive_geometry() const {
        return geometry.has_value() && is_real();
    }

    bool has_negative_geometry() const {
        return geometry.has_value() && is_gap();
    }

    bool has_real_geometry() const {
        return has_positive_geometry() || has_negative_geometry();
    }

    double opacity() const {
        if (tags.contains("opacity") && tags.at("opacity").is_number()) {
            return tags.at("opacity").get<double>();
        }
        std::string r = role();
        if (r == "gap" || r == "ghost") return 0.3;
        if (r == "mask") return 0.0;
        return 1.0;
    }

    bool is_singleton_group() const {
        return !geometry.has_value() && components.size() == 1;
    }

    Shape unwrap_singleton() const {
        if (is_singleton_group()) {
            Shape unwrapped = components[0].unwrap_singleton();
            unwrapped.tf = tf * unwrapped.tf;
            for (auto it = tags.begin(); it != tags.end(); ++it) {
                if (!unwrapped.tags.contains(it.key())) {
                    unwrapped.tags[it.key()] = it.value();
                }
            }
            return unwrapped;
        }
        return *this;
    }

    template <typename F>
    Shape map(F&& fn) const {
        Shape out = fn(*this);
        std::vector<Shape> new_children;
        new_children.reserve(components.size());
        for (const auto& child : components) {
            new_children.push_back(child.map(fn));
        }
        out.components = std::move(new_children);
        return out;
    }

    template <typename Visitor>
    void walk(Visitor&& visitor) const {
        visitor(*this);
        for (const auto& child : components) {
            child.walk(visitor);
        }
    }

    void set_role_recursive(const std::string& r) {
        add_tag("role", r);
        for (auto& child : components) {
            child.set_role_recursive(r);
        }
    }

    void set_tag_recursive(const std::string& key, const nlohmann::json& value) {
        add_tag(key, value);
        for (auto& child : components) {
            child.set_tag_recursive(key, value);
        }
    }

    static Shape make_ghost(const Shape& s) {
        Shape g = s;
        g.set_role_recursive("ghost");
        return g;
    }

    static Shape make_gap(const Shape& s) {
        Shape g = s;
        g.set_role_recursive("gap");
        return g;
    }

    static Shape make_mask(const Shape& s) {
        Shape g = s;
        g.set_role_recursive("mask");
        return g;
    }

    static Shape make_mark(const Shape& s) {
        Shape g = s;
        g.set_role_recursive("mark");
        return g;
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
     * Traverses this shape and all its component children, calling visitor for each shape with its absolute world transform.
     */
    void visit(const std::function<void(const Shape&, const Matrix&)>& visitor) const {
        visitor(*this, tf);
        for (const auto& c : components) {
            c.visit(visitor);
        }
    }

    /**
     * Collects all geometry CIDs in this shape tree along with their absolute world-space transforms.
     */
    std::vector<std::pair<fs::CID, Matrix>> collect_geometry_cids() const {
        std::vector<std::pair<fs::CID, Matrix>> result;
        if (geometry.has_value()) {
            result.push_back({*geometry, tf});
        }
        for (const auto& c : components) {
            auto child_cids = c.collect_geometry_cids();
            result.insert(result.end(), child_cids.begin(), child_cids.end());
        }
        return result;
    }
};

// ADL helpers for nlohmann::json
inline void from_json(const json& j, Shape& s) { s = Shape::from_json(j); }
inline void to_json(json& j, const Shape& s) { j = s.to_json(); }

inline void from_json(const json& j, Operation& op) { op = Operation::from_json(j); }
inline void to_json(json& j, const Operation& op) { j = op.to_json(); }

} // namespace geo
} // namespace jotcad
