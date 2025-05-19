#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Surface_mesh_simplification/Edge_collapse_visitor_base.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Bounded_normal_change_placement.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Constrained_placement.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Face_count_stop_predicate.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/GarlandHeckbert_policies.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Midpoint_placement.h>
#include <CGAL/Surface_mesh_simplification/edge_collapse.h>

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;

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

template <typename K>
static void simplify(double face_count, double sharp_edge_threshold,
                     CGAL::Surface_mesh<typename K::Point_3>& mesh,
                     std::vector<typename K::Segment_3>& sharp_edges) {
  std::cout << "QQ/simplify/1" << std::endl;
  double sharp_edge_threshold_degrees = sharp_edge_threshold * 360;
  typedef typename boost::graph_traits<
      CGAL::Surface_mesh<typename K::Point_3>>::edge_descriptor edge_descriptor;
  typedef
      typename boost::graph_traits<CGAL::Surface_mesh<typename K::Point_3>>::
          halfedge_descriptor halfedge_descriptor;
  namespace SMS = CGAL::Surface_mesh_simplification;
  typedef SMS::GarlandHeckbert_probabilistic_triangle_policies<
      CGAL::Surface_mesh<typename K::Point_3>, K>
      Prob_tri;

  CGAL::Unique_hash_map<edge_descriptor, bool> constraint_hmap(false);
  Constrained_edge_map<K> constraints_map(constraint_hmap);

  std::cout << "QQ/simplify/2" << std::endl;
  if (false) {
    SMS::Constrained_placement<
        SMS::Midpoint_placement<CGAL::Surface_mesh<typename K::Point_3>>,
        Constrained_edge_map<K>>
        placement(constraints_map);
    // map used to check that constrained_edges and the points of its vertices
    // are preserved at the end of the simplification
    // Warning: the computation of the dihedral angle is only an approximation
    // and can
    //          be far from the real value and could influence the detection of
    //          sharp edges after the simplification
    std::map<edge_descriptor,
             std::pair<typename K::Point_3, typename K::Point_3>>
        constrained_edges;
    std::size_t nb_sharp_edges = 0;
    // detect sharp edges
    std::cout << "QQ/simplify/3" << std::endl;
    for (edge_descriptor ed : edges(mesh)) {
      std::cout << "QQ/simplify/3/e" << std::endl;
      halfedge_descriptor hd = halfedge(ed, mesh);
      if (mesh.is_border(ed)) {
        ++nb_sharp_edges;
        constraint_hmap[ed] = true;
        typename K::Point_3 p = mesh.point(source(hd, mesh));
        typename K::Point_3 q = mesh.point(target(hd, mesh));
        constrained_edges[ed] = std::make_pair(p, q);
        sharp_edges.emplace_back(p, q);
      } else {
        double angle = CGAL::approximate_dihedral_angle(
            mesh.point(target(opposite(hd, mesh), mesh)),
            mesh.point(target(hd, mesh)),
            mesh.point(target(next(hd, mesh), mesh)),
            mesh.point(target(next(opposite(hd, mesh), mesh), mesh)));
        if (CGAL::abs(angle) < sharp_edge_threshold_degrees) {
          ++nb_sharp_edges;
          constraint_hmap[ed] = true;
          typename K::Point_3 p = mesh.point(source(hd, mesh));
          typename K::Point_3 q = mesh.point(target(hd, mesh));
          constrained_edges[ed] = std::make_pair(p, q);
          sharp_edges.emplace_back(p, q);
          std::cout << "QQ/simplify/3/e se=" << sharp_edges.size() << std::endl;
        }
      }
    }
    std::cout << "nb_sharp_edges=" << nb_sharp_edges << std::endl;
  }
  std::cout << "QQ/simplify/4: face_count=" << face_count << std::endl;

  SMS::Face_count_stop_predicate<CGAL::Surface_mesh<typename K::Point_3>> stop(
      face_count);

  // Garland&Heckbert simplification policies
  typedef Prob_tri GHPolicies;
  typedef typename GHPolicies::Get_cost GH_cost;
  typedef typename GHPolicies::Get_placement GH_placement;
  typedef SMS::Bounded_normal_change_placement<GH_placement>
      Bounded_GH_placement;
  GHPolicies gh_policies(mesh);
  const GH_cost& gh_cost = gh_policies.get_cost();
  const GH_placement& gh_placement = gh_policies.get_placement();
  Bounded_GH_placement placement(gh_placement);
  typedef SMS::Constrained_placement<Bounded_GH_placement,
                                     Constrained_edge_map<K>>
      Constrained_placement;
  Constrained_placement edge_constrained_placement(constraints_map, placement);

  std::cout << "QQ/simplify/5" << std::endl;
#if 0
  class Edge_collapse_stats : SMS::Edge_collapse_visitor_base<
                                  CGAL::Surface_mesh<typename K::Point_3>> {
   public:
    using SMS::Edge_collapse_visitor_base<
        CGAL::Surface_mesh<typename K::Point_3>>::OnFinished;
    using SMS::Edge_collapse_visitor_base<
        CGAL::Surface_mesh<typename K::Point_3>>::OnStarted;
    using SMS::Edge_collapse_visitor_base<
        CGAL::Surface_mesh<typename K::Point_3>>::OnStopConditionReached;

    Edge_collapse_stats() : edge_count_(0), collapse_count_(0) {}

    // Called during the collecting phase for each edge collected.
    void OnCollected(const Profile&, std::optional<double>&) { edge_count_++; }

    // Called during the processing phase for each edge selected.
    // If cost is absent the edge won't be collapsed.
    void OnSelected(const Profile&, std::optional<double> cost,
                    std::size_t initial, std::size_t current) {}

    // Called during the processing phase for each edge being collapsed.
    // If placement is absent the edge is left uncollapsed.
    void OnCollapsing(const Profile&, std::optional<Point> placement) {}

    // Called for each edge which failed the so called link-condition,
    // that is, which cannot be collapsed because doing so would
    // turn the surface mesh into a non-manifold.
    void OnNonCollapsable(const Profile&) {}

    // Called after each edge has been collapsed
    void OnCollapsed(const Profile&, vertex_descriptor) {
      if (collapse_count_++ % 1000 == 0) {
        std::cout << "Collapse edges: " << collapse_count_ << " of "
                  << edge_count_ << " remaining "
                  << (edge_count_ - collapse_count_) << std::endl;
      }
    }

   private:
    size_t edge_count_;
    size_t collapse_count_;
  };

  Edge_collapse_stats edge_collapse_stats;
#endif
  std::cout << "QQ/simplify/6" << std::endl;

  std::cout << "QQ/simplify/7: edge collapse begin" << std::endl;
  int r = SMS::edge_collapse(
      mesh, stop,
      CGAL::parameters::get_cost(gh_cost)
          .get_placement(edge_constrained_placement)
          .vertex_index_map(CGAL::get_initialized_vertex_index_map(mesh))
          .halfedge_index_map(CGAL::get_initialized_halfedge_index_map(mesh))
      // .edge_is_constrained_map(constraints_map)
      // .visitor(edge_collapse_stats)
  );

  std::cout << "QQ/simplify/8: removed edge count=" << r << std::endl;
}

template <typename K>
void simplify(double face_count, double sharp_edge_threshold,
              CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  std::vector<typename K::Segment_3> discarded_sharp_edges;
  simplify<K>(face_count, sharp_edge_threshold, mesh, discarded_sharp_edges);
}
