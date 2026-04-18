#pragma once
#include <vector>
#include <cmath>
#include <stdexcept>

namespace jotcad {
namespace geo {

struct Matrix {
    double data[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    static Matrix identity() { return Matrix(); }

    static Matrix translate(double x, double y, double z) {
        Matrix m;
        m.data[3] = x; m.data[7] = y; m.data[11] = z;
        return m;
    }

    static Matrix rotateX(double rad) {
        Matrix m;
        double c = std::cos(rad);
        double s = std::sin(rad);
        m.data[5] = c; m.data[6] = -s;
        m.data[9] = s; m.data[10] = c;
        return m;
    }

    static Matrix rotateY(double rad) {
        Matrix m;
        double c = std::cos(rad);
        double s = std::sin(rad);
        m.data[0] = c; m.data[2] = s;
        m.data[8] = -s; m.data[10] = c;
        return m;
    }

    static Matrix rotateZ(double rad) {
        Matrix m;
        double c = std::cos(rad);
        double s = std::sin(rad);
        m.data[0] = c; m.data[1] = -s;
        m.data[4] = s; m.data[5] = c;
        return m;
    }

    Matrix operator*(const Matrix& other) const {
        Matrix result;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                double sum = 0;
                for (int k = 0; k < 4; k++) {
                    sum += data[i * 4 + k] * other.data[k * 4 + j];
                }
                result.data[i * 4 + j] = sum;
            }
        }
        return result;
    }

    // Basic Inverse (for rigid body transforms: rotation + translation)
    Matrix inverse() const {
        Matrix res;
        // Transpose of 3x3 rotation part
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                res.data[i * 4 + j] = data[j * 4 + i];
            }
        }
        // New translation: -R^T * t
        double tx = data[3], ty = data[7], tz = data[11];
        res.data[3] = -(res.data[0] * tx + res.data[1] * ty + res.data[2] * tz);
        res.data[7] = -(res.data[4] * tx + res.data[5] * ty + res.data[6] * tz);
        res.data[11] = -(res.data[8] * tx + res.data[9] * ty + res.data[10] * tz);
        return res;
    }

    std::vector<double> to_vec() const {
        return std::vector<double>(data, data + 16);
    }

    static Matrix from_vec(const std::vector<double>& v) {
        Matrix m;
        if (v.size() == 16) for (int i = 0; i < 16; i++) m.data[i] = v[i];
        return m;
    }
};

} // namespace geo
} // namespace jotcad
