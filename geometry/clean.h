#pragma once

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Shape_detection/Region_growing/Polygon_mesh.h>
#include <CGAL/Shape_detection/Region_growing/Region_growing.h>
#include <CGAL/Shape_regularization/regularize_planes.h>
#include <CGAL/Surface_mesh.h>

#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "assets.h"
#include "geometry.h"
#include "remesh_util.h"
#include "repair_util.h"
#include "shape.h"
#include "validate_util.h"

namespace geometry {

typedef CGAL::Exact_predicates_inexact_constructions_kernel IK;

static GeometryId Clean(Assets& assets, Shape& shape, double angle_threshold,
                        bool use_angle_constrained = true,
                        bool enable_regularization = true,
                        bool enable_collapse = true,
                        double plane_distance_threshold = 0.1) {
  if (!shape.HasGeometryId()) return assets.UndefinedId();

  Geometry target_geom =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());

  CGAL::Surface_mesh<IK::Point_3> mesh;
  target_geom.EncodeSurfaceMesh<IK>(mesh);

  if (mesh.is_empty()) return assets.TextId(target_geom);

  // STRICT VALIDITY ASSERTIONS
  assert(validate_mesh(mesh, "Clean/Input"));

  std::cout << "Clean: Starting Shape Regularization (angle_threshold="
            << angle_threshold
            << " tau, plane_distance=" << plane_distance_threshold << ")..."
            << std::endl;

  // Shared parameters
  double max_angle_deg = angle_threshold * 360.0;
  assert(max_angle_deg >= 0.0 && max_angle_deg <= 90.0);

  // 1. Planar Region Detection (Pass 1 - for constraints)
  typedef CGAL::Shape_detection::Polygon_mesh::One_ring_neighbor_query<
      CGAL::Surface_mesh<IK::Point_3>>
      Neighbor_query;
  typedef CGAL::Shape_detection::Polygon_mesh::Least_squares_plane_fit_region<
      IK, CGAL::Surface_mesh<IK::Point_3>>
      Region_type;
  typedef CGAL::Shape_detection::Region_growing<Neighbor_query, Region_type>
      Region_growing;

  Neighbor_query neighbor_query(mesh);
  Region_type region_type(
      mesh, CGAL::parameters::maximum_distance(plane_distance_threshold)
                .maximum_angle(max_angle_deg)
                .minimum_region_size(3));

  Region_growing region_growing(faces(mesh), neighbor_query, region_type);

  std::vector<typename Region_growing::Primitive_and_region> regions;
  region_growing.detect(std::back_inserter(regions));

  std::cout << "Clean: Detected " << regions.size()
            << " planar regions (Pass 1)." << std::endl;

  if (regions.empty()) {
    std::cout << "Clean: No planar regions detected, returning original."
              << std::endl;
    return assets.TextId(target_geom);
  }

  // 2. Edge Collapse Simplification
  if (enable_collapse) {
    const auto& region_map = region_growing.region_map();
    std::cout << "Clean: Simplifying via edge collapse..." << std::endl;

    auto ecm =
        mesh.add_property_map<CGAL::Surface_mesh<IK::Point_3>::Edge_index,
                              bool>("e:is_constrained", false)
            .first;

    for (auto e : mesh.edges()) {
      auto h = mesh.halfedge(e);
      auto f1 = mesh.face(h);
      auto f2 = mesh.face(mesh.opposite(h));

      std::size_t r1 =
          (f1 == mesh.null_face()) ? std::size_t(-1) : get(region_map, f1);
      std::size_t r2 =
          (f2 == mesh.null_face()) ? std::size_t(-1) : get(region_map, f2);

      if (r1 != r2 || f1 == mesh.null_face() || f2 == mesh.null_face()) {
        ecm[e] = true;
      }
    }

    int removed =
        edge_collapse<IK>(mesh, 0.1, ecm, use_angle_constrained, max_angle_deg);

    std::cout << "Clean: Done. Collapsed " << removed
              << " edges. Remaining faces=" << mesh.number_of_faces()
              << std::endl;
  }

