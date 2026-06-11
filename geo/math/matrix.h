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
    std::string s; // The "n/d n/d ..." exact ratio string

    Matrix() : t(CGAL::IDENTITY) {
        update_string();
    }
    Matrix(const Transformation& trans) : t(trans) {
        update_string();
    }
    Matrix(const Transformation& trans, const std::string& tf_str) : t(trans), s(tf_str) {}

    void update_string() {
        std::stringstream ss;
        // Serialize the 3x4 affine matrix (12 coefficients)
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 4; ++j) {
                ss << t.cartesian(i, j) << ( (i == 2 && j == 3) ? "" : " ");
            }
        }
        s = ss.str();
    }

    static Matrix identity() { return Matrix(Transformation(CGAL::IDENTITY)); }

    static Matrix translate(FT x, FT y, FT z) {
        return Matrix(Transformation(CGAL::TRANSLATION, EK::Vector_3(x, y, z)));
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
        return Matrix(Transformation(cos_alpha, -sin_alpha, 0, 0, sin_alpha, cos_alpha, 0, 0, 0, 0, w, 0, w));
    }

    Matrix rotateZ(double turns) const {
        return rotationZ(turns) * (*this);
    }

    // Helper for rotating around a specific 2D point (x, y)
    static Matrix rotateZ(double turns, FT px, FT py) {
        return translate(px, py, 0) * rotationZ(turns) * translate(-px, -py, 0);
    }

    EK::Point_2 transform_2d(const EK::Point_2& p) const {
        auto p3 = t.transform(EK::Point_3(p.x(), p.y(), 0));
        return EK::Point_2(p3.x(), p3.y());
    }

    CGAL::Aff_transformation_2<EK> to_cgal_2d() const {
        // Extract the 2x3 part of the 3x4 affine matrix
        return CGAL::Aff_transformation_2<EK>(
            t.cartesian(0, 0), t.cartesian(0, 1), t.cartesian(0, 3),
            t.cartesian(1, 0), t.cartesian(1, 1), t.cartesian(1, 3),
            t.cartesian(3, 3) // w coefficient if present
        );
    }

    Matrix translated(FT x, FT y, FT z) const {
        return translate(x, y, z) * (*this);
    }

    static Matrix scale(FT x, FT y, FT z) {
        return Matrix(Transformation(x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0));
    }

    static Matrix from_vec(const std::string& tf_str) {
        std::stringstream ss(tf_str);
        std::vector<FT> c;
        FT val;
        while (ss >> val) c.push_back(val);
        if (c.size() == 12) {
            return Matrix(Transformation(c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10], c[11]));
        } else if (c.size() == 16) {
            return Matrix(Transformation(c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10], c[11], c[15]));
        }
        return identity();
    }

    const std::string& to_vec() const {
        return s;
    }

    FT determinant() const {
        // Determinant of the 3x3 linear part
        FT m00 = t.cartesian(0, 0); FT m01 = t.cartesian(0, 1); FT m02 = t.cartesian(0, 2);
        FT m10 = t.cartesian(1, 0); FT m11 = t.cartesian(1, 1); FT m12 = t.cartesian(1, 2);
        FT m20 = t.cartesian(2, 0); FT m21 = t.cartesian(2, 1); FT m22 = t.cartesian(2, 2);
        return m00 * (m11 * m22 - m12 * m21) - 
               m01 * (m10 * m22 - m12 * m20) + 
               m02 * (m10 * m21 - m11 * m20);
    }

    bool is_reflection() const {
        return determinant() < 0;
    }

    EK::Point_3 transform(const EK::Point_3& p) const {
        return t.transform(p);
    }

    EK::Vector_3 transform(const EK::Vector_3& v) const {
        return t.transform(v);
    }

    EK::Plane_3 transform(const EK::Plane_3& p) const {
        return t.transform(p);
    }

    // Creates an EXACTLY ORTHOGONAL matrix that transforms world coordinates into the local space of a plane.
    // Uses a high-precision weight to simulate an orthonormal basis, preventing aspect ratio distortion.
    static Matrix lookAt(const EK::Point_3& origin, const EK::Vector_3& normal) {
        EK::Vector_3 Z_dir = normal;
        
        EK::Vector_3 arbitrary(1, 0, 0);
        if (std::abs(CGAL::to_double(Z_dir.x())) > 0.9) {
            arbitrary = EK::Vector_3(0, 1, 0);
        }

        EK::Vector_3 X_dir = CGAL::cross_product(Z_dir, arbitrary);
        EK::Vector_3 Y_dir = CGAL::cross_product(Z_dir, X_dir);

        double dx = std::sqrt(CGAL::to_double(X_dir.squared_length()));
        double dy = std::sqrt(CGAL::to_double(Y_dir.squared_length()));
        double dz = std::sqrt(CGAL::to_double(Z_dir.squared_length()));

        FT w(1000000);
        auto scale_v = [&](const EK::Vector_3& v, double len) {
            double s = 1000000.0 / len;
            return std::array<FT, 3>{FT(v.x() * s), FT(v.y() * s), FT(v.z() * s)};
        };

        auto nx = scale_v(X_dir, dx);
        auto ny = scale_v(Y_dir, dy);
        auto nz = scale_v(Z_dir, dz);

        Transformation m(
            nx[0], ny[0], nz[0], origin.x() * w,
            nx[1], ny[1], nz[1], origin.y() * w,
            nx[2], ny[2], nz[2], origin.z() * w, w
        );
        
        return Matrix(m.inverse());
    }

    // Creates an EXACTLY ORTHOGONAL matrix where the Z-axis is aligned with the given normal.
    static Matrix fromNormal(const EK::Point_3& origin, const EK::Vector_3& normal) {
        EK::Vector_3 Z_dir = normal;
        
        EK::Vector_3 arbitrary(1, 0, 0);
        if (std::abs(CGAL::to_double(Z_dir.x())) > 0.9) {
            arbitrary = EK::Vector_3(0, 1, 0);
        }

        EK::Vector_3 X_dir = CGAL::cross_product(Z_dir, arbitrary);
        EK::Vector_3 Y_dir = CGAL::cross_product(Z_dir, X_dir);

        double dx = std::sqrt(CGAL::to_double(X_dir.squared_length()));
        double dy = std::sqrt(CGAL::to_double(Y_dir.squared_length()));
        double dz = std::sqrt(CGAL::to_double(Z_dir.squared_length()));

        FT w(1000000);
        auto scale_v = [&](const EK::Vector_3& v, double len) {
            double s = 1000000.0 / len;
            return std::array<FT, 3>{FT(v.x() * s), FT(v.y() * s), FT(v.z() * s)};
        };

        auto nx = scale_v(X_dir, dx);
        auto ny = scale_v(Y_dir, dy);
        auto nz = scale_v(Z_dir, dz);

        return Matrix(Transformation(
            nx[0], ny[0], nz[0], origin.x() * w,
            nx[1], ny[1], nz[1], origin.y() * w,
            nx[2], ny[2], nz[2], origin.z() * w, w
        ));
    }

    Matrix inverse() const {
        return Matrix(t.inverse());
    }

    bool operator==(const Matrix& other) const {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (t.cartesian(i, j) != other.t.cartesian(i, j)) return false;
            }
        }
        return true;
    }

    bool operator!=(const Matrix& other) const {
        return !(*this == other);
    }

    Matrix operator*(const Matrix& other) const {
        return Matrix(t * other.t);
    }
};

} // namespace geo
} // namespace jotcad
