#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/extrude.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/transform.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Surface_mesh.h>

template <typename MAP>
struct Project {
  Project(MAP map, const Tf& tf) : map(map), tf_(tf) {}

  template <typename VD, typename T>
  void operator()(const T&, VD vd) const {
    put(map, vd, get(map, vd).transform(tf_));
  }

  MAP map;
  const Tf tf_;
};

template <typename K>
static bool ExtrudeSurfaceMesh(
    CGAL::Surface_mesh<typename K::Point_3>& mesh, const Tf& top,
    const Tf& bottom, CGAL::Surface_mesh<typename K::Point_3>& extruded_mesh) {
  typedef typename boost::property_map<CGAL::Surface_mesh<typename K::Point_3>,
                                       CGAL::vertex_point_t>::type VPMap;
  if (!CGAL::is_closed(mesh) && !CGAL::is_empty(mesh)) {
    // No protection against self-intersection.
    Project<VPMap> top_projection(get(CGAL::vertex_point, extruded_mesh), top);
    Project<VPMap> bottom_projection(get(CGAL::vertex_point, extruded_mesh),
                                     bottom);
    CGAL::Polygon_mesh_processing::extrude_mesh(
        mesh, extruded_mesh, bottom_projection, top_projection);
    CGAL::Polygon_mesh_processing::triangulate_faces(extruded_mesh);
    EK::FT volume = CGAL::Polygon_mesh_processing::volume(
        extruded_mesh, CGAL::parameters::all_default());
    if (volume < 0) {
      CGAL::Polygon_mesh_processing::reverse_face_orientations(extruded_mesh);
    }
    if (volume != 0) {
      return true;
    }
  }
  return false;
}
