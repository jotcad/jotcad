#pragma once

// #include <CGAL/Exact_predicates_exact_constructions_kernel.h>
// #include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
// #include <CGAL/Surface_mesh_simplification/Edge_collapse_visitor_base.h>
// #include
// <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Bounded_normal_change_placement.h>
// #include
// <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Constrained_placement.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Face_count_stop_predicate.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/GarlandHeckbert_policies.h>
// #include
// <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Midpoint_placement.h>
#include <CGAL/Surface_mesh_simplification/edge_collapse.h>

#if 0
template <typename K>
struct Constrained_edge_map {
  typedef typename boost::graph_traits<
      CGAL::Surface_mesh<typename K::Point_3>>::edge_descriptor edge_descriptor;
  typedef boost::readable_property_map_tag category;
  typedef bool value_type;
  typedef bool reference;
  typedef edge_descriptor key_type;
  Constrained_edge_map(
      const CGAL::Unique_hash_map<key_type, bool>& aConstraints)
      : mConstraints(aConstraints) {}
  value_type operator[](const key_type& e) const { return is_constrained(e); }
  friend inline value_type get(const Constrained_edge_map& m,
                               const key_type& k) {
    return m[k];
  }
  bool is_constrained(const key_type& e) const {
    return mConstraints.is_defined(e);
  }

 private:
  const CGAL::Unique_hash_map<key_type, bool>& mConstraints;
};
#endif

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

  // CGAL::Unique_hash_map<edge_descriptor, bool> constraint_hmap(false);
  // Constrained_edge_map<K> constraints_map(constraint_hmap);

  SMS::Face_count_stop_predicate<CGAL::Surface_mesh<typename K::Point_3>> stop(
      face_count);

  // Garland&Heckbert simplification policies
  typedef Prob_tri GHPolicies;
  typedef typename GHPolicies::Get_cost GH_cost;
  typedef typename GHPolicies::Get_placement GH_placement;
  // typedef SMS::Bounded_normal_change_placement<GH_placement>
  // Bounded_GH_placement;
  GHPolicies gh_policies(mesh);
  const GH_cost& gh_cost = gh_policies.get_cost();
  const GH_placement& gh_placement = gh_policies.get_placement();
  // Bounded_GH_placement placement(gh_placement);
  // typedef SMS::Constrained_placement<Bounded_GH_placement,
  // Constrained_edge_map<K>> Constrained_placement; Constrained_placement
  // edge_constrained_placement(constraints_map, placement);

  int r = SMS::edge_collapse(
      mesh, stop,
      CGAL::parameters::get_cost(gh_cost)
          // .get_placement(edge_constrained_placement)
          .vertex_index_map(CGAL::get_initialized_vertex_index_map(mesh))
          .halfedge_index_map(CGAL::get_initialized_halfedge_index_map(mesh)));
}
