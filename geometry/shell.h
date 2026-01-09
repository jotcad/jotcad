#pragma once

#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Labeled_mesh_domain_3.h>
#include <CGAL/Mesh_criteria_3.h>
#include <CGAL/Mesh_domain_with_polyline_features_3.h>
#include <CGAL/Mesh_triangulation_3.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polyhedral_mesh_domain_with_features_3.h>
#include <CGAL/Side_of_triangle_mesh.h>
#include <CGAL/facets_in_complex_3_to_triangle_mesh.h>
#include <CGAL/make_mesh_3.h>

#include "assets.h"
#include "geometry.h"
#include "random_util.h"
#include "shape.h"

namespace geometry {

namespace {
typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Exact_predicates_inexact_constructions_kernel IK;

template <typename K>
static typename K::Vector_3 NormalOfSurfaceMeshFacet(
    const CGAL::Surface_mesh<typename K::Point_3>& mesh,
    typename CGAL::Surface_mesh<typename K::Point_3>::Face_index facet) {
  const auto h = mesh.halfedge(facet);
  return CGAL::normal(mesh.point(mesh.source(h)),
                      mesh.point(mesh.source(mesh.next(h))),
                      mesh.point(mesh.source(mesh.next(mesh.next(h)))));
}

template <typename K>
static typename K::Vector_3 unitVector(const typename K::Vector_3& vector) {
  auto len2 = vector.squared_length();
  if (len2 == 0) return vector;
  return vector / std::sqrt(CGAL::to_double(len2));
}

typedef CGAL::Polyhedral_mesh_domain_with_features_3<
    IK, CGAL::Surface_mesh<IK::Point_3>, CGAL::Default, int>
    Polyhedral_mesh_domain;

typedef CGAL::Kernel_traits<Polyhedral_mesh_domain>::Kernel
    Robust_intersections_traits;

typedef CGAL::details::Mesh_geom_traits_generator<
    Robust_intersections_traits>::type Robust_K;

typedef CGAL::Compact_mesh_cell_base_3<Robust_K, Polyhedral_mesh_domain>
    Cell_base;
typedef CGAL::Triangulation_cell_base_with_info_3<int, Robust_K, Cell_base>
    Cell_base_with_info;

typedef CGAL::Mesh_triangulation_3<
    Polyhedral_mesh_domain, Robust_intersections_traits, CGAL::Sequential_tag,
    CGAL::Default, Cell_base_with_info>::type Tr;

typedef CGAL::Mesh_complex_3_in_triangulation_3<Tr> C3t3;

template <class TriangleMesh, class GeomTraits>
class Offset_function {
  typedef CGAL::AABB_face_graph_triangle_primitive<TriangleMesh> Primitive;
  typedef CGAL::AABB_traits<GeomTraits, Primitive> Traits;
  typedef CGAL::AABB_tree<Traits> Tree;
  typedef CGAL::Side_of_triangle_mesh<TriangleMesh, GeomTraits> Side_of;

 public:
  Offset_function(TriangleMesh& tm, double offset_min, double offset_max)
      : tree_ptr_(new Tree(boost::begin(faces(tm)), boost::end(faces(tm)), tm)),
        side_of_ptr_(new Side_of(*tree_ptr_)),
        offset_min_(offset_min),
        offset_max_(offset_max),
        is_closed_(is_closed(tm)) {
    CGAL_assertion(!tree_ptr_->empty());
  }

  double operator()(const typename GeomTraits::Point_3& p) const {
    typename GeomTraits::Point_3 closest_point = tree_ptr_->closest_point(p);
    double distance =
        std::sqrt(CGAL::to_double(squared_distance(p, closest_point)));

    CGAL::Bounded_side side =
        is_closed_ ? side_of_ptr_->operator()(p) : CGAL::ON_UNBOUNDED_SIDE;

    double sd = (side == CGAL::ON_BOUNDED_SIDE) ? -distance : distance;

    // Domain is f(p) <= 0.
    // We want sd in [offset_min, offset_max].
    return std::max(sd - offset_max_, offset_min_ - sd);
  }

 private:
  std::shared_ptr<Tree> tree_ptr_;
  std::shared_ptr<Side_of> side_of_ptr_;

