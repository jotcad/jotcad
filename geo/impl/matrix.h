#pragma once
#include <vector>
#include "kernel.h"

namespace jotcad {
namespace geo {

/**
 * Matrix: Exact transformation helper wrapper around CGAL::Aff_transformation_3.
 */
struct Matrix {
    Transformation t;

    Matrix() : t(CGAL::IDENTITY) {}
    Matrix(const Transformation& trans) : t(trans) {}

    static Matrix identity() { return Matrix(); }

    static Matrix translate(double x, double y, double z) {
        return Matrix(Transformation(CGAL::TRANSLATION, Vector_3(FT(x), FT(y), FT(z))));
    }

    static Matrix rotateX(double rad_turns) {
        // CGAL doesn't have a direct rad_turns rotate, we'll use double for the trig then convert to exact if needed,
        // or just construct the rotation matrix.
        double rad = rad_turns * 2.0 * M_PI;
        double c = std::cos(rad);
        double s = std::sin(rad);
        return Matrix(Transformation(1, 0, 0, 0,
                                     0, c, -s, 0,
                                     0, s, c, 0));
    }

    static Matrix rotateY(double rad_turns) {
        double rad = rad_turns * 2.0 * M_PI;
        double c = std::cos(rad);
        double s = std::sin(rad);
        return Matrix(Transformation(c, 0, s, 0,
                                     0, 1, 0, 0,
                                     -s, 0, c, 0));
    }

    static Matrix rotateZ(double rad_turns) {
        double rad = rad_turns * 2.0 * M_PI;
        double c = std::cos(rad);
        double s = std::sin(rad);
        return Matrix(Transformation(c, -s, 0, 0,
                                     s, c, 0, 0,
                                     0, 0, 1, 0));
    }

    Matrix operator*(const Matrix& other) const {
        return Matrix(t * other.t);
    }

    Matrix inverse() const {
        return Matrix(t.inverse());
    }

    std::vector<double> to_vec() const {
        // We export back to the standard 16-element row-major vector for JOT JSON compatibility
        // Row 0
        double m00 = CGAL::to_double(t.m(0, 0));
        double m01 = CGAL::to_double(t.m(0, 1));
        double m02 = CGAL::to_double(t.m(0, 2));
        double m03 = CGAL::to_double(t.m(0, 3));
        // Row 1
        double m10 = CGAL::to_double(t.m(1, 0));
        double m11 = CGAL::to_double(t.m(1, 1));
        double m12 = CGAL::to_double(t.m(1, 2));
        double m13 = CGAL::to_double(t.m(1, 3));
        // Row 2
        double m20 = CGAL::to_double(t.m(2, 0));
        double m21 = CGAL::to_double(t.m(2, 1));
        double m22 = CGAL::to_double(t.m(2, 2));
        double m23 = CGAL::to_double(t.m(2, 3));
        
        return {
            m00, m01, m02, m03,
            m10, m11, m12, m13,
            m20, m21, m22, m23,
            0.0, 0.0, 0.0, 1.0
        };
    }

    static Matrix from_vec(const std::vector<double>& v) {
        if (v.size() < 12) return Matrix();
        return Matrix(Transformation(v[0], v[1], v[2], v[3],
                                     v[4], v[5], v[6], v[7],
                                     v[8], v[9], v[10], v[11]));
    }
};

} // namespace geo
} // namespace jotcad
