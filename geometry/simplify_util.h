#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Face_count_stop_predicate.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/GarlandHeckbert_policies.h>
#include <CGAL/Surface_mesh_simplification/edge_collapse.h>

template <typename K>
static void simplify(double face_count,
                     CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  typedef typename boost::graph_traits<
      CGAL::Surface_mesh<typename K::Point_3>>::edge_descriptor edge_descriptor;
  typedef
      typename boost::graph_traits<CGAL::Surface_mesh<typename K::Point_3>>::
          halfedge_descriptor halfedge_descriptor;
  namespace SMS = CGAL::Surface_mesh_simplification;
  typedef SMS::GarlandHeckbert_probabilistic_triangle_policies<
      CGAL::Surface_mesh<typename K::Point_3>, K>
      Prob_tri;
  SMS::Face_count_stop_predicate<CGAL::Surface_mesh<typename K::Point_3>> stop(
      face_count);
  typedef Prob_tri GHPolicies;
  typedef typename GHPolicies::Get_cost GH_cost;
  typedef typename GHPolicies::Get_placement GH_placement;
  GHPolicies gh_policies(mesh);
  const GH_cost& gh_cost = gh_policies.get_cost();
  const GH_placement& gh_placement = gh_policies.get_placement();
  SMS::edge_collapse(
      mesh, stop,
      CGAL::parameters::get_cost(gh_cost)
          .vertex_index_map(CGAL::get_initialized_vertex_index_map(mesh))
          .halfedge_index_map(CGAL::get_initialized_halfedge_index_map(mesh)));
}
