#pragma once

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/remesh.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Bounded_normal_change_placement.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Count_ratio_stop_predicate.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Midpoint_placement.h>
#include <CGAL/Surface_mesh_simplification/edge_collapse.h>

#include <cmath>
#include <optional>
#include <vector>

#include "repair_util.h"

namespace geometry {

typedef CGAL::Exact_predicates_inexact_constructions_kernel IK;

template <class GetPlacement>
class Angle_constrained_placement {
 public:
  Angle_constrained_placement(
      double max_angle_deg, const GetPlacement& get_placement = GetPlacement())
      : m_get_placement(get_placement) {
    double max_angle_rad = max_angle_deg * M_PI / 180.0;
    m_min_cos = std::cos(max_angle_rad);
  }

  template <typename Profile>
  std::optional<typename Profile::Point> operator()(
      const Profile& profile) const {
    typedef typename Profile::VertexPointMap Vertex_point_map;
    typedef typename Profile::Geom_traits Geom_traits;
    typedef typename Geom_traits::Vector_3 Vector;
    typedef typename boost::property_traits<Vertex_point_map>::value_type Point;
    typedef typename boost::property_traits<Vertex_point_map>::reference
        Point_reference;

    const Geom_traits& gt = profile.geom_traits();
    const Vertex_point_map& vpm = profile.vertex_point_map();

    std::optional<typename Profile::Point> op = m_get_placement(profile);
    if (op) {
      const typename Profile::Triangle_vector& triangles = profile.triangles();
      if (triangles.size() > 2) {
        typename Profile::Triangle_vector::const_iterator it =
            triangles.begin();

        if (profile.left_face_exists()) ++it;
        if (profile.right_face_exists()) ++it;

        while (it != triangles.end()) {
          const typename Profile::Triangle& t = *it;
          Point_reference p = get(vpm, t.v0);
          Point_reference q = get(vpm, t.v1);
          Point_reference r = get(vpm, t.v2);
          const Point& q2 = *op;

          Vector eqp = gt.construct_vector_3_object()(q, p);
          Vector eqr = gt.construct_vector_3_object()(q, r);
          Vector eq2p = gt.construct_vector_3_object()(q2, p);
          Vector eq2r = gt.construct_vector_3_object()(q2, r);

          Vector n1 = gt.construct_cross_product_vector_3_object()(eqp, eqr);
          Vector n2 = gt.construct_cross_product_vector_3_object()(eq2p, eq2r);

          // Normalize to check angle
          double len1_sq = gt.compute_scalar_product_3_object()(n1, n1);
          double len2_sq = gt.compute_scalar_product_3_object()(n2, n2);

          if (len1_sq > 0 && len2_sq > 0) {
            double dot = gt.compute_scalar_product_3_object()(n1, n2);
            // If dot is negative, angle > 90.
            if (dot < 0) return std::nullopt;

            // Check angle: dot / (|n1|*|n2|) < min_cos  => fail
            // dot^2 / (len1_sq * len2_sq) < min_cos^2 => fail (since dot>0)
            if (dot * dot < m_min_cos * m_min_cos * len1_sq * len2_sq) {
              return std::nullopt;
            }
          } else {
            if (len2_sq == 0) return std::nullopt;
          }

          ++it;
        }
      }
    }

    return op;
  }

 private:
  const GetPlacement m_get_placement;
  double m_min_cos;
};

template <typename K>
static void isotropic_remesh(CGAL::Surface_mesh<typename K::Point_3>& mesh,
                             double target_edge_length, int iterations = 3) {
  if (mesh.is_empty() || target_edge_length <= 0) return;
  CGAL::Polygon_mesh_processing::isotropic_remeshing(
      faces(mesh), target_edge_length, mesh,
      CGAL::parameters::number_of_iterations(iterations)
          .protect_constraints(false));
  repair_degeneracies<K>(mesh);
}

template <typename K, typename EdgeIsConstrainedMap>
static int edge_collapse(CGAL::Surface_mesh<typename K::Point_3>& mesh,
                         double stop_ratio, EdgeIsConstrainedMap ecm,
                         bool use_angle_constrained, double max_angle_deg) {
  if (mesh.is_empty()) return 0;
  namespace SMS = CGAL::Surface_mesh_simplification;
  SMS::Count_ratio_stop_predicate<CGAL::Surface_mesh<typename K::Point_3>> stop(
      stop_ratio);

  int removed = 0;
  if (use_angle_constrained) {
    removed = SMS::edge_collapse(
        mesh, stop,
        CGAL::parameters::edge_is_constrained_map(ecm).get_placement(
            Angle_constrained_placement<SMS::Midpoint_placement<
                CGAL::Surface_mesh<typename K::Point_3>>>(max_angle_deg)));
  } else {
    removed = SMS::edge_collapse(
        mesh, stop,
        CGAL::parameters::edge_is_constrained_map(ecm).get_placement(
            SMS::Bounded_normal_change_placement<SMS::Midpoint_placement<
                CGAL::Surface_mesh<typename K::Point_3>>>()));
  }
  repair_degeneracies<K>(mesh);
  return removed;
}

}  // namespace geometry
