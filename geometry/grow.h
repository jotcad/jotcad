#pragma once

#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/transform.h>
#include <CGAL/Polygon_triangulation_decomposition_2.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/boost/graph/generators.h>
#include <CGAL/connect_holes.h>
#include <CGAL/convex_hull_3.h>
#include <CGAL/mark_domain_in_triangulation.h>
#include <CGAL/partition_2.h>
#include <CGAL/simplest_rational_in_interval.h>
#include <CGAL/Nef_polyhedron_3.h>
#include <CGAL/convex_decomposition_3.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Polygon_2.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef EK::FT FT;

template <typename K>
static void to_points(const CGAL::Surface_mesh<typename K::Point_3>& mesh,
                      std::vector<typename K::Point_3>& points) {
  for (const auto vertex : mesh.vertices()) {
    points.push_back(mesh.point(vertex));
  }
}

template <typename K>
static void to_points(const CGAL::Polygon_with_holes_2<K> polygon,
                      const typename K::Plane_3& plane,
                      std::vector<typename K::Point_3>& points) {
  for (const auto& point : polygon.outer_boundary()) {
    points.push_back(plane.to_3d(point));
  }
  for (auto hole = polygon.holes_begin(); hole != polygon.holes_end(); ++hole) {
    for (const auto& point : *hole) {
      points.push_back(plane.to_3d(point));
    }
  }
}

template <typename K>
static void to_points(const typename K::Segment_3& segment,
                      std::vector<typename K::Point_3>& points) {
  points.push_back(segment.source());
  points.push_back(segment.target());
}

template <typename K>
static void to_points(const typename K::Point_3& point,
                      std::vector<typename K::Point_3>& points) {
  points.push_back(point);
}

template <class K>
static CGAL::Aff_transformation_3<K> translate_to(const typename K::Point_3& p) {
  return CGAL::Aff_transformation_3<K>(CGAL::TRANSLATION, p - CGAL::ORIGIN);
}

