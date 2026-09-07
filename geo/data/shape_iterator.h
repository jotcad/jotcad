#pragma once
#include <vector>
#include <iterator>
#include <cstddef>

namespace jotcad {
namespace geo {

struct Shape;

/**
 * BasicShapeIterator: Forward iterator performing depth-first pre-order traversal
 * of a Shape tree (visiting parent before children, in component order).
 */
template <typename ShapeType, bool StopAtItems = false>
class BasicShapeIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ShapeType;
    using difference_type = std::ptrdiff_t;
    using pointer = ShapeType*;
    using reference = ShapeType&;

    BasicShapeIterator() = default;

    explicit BasicShapeIterator(ShapeType* root) {
        if (root) {
            if constexpr (StopAtItems) {
                m_stack.push_back({root, 0});
                descend_to_item();
            } else {
                m_stack.push_back({root, 0});
            }
        }
    }

    reference operator*() const {
        return *m_stack.back().node;
    }

    pointer operator->() const {
        return m_stack.back().node;
    }

    BasicShapeIterator& operator++() {
        if (m_stack.empty()) return *this;

        if constexpr (!StopAtItems) {
            ShapeType* current = m_stack.back().node;
            // 1. Descend to first child if components exist
            if (!current->components.empty()) {
                m_stack.push_back({&current->components[0], 0});
                return *this;
            }

            // 2. Leaf reached: backtrack up the stack to find the next sibling
            while (!m_stack.empty()) {
                m_stack.pop_back();
                if (m_stack.empty()) break;

                auto& parent_frame = m_stack.back();
                parent_frame.child_index++;
                if (parent_frame.child_index < parent_frame.node->components.size()) {
                    m_stack.push_back({&parent_frame.node->components[parent_frame.child_index], 0});
                    break;
                }
            }
            return *this;
        } else {
            // In StopAtItems mode: current node was an item or de facto item.
            // Do NOT descend into components; advance to next item/leaf.
            advance_next();
            return *this;
        }
    }

    BasicShapeIterator operator++(int) {
        BasicShapeIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const BasicShapeIterator& other) const {
        if (m_stack.empty() && other.m_stack.empty()) return true;
        if (m_stack.empty() != other.m_stack.empty()) return false;
        return m_stack.back().node == other.m_stack.back().node &&
               m_stack.size() == other.m_stack.size();
    }

    bool operator!=(const BasicShapeIterator& other) const {
        return !(*this == other);
    }

private:
    struct Frame {
        ShapeType* node;
        std::size_t child_index;
    };
    std::vector<Frame> m_stack;

    static bool is_container(ShapeType* node) {
        return !node->is_item() && !node->geometry.has_value() && !node->components.empty();
    }

    static bool is_valid_item(ShapeType* node) {
        return node->is_item() || node->geometry.has_value();
    }

    void descend_to_item() {
        while (!m_stack.empty()) {
            ShapeType* current = m_stack.back().node;
            if (is_container(current)) {
                m_stack.push_back({&current->components[0], 0});
            } else if (!is_valid_item(current)) {
                // Empty placeholder node (no geometry, not item, no components): advance past it
                advance_next();
                break;
            } else {
                break;
            }
        }
    }

    void advance_next() {
        while (!m_stack.empty()) {
            m_stack.pop_back();
            if (m_stack.empty()) break;

            auto& parent_frame = m_stack.back();
            parent_frame.child_index++;
            if (parent_frame.child_index < parent_frame.node->components.size()) {
                m_stack.push_back({&parent_frame.node->components[parent_frame.child_index], 0});
                descend_to_item();
                return;
            }
        }
    }
};

using ShapeIterator = BasicShapeIterator<Shape, false>;
using ConstShapeIterator = BasicShapeIterator<const Shape, false>;
using ItemShapeIterator = BasicShapeIterator<Shape, true>;
using ConstItemShapeIterator = BasicShapeIterator<const Shape, true>;

} // namespace geo
} // namespace jotcad

#include "shape.h"

namespace jotcad {
namespace geo {

inline ConstShapeIterator Shape::begin() const { return ConstShapeIterator(this); }
inline ConstShapeIterator Shape::end() const { return ConstShapeIterator(nullptr); }
inline ConstShapeIterator Shape::cbegin() const { return ConstShapeIterator(this); }
inline ConstShapeIterator Shape::cend() const { return ConstShapeIterator(nullptr); }

inline ShapeIterator Shape::begin() { return ShapeIterator(this); }
inline ShapeIterator Shape::end() { return ShapeIterator(nullptr); }

inline ShapeRange<ShapeIterator> Shape::shapes() { return {begin(), end()}; }
inline ShapeRange<ConstShapeIterator> Shape::shapes() const { return {begin(), end()}; }

inline ShapeRange<ItemShapeIterator> Shape::items() { return {ItemShapeIterator(this), ItemShapeIterator(nullptr)}; }
inline ShapeRange<ConstItemShapeIterator> Shape::items() const { return {ConstItemShapeIterator(this), ConstItemShapeIterator(nullptr)}; }

} // namespace geo
} // namespace jotcad
