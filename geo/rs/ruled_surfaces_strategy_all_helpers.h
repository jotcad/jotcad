#pragma once

#include <CGAL/determinant.h>

#include <cassert>
#include <cmath>
#include <functional>

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_strategy_linear_helpers.h"
#include "visitor.h"

namespace ruled_surfaces {
namespace internal {

template <typename Objective, typename Visitor>
inline void triangulate_exhaustive(const PolygonalChain& p,
                                   const PolygonalChain& q,
                                   const Objective& objective,
                                   Visitor& visitor) {
  size_t m = p.size();
  size_t n = q.size();
  if (visitor.OnStart(p, q) == RuledSurfaceVisitor::kStop) return;
  if (m < 1 || n < 1) return;
  if (m == 1 && n == 1) return;

  std::vector<bool> moves(m + n - 2);
  for (size_t i = 0; i < n - 1; ++i) moves[i] = false;
  for (size_t i = n - 1; i < m + n - 2; ++i) moves[i] = true;

  do {
    int ci = 0, cj = 0;
    PolygonSoup soup;
    bool possible = true;
    bool rejected = false;
    for (bool is_p_succeed : moves) {
      // If ambiguity arises from a coplanar quad, canonically choose P-moves
      // by rejecting paths that take the Q-move in that situation.
      if (!is_p_succeed && ci + 1 < m && cj + 1 < n &&
          std::abs(CGAL::determinant(q[cj] - p[ci], p[ci + 1] - p[ci],
                                     q[cj + 1] - p[ci])) < 1e-7) {
        rejected = true;
        break;
      }
      if (is_p_succeed) {
        if (ci + 1 >= m) {
          possible = false;
          break;
        }
        const auto& p1 = p[ci];
        const auto& p2 = q[cj];
        const auto& p3 = p[ci + 1];
        assert(p1 != p2 && p2 != p3 && p1 != p3);
        assert(!internal::is_degenerate(p1, p2, p3));
        soup.push_back({p1, p2, p3});
        ci++;
      } else {
        if (cj + 1 >= n) {
          possible = false;
          break;
        }
        const auto& p1 = p[ci];
        const auto& p2 = q[cj];
        const auto& p3 = q[cj + 1];
        assert(p1 != p2 && p2 != p3 && p1 != p3);
        assert(!internal::is_degenerate(p1, p2, p3));
        soup.push_back({p1, p2, p3});
        cj++;
      }
    }

    if (!possible || rejected) continue;

    Mesh current_mesh = soup_to_mesh(soup);
    assert(!is_degenerate_mesh(current_mesh));
    if (visitor.OnPermutation(current_mesh) == RuledSurfaceVisitor::kStop)
      return;
    if (has_self_intersections(current_mesh)) continue;

    double current_cost = objective.calculate_cost(current_mesh);
    if (visitor.OnValidMesh(current_mesh, current_cost, moves) ==
        RuledSurfaceVisitor::kStop)
      return;
  } while (std::next_permutation(moves.begin(), moves.end()));
}

}  // namespace internal
}  // namespace ruled_surfaces
