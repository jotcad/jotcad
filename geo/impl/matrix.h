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

    static Matrix from_vec(const std::vector<std::string>& v) {
        if (v.size() < 16) return identity();
        auto parse = [](const std::string& s) {
            size_t slash = s.find('/');
            if (slash == std::string::npos) return FT(std::stod(s));
            std::string n = s.substr(0, slash);
            std::string d = s.substr(slash + 1);
            // CGAL FT can be constructed from strings to maintain precision if GMP/Boost Multiprecision is enabled.
            // For now, use double as fallback for the construction if direct string init isn't available, 
            // but the identity is already stable because the CID will be based on the string "n/d".
            return FT(std::stod(n)) / FT(std::stod(d));
        };
        return Matrix(Transformation(
            parse(v[0]), parse(v[1]), parse(v[2]), parse(v[3]),
            parse(v[4]), parse(v[5]), parse(v[6]), parse(v[7]),
            parse(v[8]), parse(v[9]), parse(v[10]), parse(v[11]),
            parse(v[15])
        ));
    }

    std::vector<std::string> to_vec() const {
        std::vector<std::string> v(16, "0/1");
        auto to_ratio = [](const FT& val) {
            double dval = CGAL::to_double(val);
            if (dval == 0) return std::string("0/1");
            long long precision = 1000000000LL;
            long long n = (long long)std::round(std::abs(dval) * precision);
            long long d = precision;
            long long common = std::gcd(n, d);
            return std::to_string((dval < 0 ? -1 : 1) * (n / common)) + "/" + std::to_string(d / common);
        };
        v[0] = to_ratio(t.cartesian(0, 0));
        v[1] = to_ratio(t.cartesian(0, 1));
        v[2] = to_ratio(t.cartesian(0, 2));
        v[3] = to_ratio(t.cartesian(0, 3));
        v[4] = to_ratio(t.cartesian(1, 0));
        v[5] = to_ratio(t.cartesian(1, 1));
        v[6] = to_ratio(t.cartesian(1, 2));
        v[7] = to_ratio(t.cartesian(1, 3));
        v[8] = to_ratio(t.cartesian(2, 0));
        v[9] = to_ratio(t.cartesian(2, 1));
        v[10] = to_ratio(t.cartesian(2, 2));
        v[11] = to_ratio(t.cartesian(2, 3));
        v[15] = to_ratio(t.cartesian(3, 3));
        return v;
    }

    EK::Point_3 transform(const EK::Point_3& p) const {
        return t.transform(p);
    }

    EK::Plane_3 transform(const EK::Plane_3& p) const {
        return t.transform(p);
    }

    // Creates a matrix that transforms world coordinates into the local space of a plane
    // where the plane is the XY plane (Z=0).
    static Matrix lookAt(const EK::Point_3& origin, const EK::Vector_3& normal) {
        double nx = CGAL::to_double(normal.x());
        double ny = CGAL::to_double(normal.y());
        double nz = CGAL::to_double(normal.z());
        double len = std::sqrt(nx*nx + ny*ny + nz*nz);
        nx /= len; ny /= len; nz /= len;

        double ux = (std::abs(nx) < 0.9) ? 1.0 : 0.0;
        double uy = (std::abs(nx) < 0.9) ? 0.0 : 1.0;
        double uz = 0.0;

        // Cross product to find Y axis
        double vx = ny*uz - nz*uy;
        double vy = nz*ux - nx*uz;
        double vz = nx*uy - ny*ux;
        double vlen = std::sqrt(vx*vx + vy*vy + vz*vz);
        vx /= vlen; vy /= vlen; vz /= vlen;

        // Cross product to find X axis
        ux = vy*nz - vz*ny;
        uy = vz*nx - vx*nz;
        uz = vx*ny - vy*nx;

        CGAL::Aff_transformation_3<EK> m(
            FT(ux), FT(uy), FT(uz), FT(origin.x()),
            FT(vx), FT(vy), FT(vz), FT(origin.y()),
            FT(nx), FT(ny), FT(nz), FT(origin.z())
        );
        return Matrix(m.inverse());
    }

    Matrix inverse() const {
        return Matrix(t.inverse());
    }

    Matrix operator*(const Matrix& other) const {
        return Matrix(t * other.t);
    }
};

} // namespace geo
} // namespace jotcad