  double offset_min_;
  double offset_max_;
  bool is_closed_;
};

}  // namespace

static GeometryId Shell(Assets& assets, Shape& shape, double inner_offset,
                        double outer_offset, bool protect, double angle,
                        double sizing, double approx, double edge_size) {
  if (!shape.HasGeometryId()) return assets.UndefinedId();

  Geometry target_geom =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<IK::Point_3> mesh;
  target_geom.EncodeSurfaceMesh<IK>(mesh);

  if (mesh.is_empty()) return assets.TextId(target_geom);

  typedef IK GT;
  typedef CGAL::Labeled_mesh_domain_3<GT, int, int> Mesh_domain_base;
  typedef CGAL::Mesh_domain_with_polyline_features_3<Mesh_domain_base>
      Mesh_domain;
  typedef C3t3::Triangulation Tr;
  typedef CGAL::Mesh_criteria_3<Tr> Mesh_criteria;
  typedef GT::Sphere_3 Sphere_3;

  CGAL::Bbox_3 bbox = CGAL::Polygon_mesh_processing::bbox(mesh);

  GT::Point_3 center((bbox.xmax() + bbox.xmin()) / 2,
                     (bbox.ymax() + bbox.ymin()) / 2,
                     (bbox.zmax() + bbox.zmin()) / 2);

  double d_min = std::min(inner_offset, outer_offset);
  double d_max = std::max(inner_offset, outer_offset);

  double rad2 = CGAL::square(
      0.6 * (std::sqrt(CGAL::square(bbox.xmax() - bbox.xmin() + d_max * 2) +
                       CGAL::square(bbox.ymax() - bbox.ymin() + d_max * 2) +
                       CGAL::square(bbox.zmax() - bbox.zmin() + d_max * 2))));

  namespace p = CGAL::parameters;

  Mesh_domain domain = Mesh_domain::create_implicit_mesh_domain(
      p::function = Offset_function<CGAL::Surface_mesh<IK::Point_3>, IK>(
          mesh, d_min, d_max),
      p::bounding_object = Sphere_3(center, rad2),
      p::relative_error_bound = 1e-7,
      p::construct_surface_patch_index = [](int i, int j) {
        return (i * 1000 + j);
      });

  const CGAL::Mesh_facet_topology topology =
      CGAL::FACET_VERTICES_ON_SAME_SURFACE_PATCH;

  Mesh_criteria criteria(p::facet_angle = angle, p::facet_size = sizing,
                         p::facet_distance = approx,
                         p::facet_topology = topology,
                         p::edge_size = edge_size);

  if (protect) {
    // Protect sharp edges (more than 30 degrees).
    std::vector<std::vector<IK::Point_3>> polylines;
    auto add_polyline = [&polylines](IK::Point_3 source, IK::Point_3 target) {
      for (const auto& entry : polylines) {
        if ((entry[0] == source && entry[1] == target) ||
            (entry[1] == source && entry[0] == target)) {
          // Polyline already present.
          return;
        }
      }
      std::vector<IK::Point_3> polyline{source, target};
      polylines.push_back(polyline);
    };
    const auto& m = mesh;
    const auto& pts = m.points();
    for (const auto edge : mesh.edges()) {
      const auto& e = CGAL::halfedge(edge, m);
      EK::FT angle_val = CGAL::approximate_dihedral_angle(
          pts[CGAL::target(opposite(e, m), m)], pts[CGAL::target(e, m)],
          pts[CGAL::target(CGAL::next(e, m), m)],
          pts[CGAL::target(CGAL::next(CGAL::opposite(e, m), m), m)]);
      if (abs(angle_val - 180) < 30) {
        continue;
      }
      {
        const auto& facet = mesh.face(e);
        auto normal = unitVector<IK>(NormalOfSurfaceMeshFacet<IK>(mesh, facet));
        add_polyline(mesh.point(mesh.source(e)) + normal * d_max,
                     mesh.point(mesh.target(e)) + normal * d_max);
        add_polyline(mesh.point(mesh.source(e)) + normal * d_min,
                     mesh.point(mesh.target(e)) + normal * d_min);
      }
      {
        const auto& facet = mesh.face(opposite(e, m));
        auto normal = unitVector<IK>(NormalOfSurfaceMeshFacet<IK>(mesh, facet));
        add_polyline(mesh.point(mesh.source(e)) + normal * d_max,
                     mesh.point(mesh.target(e)) + normal * d_max);
        add_polyline(mesh.point(mesh.source(e)) + normal * d_min,
                     mesh.point(mesh.target(e)) + normal * d_min);
      }
    }
    domain.add_features(polylines.begin(), polylines.end());
  }

  make_deterministic();

  C3t3 c3t3 = CGAL::make_mesh_3<C3t3>(domain, criteria);

  const Tr& tr = c3t3.triangulation();

  if (tr.number_of_vertices() > 0) {
    CGAL::Surface_mesh<IK::Point_3> epick_result;
    CGAL::facets_in_complex_3_to_triangle_mesh(c3t3, epick_result);

    std::cout << "Shell: result vertices=" << epick_result.number_of_vertices()
              << " faces=" << epick_result.number_of_faces() << std::endl;

    if (!CGAL::is_valid_polygon_mesh(epick_result)) {
      std::cout << "Shell: Result is NOT a valid polygon mesh!" << std::endl;
    }

    if (!CGAL::is_closed(epick_result)) {
      std::cout << "Shell: Result is NOT closed!" << std::endl;
    }

    auto fccm =
        epick_result
            .add_property_map<CGAL::Surface_mesh<IK::Point_3>::Face_index,
                              size_t>("f:component_index", 0)
            .first;
    size_t num_components =
        CGAL::Polygon_mesh_processing::connected_components(epick_result, fccm);
    std::cout << "Shell: connected components=" << num_components << std::endl;

    if (CGAL::is_closed(epick_result) &&
        !CGAL::Polygon_mesh_processing::is_outward_oriented(epick_result)) {
      CGAL::Polygon_mesh_processing::reverse_face_orientations(epick_result);
    }
    Geometry output;
    output.DecodeSurfaceMesh<IK>(epick_result);
    return assets.TextId(output.Transform(shape.GetTf().inverse()));
  }

  std::cout << "Shell: No domain found, returning empty geometry." << std::endl;
  return assets.UndefinedId();
}

}  // namespace geometry
