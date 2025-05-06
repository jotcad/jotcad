#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/extrude.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/transform.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Surface_mesh.h>

#include "assets.h"
#include "shape.h"
#include "geometry.h"

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

static void Extrude(Assets& assets, Shape& shape, Shape& top, Shape& bottom, Napi::Array results) {
  typedef typename boost::property_map<CGAL::Surface_mesh<EK::Point_3>,
                                       CGAL::vertex_point_t>::type VPMap;
  uint32_t nth = 0;

  auto extrude_surface_mesh = [&](CGAL::Surface_mesh<CGAL::Point_3<EK>>& mesh) {
    if (!CGAL::is_closed(mesh) && !CGAL::is_empty(mesh)) {
      CGAL::Polygon_mesh_processing::transform(shape.GetTf(), mesh, CGAL::parameters::all_default());
      // No protection against self-intersection.
      CGAL::Surface_mesh<CGAL::Point_3<EK>> extruded_mesh;
      Project<VPMap> top_projection(get(CGAL::vertex_point, extruded_mesh), top.GetTf());
      Project<VPMap> bottom_projection(get(CGAL::vertex_point, extruded_mesh), bottom.GetTf());
      CGAL::Polygon_mesh_processing::extrude_mesh(mesh, extruded_mesh, bottom_projection, top_projection);
      CGAL::Polygon_mesh_processing::triangulate_faces(extruded_mesh);
      EK::FT volume = CGAL::Polygon_mesh_processing::volume(extruded_mesh, CGAL::parameters::all_default());
      if (volume < 0) {
        CGAL::Polygon_mesh_processing::reverse_face_orientations(extruded_mesh);
      }
      if (volume != 0) {
        CGAL::Polygon_mesh_processing::transform(shape.GetTf().inverse(), extruded_mesh, CGAL::parameters::all_default());
        results.Set(nth++, assets.TextId(extruded_mesh));
      }
    }
  };

  {
    // Extrude open meshes.
    CGAL::Surface_mesh<CGAL::Point_3<EK>> mesh = assets.GetSurfaceMesh(shape.GeometryId());
    extrude_surface_mesh(mesh);
  }
  {
    // Extrude faces.
    CGAL::Surface_mesh<CGAL::Point_3<EK>> mesh = assets.GetFaceSurfaceMesh(shape.GeometryId());
    extrude_surface_mesh(mesh);
  }
}
