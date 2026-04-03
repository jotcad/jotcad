#pragma once

#include <vector>
#include <string>
#include <array>
#include <iostream>

namespace jotcad {
namespace geo {

struct Vertex {
    double x, y, z;
};

struct Geometry {
    std::vector<Vertex> vertices;
    std::vector<int> points;
    std::vector<std::array<int, 2>> segments;
    std::vector<std::array<int, 3>> triangles;
    
    // A face is a collection of loops (indices into vertices).
    // The first loop is the outer boundary, subsequent are holes.
    struct Face {
        std::vector<std::vector<int>> loops;
    };
    std::vector<Face> faces;

    /**
     * Encodes the geometry into the .jot text format.
     */
    std::string encode_text() const;

    /**
     * Decodes the geometry from the .jot text format.
     */
    static Geometry decode_text(const std::string& text);
};

} // namespace geo
} // namespace jotcad
