#pragma once
#include "protocols.h"
#include <cmath>

namespace jotcad {
namespace geo {

// Exact rational approximation for Pi (355/113 is very good)
static const FT RATIONAL_PI = FT(355) / FT(113);
static const FT RATIONAL_TWO_PI = RATIONAL_PI * FT(2);

// High precision rational sin/cos approximations for arbitrary turns (0 to 1)
inline std::pair<FT, FT> get_approx_sincos(double turns) {
    double theta = turns * 2.0 * M_PI;
    // We use a high-precision denominator for rational approximation
    const long long PREC = 10000000000LL;
    FT s = FT((long long)(std::sin(theta) * PREC)) / FT(PREC);
    FT c = FT((long long)(std::cos(theta) * PREC)) / FT(PREC);
    return {s, c};
}

} // namespace geo
} // namespace jotcad