  // 3. Regularization
  if (enable_regularization) {
    Neighbor_query neighbor_query_2(mesh);
    Region_type region_type_2(
        mesh, CGAL::parameters::maximum_distance(plane_distance_threshold)
                  .maximum_angle(max_angle_deg)
                  .minimum_region_size(3));
    Region_growing region_growing_2(faces(mesh), neighbor_query_2,
                                    region_type_2);

    std::vector<typename Region_growing::Primitive_and_region> regions_2;
    region_growing_2.detect(std::back_inserter(regions_2));

    std::cout << "Clean: Detected " << regions_2.size()
              << " planar regions (Pass 2)." << std::endl;

    if (!regions_2.empty()) {
      const auto& region_map_2 = region_growing_2.region_map();

      // Prepare for Regularization
      std::vector<IK::Point_3> face_centers;
      std::vector<std::size_t> point_to_plane_map;
      std::vector<IK::Plane_3> planes;

      for (std::size_t i = 0; i < regions_2.size(); ++i) {
        planes.push_back(regions_2[i].first);
        for (auto f_idx : regions_2[i].second) {
          IK::Point_3 center(0, 0, 0);
          int v_count = 0;
          for (auto v : mesh.vertices_around_face(mesh.halfedge(f_idx))) {
            center = center + (mesh.point(v) - CGAL::ORIGIN);
            v_count++;
          }
          center = CGAL::ORIGIN + (center - CGAL::ORIGIN) / v_count;
          face_centers.push_back(center);
          point_to_plane_map.push_back(i);
        }
      }

      // Regularize Planes
      std::cout << "Clean: Regularizing " << planes.size() << " planes..."
                << std::endl;
      CGAL::Shape_regularization::Planes::regularize_planes(
          planes, face_centers,
          CGAL::parameters::plane_index_map(
              CGAL::make_property_map(point_to_plane_map))
              .maximum_angle(max_angle_deg));

      // Vertex Projection
      std::map<CGAL::Surface_mesh<IK::Point_3>::Vertex_index, IK::Point_3>
          new_positions;

      for (auto v : mesh.vertices()) {
        std::set<std::size_t> associated_regions;
        for (auto f : faces_around_target(mesh.halfedge(v), mesh)) {
          if (f == CGAL::Surface_mesh<IK::Point_3>::null_face()) continue;
          std::size_t region_id = get(region_map_2, f);
          if (region_id != std::size_t(-1)) {
            associated_regions.insert(region_id);
          }
        }

        if (associated_regions.empty()) continue;

        if (associated_regions.size() == 1) {
          new_positions[v] =
              planes[*associated_regions.begin()].projection(mesh.point(v));
        } else if (associated_regions.size() == 2) {
          auto it = associated_regions.begin();
          const auto& p1 = planes[*it++];
          const auto& p2 = planes[*it];
          auto result = CGAL::intersection(p1, p2);
          if (result) {
            if (const IK::Line_3* line = std::get_if<IK::Line_3>(&*result)) {
              new_positions[v] = line->projection(mesh.point(v));
            }
          }
        } else if (associated_regions.size() >= 3) {
          auto it = associated_regions.begin();
          const auto& p1 = planes[*it++];
          const auto& p2 = planes[*it++];
          const auto& p3 = planes[*it];
          auto result = CGAL::intersection(p1, p2, p3);
          if (result) {
            if (const IK::Point_3* pt = std::get_if<IK::Point_3>(&*result)) {
              new_positions[v] = *pt;
            }
          }
        }
      }

      for (const auto& [v, pos] : new_positions) {
        mesh.point(v) = pos;
      }

      repair_degeneracies<IK>(mesh);
      assert(validate_mesh(mesh, "Clean/AfterProjection"));
    }
  }

  assert(validate_mesh(mesh, "Clean/Output"));

  Geometry output;
  output.DecodeSurfaceMesh<IK>(mesh);
  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}

}  // namespace geometry
