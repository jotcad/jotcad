#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Simple_cartesian.h>

#include <cmath>
#include <vector>

#include "geometry.h"

namespace geometry {

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Exact_predicates_inexact_constructions_kernel IK;
typedef CGAL::Simple_cartesian<double> CK;

enum class SweepStrategy { SIMPLE = 0, MITER = 1, ROUND = 2 };

struct Frame {
  EK::Point_3 position;
  EK::Vector_3 tangent;
  EK::Vector_3 normal;
  EK::Vector_3 binormal;

  EK::Point_3 transform(const EK::Point_3& p) const {
    return position + CGAL::to_double(p.x()) * normal +
           CGAL::to_double(p.y()) * binormal + CGAL::to_double(p.z()) * tangent;
  }

  void twist(double theta) {
    double c = std::cos(theta);
    double s = std::sin(theta);
    EK::Vector_3 new_n = c * normal + s * binormal;
    EK::Vector_3 new_b = -s * normal + c * binormal;
    normal = new_n;
    binormal = new_b;
  }
};

static std::vector<Frame> generate_frames(const std::vector<EK::Point_3>& path,
                                          bool closed) {
  std::vector<Frame> frames;
  if (path.size() < 2) return frames;

  frames.reserve(path.size());

  EK::Point_3 p0 = path[0];
  EK::Point_3 p1 = path[1];
  EK::Vector_3 t0 = p1 - p0;
  double t0_len_sq = CGAL::to_double(t0.squared_length());
  if (t0_len_sq > 0) t0 = t0 / std::sqrt(t0_len_sq);

  EK::Vector_3 n0;
  if (std::abs(CGAL::to_double(t0.x())) < 0.9) {
    n0 = CGAL::cross_product(t0, EK::Vector_3(1, 0, 0));
  } else {
    n0 = CGAL::cross_product(t0, EK::Vector_3(0, 1, 0));
  }
  n0 = n0 / std::sqrt(CGAL::to_double(n0.squared_length()));
  EK::Vector_3 b0 = CGAL::cross_product(t0, n0);
  frames.push_back({p0, t0, n0, b0});

  for (size_t i = 1; i < path.size(); ++i) {
    EK::Point_3 prev_p = path[i - 1];
    EK::Point_3 curr_p = path[i];
    EK::Vector_3 prev_t = frames.back().tangent;
    EK::Vector_3 prev_n = frames.back().normal;

    EK::Vector_3 v1 = curr_p - prev_p;
    double c1 = CGAL::to_double(v1.squared_length());
    if (c1 == 0) {
      frames.push_back(frames.back());
      frames.back().position = curr_p;
      continue;
    }

    EK::Vector_3 n_prev_reflected =
        prev_n -
        (2.0 / c1) * CGAL::to_double(CGAL::scalar_product(v1, prev_n)) * v1;
    EK::Vector_3 t_prev_reflected =
        prev_t -
        (2.0 / c1) * CGAL::to_double(CGAL::scalar_product(v1, prev_t)) * v1;

    EK::Vector_3 next_t;
    if (i < path.size() - 1) {
      next_t = path[i + 1] - curr_p;
    } else if (closed) {
      next_t = path[0] - curr_p;
    } else {
      next_t = prev_t;
    }
    double next_t_len_sq = CGAL::to_double(next_t.squared_length());
    if (next_t_len_sq > 0) next_t = next_t / std::sqrt(next_t_len_sq);

    EK::Vector_3 v2 = next_t - t_prev_reflected;
    double c2 = CGAL::to_double(v2.squared_length());

    EK::Vector_3 next_n;
    if (c2 == 0) {
      next_n = n_prev_reflected;
    } else {
      next_n = n_prev_reflected -
               (2.0 / c2) *
                   CGAL::to_double(CGAL::scalar_product(v2, n_prev_reflected)) *
                   v2;
    }
    next_n = next_n / std::sqrt(CGAL::to_double(next_n.squared_length()));
    EK::Vector_3 next_b = CGAL::cross_product(next_t, next_n);
    frames.push_back({curr_p, next_t, next_n, next_b});
  }

  if (closed && frames.size() > 1) {
    EK::Point_3 prev_p = path.back();
    EK::Point_3 curr_p = path[0];
    EK::Vector_3 prev_t = frames.back().tangent;
    EK::Vector_3 prev_n = frames.back().normal;
    EK::Vector_3 v1 = curr_p - prev_p;
    double c1 = CGAL::to_double(v1.squared_length());
    if (c1 > 0) {
      EK::Vector_3 n_reflected =
          prev_n -
          (2.0 / c1) * CGAL::to_double(CGAL::scalar_product(v1, prev_n)) * v1;
      EK::Vector_3 t_reflected =
          prev_t -
          (2.0 / c1) * CGAL::to_double(CGAL::scalar_product(v1, prev_t)) * v1;
      EK::Vector_3 target_t = frames[0].tangent;
      EK::Vector_3 v2 = target_t - t_reflected;
      double c2 = CGAL::to_double(v2.squared_length());
      EK::Vector_3 final_n =
          (c2 == 0)
              ? n_reflected
              : n_reflected -
                    (2.0 / c2) *
                        CGAL::to_double(CGAL::scalar_product(v2, n_reflected)) *
                        v2;
      EK::Vector_3 target_n = frames[0].normal;
      double dot = CGAL::to_double(CGAL::scalar_product(final_n, target_n));
      if (dot > 1.0) dot = 1.0;
      if (dot < -1.0) dot = -1.0;
      double angle = std::acos(dot);
      EK::Vector_3 cross = CGAL::cross_product(final_n, target_n);
      if (CGAL::to_double(CGAL::scalar_product(cross, target_t)) < 0)
        angle = -angle;
      for (size_t i = 0; i < frames.size(); ++i)
        frames[i].twist(((double)i / (double)frames.size()) * angle);
    }
  }
  return frames;
}

static EK::Point_3 pivot_rotate(const EK::Point_3& p_world,
                                const EK::Point_3& pivot,
                                const EK::Vector_3& axis, double angle) {
  if (angle == 0 || axis.squared_length() == 0) return p_world;
  double ax = CGAL::to_double(axis.x()), ay = CGAL::to_double(axis.y()),
         az = CGAL::to_double(axis.z());
  double len = std::sqrt(ax * ax + ay * ay + az * az);
  ax /= len;
  ay /= len;
  az /= len;
  double c = std::cos(angle), s = std::sin(angle), t = 1.0 - c;
  double m00 = c + ax * ax * t, m01 = ax * ay * t - az * s,
         m02 = ax * az * t + ay * s;
  double m10 = ay * ax * t + az * s;
  double m11 = c + ay * ay * t;
  double m12 = ay * az * t - ax * s;
  double m20 = az * ax * t - ay * s;
  double m21 = az * ay * t + ax * s;
  double m22 = c + az * az * t;
  CK::Aff_transformation_3 rotate(m00, m01, m02, 0, m10, m11, m12, 0, m20, m21,
                                  m22, 0);
  EK::Vector_3 v = p_world - pivot;
  CK::Vector_3 v_ck(CGAL::to_double(v.x()), CGAL::to_double(v.y()),
                    CGAL::to_double(v.z()));
  return pivot + EK::Vector_3(rotate.transform(v_ck).x(),
                              rotate.transform(v_ck).y(),
                              rotate.transform(v_ck).z());
}

static EK::Point_3 miter_project(const EK::Point_3& p_world,
                                 const EK::Point_3& pivot,
                                 const EK::Vector_3& t_in,
                                 const EK::Vector_3& t_out, double iota) {
  EK::Vector_3 n = t_in + t_out;
  double n_len_sq = CGAL::to_double(n.squared_length());
  if (n_len_sq < 1e-9) return p_world;
  EK::Point_3 shifted_pivot = pivot + iota * (n / std::sqrt(n_len_sq));
  EK::Plane_3 miter_plane(shifted_pivot, n);
  auto result = CGAL::intersection(miter_plane, EK::Line_3(p_world, t_out));
  if (result) {
    if (const EK::Point_3* pt = std::get_if<EK::Point_3>(&*result)) return *pt;
  }
  return p_world;
}

}  // namespace geometry