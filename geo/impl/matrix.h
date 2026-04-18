#pragma once
#include <vector>
#include <cmath>
#include <CGAL/Aff_transformation_3.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/rational_rotation.h>

namespace jotcad {
namespace geo {

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef EK::FT FT;
typedef CGAL::Aff_transformation_3<EK> Transformation;

struct Matrix {
    Transformation t;

    Matrix() : t(CGAL::IDENTITY) {}
    Matrix(const Transformation& trans) : t(trans) {}

    static Matrix identity() { return Matrix(Transformation(CGAL::IDENTITY)); }

    static Matrix translate(double x, double y, double z) {
        return Matrix(Transformation(CGAL::TRANSLATION, EK::Vector_3(FT(x), FT(y), FT(z))));
    }

    static Matrix rotationX(double turns) {
        EK::RT sin_alpha, cos_alpha, w;
        double radians = turns * 2.0 * M_PI;
        CGAL::rational_rotation_approximation(radians, sin_alpha, cos_alpha, w, EK::RT(1), EK::RT(1000));
        return Matrix(Transformation(w, 0, 0, 0, 0, cos_alpha, -sin_alpha, 0, 0, sin_alpha, cos_alpha, 0, w));
    }

    static Matrix rotationY(double turns) {
        EK::RT sin_alpha, cos_alpha, w;
        double radians = turns * 2.0 * M_PI;
        CGAL::rational_rotation_approximation(radians, sin_alpha, cos_alpha, w, EK::RT(1), EK::RT(1000));
        return Matrix(Transformation(cos_alpha, 0, -sin_alpha, 0, 0, w, 0, 0, sin_alpha, 0, cos_alpha, 0, w));
    }

    static Matrix rotationZ(double turns) {
        EK::RT sin_alpha, cos_alpha, w;
        double radians = turns * 2.0 * M_PI;
        CGAL::rational_rotation_approximation(radians, sin_alpha, cos_alpha, w, EK::RT(1), EK::RT(1000));
        return Matrix(Transformation(cos_alpha, sin_alpha, 0, 0, -sin_alpha, cos_alpha, 0, 0, 0, 0, w, 0, w));
    }

    static Matrix from_vec(const std::vector<double>& v) {
        if (v.size() < 16) return identity();
        // CGAL(m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, [w])
        // We use CGAL's internal mapping:
        // m00, m01, m02, m03
        // m10, m11, m12, m13
        // m20, m21, m22, m23
        return Matrix(Transformation(
            FT(v[0]), FT(v[1]), FT(v[2]), FT(v[3]),
            FT(v[4]), FT(v[5]), FT(v[6]), FT(v[7]),
            FT(v[8]), FT(v[9]), FT(v[10]), FT(v[11]),
            FT(v[15]) // w
        ));
    }

    std::vector<double> to_vec() const {
        std::vector<double> v(16, 0.0);
        // Map CGAL back to our row-major 4x4 vector
        v[0] = CGAL::to_double(t.cartesian(0, 0));
        v[1] = CGAL::to_double(t.cartesian(0, 1));
        v[2] = CGAL::to_double(t.cartesian(0, 2));
        v[3] = CGAL::to_double(t.cartesian(0, 3));
        v[4] = CGAL::to_double(t.cartesian(1, 0));
        v[5] = CGAL::to_double(t.cartesian(1, 1));
        v[6] = CGAL::to_double(t.cartesian(1, 2));
        v[7] = CGAL::to_double(t.cartesian(1, 3));
        v[8] = CGAL::to_double(t.cartesian(2, 0));
        v[9] = CGAL::to_double(t.cartesian(2, 1));
        v[10] = CGAL::to_double(t.cartesian(2, 2));
        v[11] = CGAL::to_double(t.cartesian(2, 3));
        v[12] = 0; v[13] = 0; v[14] = 0;
        v[15] = CGAL::to_double(t.cartesian(3, 3));
        return v;
    }

    Matrix inverse() const {
        return Matrix(t.inverse());
    }

    Matrix operator*(const Matrix& other) const {
        // Compose transformations
        return Matrix(t * other.t);
    }
};

} // namespace geo
} // namespace jotcad
