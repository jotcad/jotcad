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

    EK::Plane_3 transform(const EK::Plane_3& p) const {
        return t.transform(p);
    }

    // Creates an EXACT orthogonal matrix that transforms world coordinates into the local space of a plane.
    static Matrix lookAt(const EK::Point_3& origin, const EK::Vector_3& normal) {
        // Use an un-normalized but EXACT orthogonal basis.
        EK::Vector_3 Z = normal;
        
        // Find an arbitrary vector that is not parallel to Z
        EK::Vector_3 arbitrary(1, 0, 0);
        if (std::abs(CGAL::to_double(Z.x())) > 0.9) {
            arbitrary = EK::Vector_3(0, 1, 0);
        }

        // Exact cross products for orthogonal basis
        EK::Vector_3 X = CGAL::cross_product(Z, arbitrary);
        EK::Vector_3 Y = CGAL::cross_product(Z, X);

        // Construct the transformation: This matrix takes (1,0,0) to X, (0,1,0) to Y, etc.
        Transformation m(
            X.x(), Y.x(), Z.x(), origin.x(),
            X.y(), Y.y(), Z.y(), origin.y(),
            X.z(), Y.z(), Z.z(), origin.z()
        );
        
        // The lookAt matrix is the INVERSE: it takes world points into this local frame.
        return Matrix(m.inverse());
    }

    // Creates an EXACT orthogonal matrix where the Z-axis is aligned with the given normal.
    static Matrix fromNormal(const EK::Point_3& origin, const EK::Vector_3& normal) {
        EK::Vector_3 Z = normal;
        
        // Find an arbitrary vector that is not parallel to Z
        EK::Vector_3 arbitrary(1, 0, 0);
        if (std::abs(CGAL::to_double(Z.x())) > 0.9) {
            arbitrary = EK::Vector_3(0, 1, 0);
        }

        // Exact cross products for orthogonal basis
        EK::Vector_3 X = CGAL::cross_product(Z, arbitrary);
        EK::Vector_3 Y = CGAL::cross_product(Z, X);

        // Construct the transformation: This matrix takes (1,0,0) to X, (0,1,0) to Y, etc.
        return Matrix(Transformation(
            X.x(), Y.x(), Z.x(), origin.x(),
            X.y(), Y.y(), Z.y(), origin.y(),
            X.z(), Y.z(), Z.z(), origin.z()
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