static GeometryId Grow(Assets& assets, Shape& shape,
                      std::vector<Shape>& tool_shapes) {
  try {
    typedef CGAL::Polygon_2<EK> Polygon_2;
    typedef std::list<Polygon_2> Polygon_list;

    std::vector<Polygon_2> hole_polygons;

    std::vector<EK::Point_3> tool_points;
    for (Shape& tool_shape : tool_shapes) {
      Geometry tool_geometry =
          assets.GetGeometry(tool_shape.GeometryId())
              .Transform(tool_shape.GetTf());
      for (const auto& vertex : tool_geometry.vertices_) {
        tool_points.push_back(vertex);
      }
    }

    Geometry geometry =
        assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
    CGAL::Surface_mesh<EK::Point_3> final_mesh;

    auto add_hull = [&](const std::vector<EK::Point_3>& points) {
      CGAL::Surface_mesh<EK::Point_3> hull;
      CGAL::convex_hull_3(points.begin(), points.end(), hull);
      if (final_mesh.is_empty()) {
        final_mesh = hull;
      } else {
        CGAL::Polygon_mesh_processing::corefine_and_compute_union(
            final_mesh, hull, final_mesh,
            CGAL::parameters::throw_on_self_intersection(true));
      }
    };

    if (!geometry.triangles_.empty()) {
      CGAL::Surface_mesh<EK::Point_3> mesh;
      geometry.EncodeSurfaceMesh<EK>(mesh);
      typedef CGAL::Nef_polyhedron_3<EK> Nef;
      Nef nef(mesh);

      try {
        CGAL::convex_decomposition_3(nef);
      } catch (const std::exception& e) {
        Napi::Error::New(assets.Env(), "Grow: Failed to decompose mesh")
            .ThrowAsJavaScriptException();
      }

      auto ci = nef.volumes_begin();
      if (ci == nef.volumes_end()) {
        // The mesh was empty.
      } else {
        EK::Point_3 zero(0, 0, 0);

        if (++ci == nef.volumes_end()) {
          // The mesh is already completely convex.
          std::vector<EK::Point_3> points;
          for (const auto& vertex : mesh.vertices()) {
            const auto& offset = mesh.point(vertex) - zero;
            for (const auto& point : tool_points) {
              points.push_back(point + offset);
            }
          }
          add_hull(points);
        } else {
          // Split out the convex parts.
          for (; ci != nef.volumes_end(); ++ci) {
            CGAL::Polyhedron_3<EK> shell;
            nef.convert_inner_shell_to_polyhedron(ci->shells_begin(), shell);
            std::vector<EK::Point_3> points;
            for (const auto& point : shell.points()) {
              const auto& offset = point - zero;
              for (const auto& point : tool_points) {
                points.push_back(point + offset);
              }
            }
            add_hull(points);
          }
        }
      }
    } else if (!geometry.points_.empty()) {
      CGAL::Surface_mesh<EK::Point_3> tool;
      CGAL::convex_hull_3(tool_points.begin(), tool_points.end(), tool);
      for (const auto& point_index : geometry.points_) {
        CGAL::Surface_mesh<EK::Point_3> a_tool = tool;
        CGAL::Polygon_mesh_processing::transform(
            translate_to<EK>(geometry.vertices_[point_index]), a_tool);
        std::vector<EK::Point_3> points;
        to_points<EK>(a_tool, points);
        add_hull(points);
      }
    } else if (!geometry.segments_.empty()) {
      EK::Point_3 zero(0, 0, 0);
      for (const auto& segment : geometry.segments_) {
        const EK::Vector_3 source_offset =
            geometry.vertices_[segment.first] - zero;
        const EK::Vector_3 target_offset =
            geometry.vertices_[segment.second] - zero;
        std::vector<EK::Point_3> points;
        for (const auto& point : tool_points) {
          points.push_back(point + source_offset);
          points.push_back(point + target_offset);
        }
        add_hull(points);
      }
    } else if (!geometry.faces_.empty()) {
      EK::Point_3 zero(0, 0, 0);
      const auto plane = EK::Plane_3(0, 0, 1, 0);
      for (const auto& face_data : geometry.faces_) {
        const auto& face_points = face_data.first;
        const auto& holes = face_data.second;
        Polygon_2 outer;
        for (const auto& point_index : face_points) {
          outer.push_back(plane.to_2d(geometry.vertices_[point_index]));
        }
        std::vector<Polygon_2> hole_polygons;
        for (const auto& hole : holes) {
          Polygon_2 hole_polygon;
          for (const auto& point_index : hole) {
            hole_polygon.push_back(
                plane.to_2d(geometry.vertices_[point_index]));
          }
          hole_polygons.push_back(hole_polygon);
        }
        CGAL::Polygon_with_holes_2<EK> pwh(outer, hole_polygons.begin(),
                                           hole_polygons.end());

        if (pwh.holes_begin() == pwh.holes_end()) {
              // We can use optimal partitioning if there are no holes.
              typedef CGAL::Partition_traits_2<EK> Traits;
	      typedef Traits::Polygon_2 Polygon_2;
              typedef std::list<Polygon_2> Polygon_list;
              Polygon_list partition_polys;
              // We invest more here to minimize unions below.
              CGAL::optimal_convex_partition_2(
                  pwh.outer_boundary().vertices_begin(),
                  pwh.outer_boundary().vertices_end(),
                  std::back_inserter(partition_polys));
              for (const auto& polygon : partition_polys) {
                std::vector<EK::Point_3> points;
                for (const auto& point_2 : polygon) {
                  const auto point = plane.to_3d(point_2);
                  const auto offset = point - zero;
                  for (const auto& tool_point : tool_points) {
                    points.push_back(tool_point + offset);
                  }
                }
                add_hull(points);
              }
        } else {
          typedef CGAL::Exact_predicates_tag Itag;
          typedef CGAL::Constrained_Delaunay_triangulation_2<
              EK, CGAL::Default, Itag>
              CDT;
          CDT cdt;
          cdt.insert_constraint(pwh.outer_boundary().vertices_begin(),
                                pwh.outer_boundary().vertices_end(), true);
          for (auto hole = pwh.holes_begin(); hole != pwh.holes_end(); ++hole) {
            cdt.insert_constraint(hole->vertices_begin(), hole->vertices_end(),
                                  true);
          }
          std::unordered_map<CDT::Face_handle, bool> in_domain_map;
          boost::associative_property_map<
              std::unordered_map<CDT::Face_handle, bool>>
              in_domain(in_domain_map);
          CGAL::mark_domain_in_triangulation(cdt, in_domain);
          for (auto face : cdt.finite_face_handles()) {
            if (!get(in_domain, face)) {
              continue;
            }
            std::vector<EK::Point_3> points;
            auto add_point = [&](const EK::Point_2& p2) {
              auto offset = plane.to_3d(p2) - zero;
              for (const auto& tool_point : tool_points) {
                points.push_back(tool_point + offset);
              }
            };
            add_point(face->vertex(0)->point());
            add_point(face->vertex(1)->point());
            add_point(face->vertex(2)->point());
            add_hull(points);
          }
        }
      }
    }

    Geometry output;
    output.DecodeSurfaceMesh<EK>(final_mesh);
    return assets.TextId(output.Transform(shape.GetTf().inverse()));
  } catch (const std::exception& e) {
    Napi::Error::New(assets.Env(), e.what()).ThrowAsJavaScriptException();
  }
  return assets.UndefinedId();
}

