#pragma once
#include <vector>
#include <cmath>

namespace jotcad {
namespace geo {

struct Matrix {
    double data[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

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
};

} // namespace geo
} // namespace jotcad