/*
static int Grow(Geometry* geometry, size_t count) {
  try {
    typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
    size_t size = geometry->size();

    geometry->copyInputMeshesToOutputMeshes();
    geometry->copyInputPointsToOutputPoints();
    geometry->copyInputSegmentsToOutputSegments();
    geometry->transformToAbsoluteFrame();
    geometry->convertPlanarMeshesToPolygons();

    std::vector<EK::Point_3> tool_points;

    auto add_hull = [&](size_t nth, const std::vector<EK::Point_3>& points) {
      size_t target = geometry->add(GEOMETRY_MESH);
      CGAL::convex_hull_3(points.begin(), points.end(), geometry->mesh(target));
      geometry->origin(target) = nth;
    };

    // For now we assume the tool is convex.
    for (size_t nth = count; nth < size; nth++) {
      geometry->tags(nth).push_back("type:ghost");
      geometry->tags(nth).push_back("material:ghost");
      switch (geometry->type(nth)) {
        case GEOMETRY_MESH:
          to_points<EK>(geometry->mesh(nth), tool_points);
          break;
        case GEOMETRY_POLYGONS_WITH_HOLES: {
          const auto& plane = geometry->plane(nth);
          for (const auto& pwh : geometry->pwh(nth)) {
            to_points<EK>(pwh, plane, tool_points);
          }
          break;
        }
        case GEOMETRY_SEGMENTS:
          for (const auto& segment : geometry->segments(nth)) {
            to_points<EK>(segment, tool_points);
          }
          break;
        case GEOMETRY_POINTS:
          for (const auto& point : geometry->points(nth)) {
            to_points<EK>(point, tool_points);
          }
          break;
      }
    }

    for (size_t nth = 0; nth < count; nth++) {
      switch (geometry->getType(nth)) {
        case GEOMETRY_MESH: {
          typedef CGAL::Nef_polyhedron_3<Epeck_kernel> Nef;
          auto& mesh = geometry->mesh(nth);

          Nef nef(mesh);

          try {
            CGAL::convex_decomposition_3(nef);
          } catch (const std::exception& e) {
            throw;
          }

          auto ci = nef.volumes_begin();
          if (ci == nef.volumes_end()) {
            // The mesh was empty.
            break;
          }

          EK::Point_3 zero(0, 0, 0);

          if (++ci == nef.volumes_end()) {
            // The mesh is already completely convex.
            std::vector<EK::Point_3> points;
            for (const auto& vertex : mesh.vertices()) {
              const auto& offset = mesh.point(vertex) - zero;
              for (const auto& point : tool_points) {
                points.push_back(point + offset);
              }
            }
            add_hull(nth, points);
          } else {
            mesh.clear();
            // Split out the convex parts.
            for (; ci != nef.volumes_end(); ++ci) {
              CGAL::Polyhedron_3<Epeck_kernel> shell;
              nef.convert_inner_shell_to_polyhedron(ci->shells_begin(), shell);
              std::vector<EK::Point_3> points;
              for (const auto& point : shell.points()) {
                const auto& offset = point - zero;
                for (const auto& point : tool_points) {
                  points.push_back(point + offset);
                }
              }
              add_hull(nth, points);
            }
          }
          break;
        }
        case GEOMETRY_POINTS: {
          CGAL::Surface_mesh<EK::Point_3> tool;
          CGAL::convex_hull_3(tool_points.begin(), tool_points.end(), tool);
          for (const auto& point : geometry->points(nth)) {
            size_t target = geometry->add(GEOMETRY_MESH);
            geometry->origin(target) = nth;
            geometry->mesh(target) = tool;
            CGAL::Polygon_mesh_processing::transform(translate_to(point),
                                                     geometry->mesh(target));
          }
          break;
        }
        case GEOMETRY_SEGMENTS: {
          EK::Point_3 zero(0, 0, 0);
          for (const auto& segment : geometry->segments(nth)) {
            const EK::Vector_3 source_offset = segment.source() - zero;
            const EK::Vector_3 target_offset = segment.target() - zero;
            std::vector<EK::Point_3> points;
            for (const auto& point : tool_points) {
              points.push_back(point + source_offset);
              points.push_back(point + target_offset);
            }
            add_hull(nth, points);
          }
          break;
        }
        case GEOMETRY_POLYGONS_WITH_HOLES: {
          EK::Point_3 zero(0, 0, 0);
          CGAL::Polygon_triangulation_decomposition_2<EK> triangulator;
          // We need to copy plane to survive vector reallocation due to
          // geometry->add().
          const auto plane = geometry->plane(nth);
          for (const auto& pwh : geometry->pwh(nth)) {
            if (pwh.holes_begin() == pwh.holes_end()) {
              // We can use optimal partitioning if there are no holes.
              typedef CGAL::Partition_traits_2<EK> Traits;
              typedef CGAL::Polygon_2<EK> Polygon_2;
              typedef std::list<Polygon_2> Polygon_list;
              Polygon_list partition_polys;
              // We invest more here to minimize unions below.
              CGAL::optimal_convex_partition_2(
                  pwh.outer_boundary().vertices_begin(),
                  pwh.outer_boundary().vertices_end(),
                  std::back_inserter(partition_polys));
              for (const auto& polygon : partition_polys) {
                std::vector<EK::Point_3> points;
                for (const auto& point_2 : polygon) {
                  const auto point = plane.to_3d(point_2);
                  const auto offset = point - zero;
                  for (const auto& tool_point : tool_points) {
                    points.push_back(tool_point + offset);
                  }
                }
                add_hull(nth, points);
              }
            } else {
              typedef CGAL::Exact_predicates_tag Itag;
              typedef CGAL::Constrained_Delaunay_triangulation_2<
                  EK, CGAL::Default, Itag>
                  CDT;
              CDT cdt;
              cdt.insert_constraint(pwh.outer_boundary().vertices_begin(),
                                    pwh.outer_boundary().vertices_end(), true);
              for (auto hole = pwh.holes_begin(); hole != pwh.holes_end();
                   ++hole) {
                cdt.insert_constraint(hole->vertices_begin(),
                                      hole->vertices_end(), true);
              }
              std::unordered_map<CDT::Face_handle, bool> in_domain_map;
              boost::associative_property_map<
                  std::unordered_map<CDT::Face_handle, bool>>
                  in_domain(in_domain_map);
              CGAL::mark_domain_in_triangulation(cdt, in_domain);
              for (auto face : cdt.finite_face_handles()) {
                if (!get(in_domain, face)) {
                  continue;
                }
                std::vector<EK::Point_3> points;
                auto add_point = [&](const EK::Point_2& p2) {
                  auto offset = plane.to_3d(p2) - zero;
                  for (const auto& tool_point : tool_points) {
                    points.push_back(tool_point + offset);
                  }
                };
                add_point(face->vertex(0)->point());
                add_point(face->vertex(1)->point());
                add_point(face->vertex(2)->point());
                add_hull(nth, points);
              }
            }
          }
          break;
        }
      }
    }

    geometry->removeEmptyMeshes();
    geometry->convertPlanarMeshesToPolygons();
    geometry->transformToLocalFrame();

    return STATUS_OK;
  } catch (const std::exception& e) {
    std::cout << "Grow: " << e.what() << std::endl;
    throw;
  }
}
*/
