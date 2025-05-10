#pragma once

#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/intersections.h>

template <typename K>
static bool toPolygonsWithHolesFromBoundariesAndHoles(
    std::vector<CGAL::Polygon_2<K>>& boundaries,
    std::vector<CGAL::Polygon_2<K>>& holes,
    std::vector<CGAL::Polygon_with_holes_2<K>>& pwhs) {
  for (auto& boundary : boundaries) {
    if (boundary.size() == 0) {
      continue;
    }
    if (!boundary.is_simple()) {
      std::cout
          << "toPolygonsWithHolesFromBoundariesAndHoles: non_simple_boundary="
          << boundary << std::endl;
      return false;
    }
    if (boundary.orientation() != CGAL::Sign::POSITIVE) {
      boundary.reverse_orientation();
    }
    std::vector<CGAL::Polygon_2<K>> local_holes;
    for (auto& hole : holes) {
      if (hole.size() == 0) {
        continue;
      }
      if (!hole.is_simple()) {
        std::cout
            << "toPolygonsWithHolesFromBoundariesAndHoles: non_simple_hole="
            << hole << std::endl;
        return false;
      }
      const typename CGAL::Polygon_2<K>::Point_2& representative_point =
          hole[0];
      if (!boundary.has_on_negative_side(representative_point)) {
        // We permit holes to touch a boundary.
        if (hole.orientation() != CGAL::Sign::NEGATIVE) {
          hole.reverse_orientation();
        }
        // TODO: Consider destructively moving.
        local_holes.push_back(hole);
      }
    }
    // Remove holes within holes.
    if (local_holes.size() > 1) {
      std::vector<CGAL::Polygon_2<K>> distinct_holes;
      // FIX: Find a better way.
      // Turn all of the holes outside in.
      for (size_t a = 0; a < local_holes.size(); a++) {
        if (local_holes[a].orientation() != CGAL::Sign::POSITIVE) {
          local_holes[a].reverse_orientation();
        }
      }
      for (size_t a = 0; a < local_holes.size(); a++) {
        bool is_distinct = true;
        for (size_t b = 0; b < local_holes.size(); b++) {
          if (a == b) {
            continue;
          }
          if (CGAL::do_intersect(local_holes[a], local_holes[b]) &&
              local_holes[a].has_on_unbounded_side(local_holes[b][0])) {
            // The polygons overlap, and a is inside b, so skip a.
            is_distinct = false;
            break;
          }
        }
        if (is_distinct) {
          distinct_holes.push_back(local_holes[a]);
        }
      }
      // Then turn them back inside out.
      for (size_t a = 0; a < distinct_holes.size(); a++) {
        distinct_holes[a].reverse_orientation();
      }
      pwhs.emplace_back(boundary, distinct_holes.begin(), distinct_holes.end());
    } else {
      pwhs.emplace_back(boundary, local_holes.begin(), local_holes.end());
    }
  }
  for (auto& pwh : pwhs) {
    assert(pwh.outer_boundary().orientation() == CGAL::Sign::POSITIVE);
    for (const auto& hole : pwh.holes()) {
      assert(hole.orientation() == CGAL::Sign::NEGATIVE);
    }
  }
  return true;
}

template <typename Polygon_2>
static void simplifyPolygon(Polygon_2& polygon,
                            std::vector<Polygon_2>& simple_polygons) {
  if (polygon.size() < 3) {
    polygon.clear();
    return;
  }

  // Remove duplicate points.
  for (size_t nth = 0, limit = polygon.size(); nth < limit;) {
    const typename Polygon_2::Point_2& a = polygon[nth];
    const typename Polygon_2::Point_2& b = polygon[(nth + 1) % limit];

    if (a == b) {
      polygon.erase(polygon.begin() + nth);
      limit--;
    } else {
      nth++;
    }
  }

  if (polygon.size() < 3) {
    polygon.clear();
    return;
  }

  if (polygon.is_simple()) {
    simple_polygons.push_back(std::move(polygon));
    polygon.clear();
    return;
  }

  // Remove self intersections.
  std::set<typename Polygon_2::Point_2> seen_points;
  for (auto to = polygon.begin(); to != polygon.end();) {
    auto [found, created] = seen_points.insert(*to);
    if (created) {
      // Advance iterator to next position.
      ++to;
    } else {
      // This is a duplicate -- cut out the loop.
      auto from = std::find(polygon.begin(), to, *to);
      if (from == to) {
        std::cout << "QQ/Could not find seen point" << std::endl;
      }
      Polygon_2 cut(from, to);
      simplifyPolygon(cut, simple_polygons);
      if (cut.size() != 0) {
        std::cout << "QQ/cut was not cleared" << std::endl;
      }
      for (auto it = from; it != to; ++it) {
        seen_points.erase(*it);
      }
      // Erase advances the iterator to the new position.
      to = polygon.erase(from, to);
    }
  }

  if (polygon.size() < 3) {
    polygon.clear();
    return;
  }

  simplifyPolygon(polygon, simple_polygons);

  if (polygon.size() != 0) {
    std::cout << "QQ/polygon was not cleared" << std::endl;
  }

  for (const auto& simple_polygon : simple_polygons) {
    if (!simple_polygon.is_simple()) {
      std::cout
          << "QQ/simplifyPolygon produced non-simple polygon in simple_polygons"
          << std::endl;
      // print_polygon_nl(simple_polygon);
    }
  }
}

template <typename Arrangement_2>
static void analyzeCcb(typename Arrangement_2::Ccb_halfedge_circulator start,
                       size_t& region) {
  size_t base_region = region;
  typename Arrangement_2::Ccb_halfedge_circulator edge = start;
  std::map<typename Arrangement_2::Point_2,
           typename Arrangement_2::Ccb_halfedge_circulator>
      seen;
  do {
    auto [there, inserted] =
        seen.insert(std::make_pair(edge->source()->point(), edge));
    if (!inserted) {
      // This forms a loop: retrace it with the next region id.
      size_t subregion = ++region;
      auto trace = there->second;
      size_t superregion = trace->data();
      do {
        if (trace->data() == superregion) {
          trace->set_data(subregion);
        }
      } while (++trace != edge);
      // Update the entry to refer to point replacing the loop.
      there->second = edge;
    }
    edge->set_data(base_region);
  } while (++edge != start);
}

template <typename Arrangement_2>
static void analyzeArrangementRegions(Arrangement_2& arrangement) {
  // Region zero should cover the unbounded face.
  for (auto edge = arrangement.halfedges_begin();
       edge != arrangement.halfedges_end(); ++edge) {
    edge->set_data(0);
  }
  size_t region = 0;
  for (auto face = arrangement.faces_begin(); face != arrangement.faces_end();
       ++face) {
    if (face->number_of_outer_ccbs() == 1) {
      region++;
      analyzeCcb<Arrangement_2>(face->outer_ccb(), region);
    }
    for (auto hole = face->holes_begin(); hole != face->holes_end(); ++hole) {
      region++;
      analyzeCcb<Arrangement_2>(*hole, region);
    }
  }
}

static void removeRepeatedPointsInPolygon(CGAL::Polygon_2<EK>& polygon) {
  for (size_t nth = 0, limit = polygon.size(); nth < limit;) {
    const CGAL::Point_2<EK>& a = polygon[nth];
    const CGAL::Point_2<EK>& b = polygon[(nth + 1) % limit];
    if (a == b) {
      polygon.erase(polygon.begin() + nth);
      limit--;
    } else {
      nth++;
    }
  }
}

template <typename Polygon_2, typename Polygons_with_holes_2>
static void toSimplePolygonsWithHolesFromBoundariesAndHoles(
    std::vector<Polygon_2>& boundaries, std::vector<Polygon_2>& holes,
    Polygons_with_holes_2& pwhs) {
  std::vector<Polygon_2> simple_boundaries;
  for (auto& boundary : boundaries) {
    simplifyPolygon(boundary, simple_boundaries);
  }
  std::vector<Polygon_2> simple_holes;
  for (auto& hole : holes) {
    simplifyPolygon(hole, simple_holes);
  }
  toPolygonsWithHolesFromBoundariesAndHoles(simple_boundaries, simple_holes,
                                            pwhs);
}

// FIX: handle holes properly.
template <typename Arrangement_2>
static bool convertArrangementToPolygonsWithHolesNonZero(
    Arrangement_2& arrangement,
    std::vector<CGAL::Polygon_with_holes_2<EK>>& out,
    std::vector<CGAL::Segment_2<EK>>& non_simple) {
  analyzeArrangementRegions(arrangement);

  std::set<typename Arrangement_2::Face_handle> current;
  std::set<typename Arrangement_2::Face_handle> next;

  for (auto face = arrangement.faces_begin(); face != arrangement.faces_end();
       ++face) {
    if (face->is_unbounded() || face->number_of_outer_ccbs() != 1) {
      continue;
    }
    std::map<size_t, CGAL::Polygon_2<EK>> polygon_boundary_by_region;
    typename Arrangement_2::Ccb_halfedge_circulator start = face->outer_ccb();
    typename Arrangement_2::Ccb_halfedge_circulator edge = start;
    do {
      polygon_boundary_by_region[edge->data()].push_back(
          edge->source()->point());
    } while (++edge != start);

    std::vector<CGAL::Polygon_2<EK>> polygon_boundaries;
    for (auto& [region, polygon] : polygon_boundary_by_region) {
      removeRepeatedPointsInPolygon(polygon);
      if (polygon.size() < 3) {
        continue;
      }
      polygon_boundaries.push_back(std::move(polygon));
    }

    if (polygon_boundaries.empty()) {
      continue;
    }

    std::map<size_t, CGAL::Polygon_2<EK>> polygon_hole_by_region;
    for (typename Arrangement_2::Hole_iterator hole = face->holes_begin();
         hole != face->holes_end(); ++hole) {
      typename Arrangement_2::Ccb_halfedge_circulator start = *hole;
      typename Arrangement_2::Ccb_halfedge_circulator edge = start;
      do {
        polygon_hole_by_region[edge->data()].push_back(edge->source()->point());
      } while (++edge != start);
    }

    std::vector<CGAL::Polygon_2<EK>> polygon_holes;
    for (auto& [region, polygon] : polygon_hole_by_region) {
      CGAL::Polygon_2<EK> original = polygon;
      removeRepeatedPointsInPolygon(polygon);
      if (polygon.size() < 3) {
        continue;
      }
      polygon_holes.push_back(std::move(polygon));
    }

    toSimplePolygonsWithHolesFromBoundariesAndHoles(polygon_boundaries,
                                                    polygon_holes, out);
  }

  return true;
}

template <typename Arrangement_2>
static bool convertArrangementToPolygonsWithHolesNonZero(
    Arrangement_2& arrangement,
    std::vector<CGAL::Polygon_with_holes_2<EK>>& out) {
  std::vector<CGAL::Segment_2<EK>> non_simple;
  return convertArrangementToPolygonsWithHolesNonZero(arrangement, out,
                                                      non_simple);
}

template <typename Arrangement_2>
static bool convertArrangementToPolygonsWithHolesEvenOdd(
    Arrangement_2& arrangement,
    std::vector<CGAL::Polygon_with_holes_2<EK>>& out,
    std::vector<CGAL::Segment_2<EK>>& non_simple) {
  analyzeArrangementRegions(arrangement);

  std::map<size_t, CGAL::Sign> region_sign;

  std::set<typename Arrangement_2::Face_handle> current;
  std::set<typename Arrangement_2::Face_handle> next;

  // FIX: Make this more efficient?
  for (auto edge = arrangement.halfedges_begin();
       edge != arrangement.halfedges_end(); ++edge) {
    region_sign[edge->data()] = CGAL::Sign::ZERO;
  }

  // The unbounded faces all and only have region zero: seed these as negative.
  region_sign[0] = CGAL::Sign::NEGATIVE;
  // Set up an initial negative front expanding from the unbounded faces.
  CGAL::Sign phase = CGAL::Sign::NEGATIVE;
  CGAL::Sign unphase = CGAL::Sign::POSITIVE;
  for (auto face = arrangement.unbounded_faces_begin();
       face != arrangement.unbounded_faces_end(); ++face) {
    current.insert(face);
  }

  // Propagate the wavefront.
  while (!current.empty()) {
    for (auto& face : current) {
      if (face->number_of_outer_ccbs() == 1) {
        typename Arrangement_2::Ccb_halfedge_circulator start =
            face->outer_ccb();
        typename Arrangement_2::Ccb_halfedge_circulator edge = start;
        do {
          const auto twin = edge->twin();
          if (region_sign[twin->data()] == CGAL::Sign::ZERO) {
            region_sign[twin->data()] = unphase;
            next.insert(twin->face());
          }
        } while (++edge != start);
      }

      for (auto hole = face->holes_begin(); hole != face->holes_end(); ++hole) {
        typename Arrangement_2::Ccb_halfedge_circulator start = *hole;
        typename Arrangement_2::Ccb_halfedge_circulator edge = start;
        do {
          auto twin = edge->twin();
          if (edge->face() == twin->face()) {
            // We can't step into degenerate antenna.
            continue;
          }
          if (region_sign[twin->data()] == CGAL::Sign::ZERO) {
            region_sign[twin->data()] = unphase;
            next.insert(twin->face());
          }
        } while (++edge != start);
      }
    }
    current = std::move(next);
    next.clear();

    CGAL::Sign next_phase = unphase;
    unphase = phase;
    phase = next_phase;
  }

  for (auto face = arrangement.faces_begin(); face != arrangement.faces_end();
       ++face) {
    if (face->is_unbounded() || face->number_of_outer_ccbs() != 1) {
      continue;
    }
    std::map<size_t, CGAL::Polygon_2<EK>> polygon_boundary_by_region;
    typename Arrangement_2::Ccb_halfedge_circulator start = face->outer_ccb();
    typename Arrangement_2::Ccb_halfedge_circulator edge = start;
    do {
      polygon_boundary_by_region[edge->data()].push_back(
          edge->source()->point());
    } while (++edge != start);

    std::vector<CGAL::Polygon_2<EK>> polygon_boundaries;
    for (auto& [region, polygon] : polygon_boundary_by_region) {
      if (region_sign[region] != CGAL::Sign::POSITIVE) {
        continue;
      }
      removeRepeatedPointsInPolygon(polygon);
      if (polygon.size() < 3) {
        continue;
      }
      polygon_boundaries.push_back(std::move(polygon));
    }

    if (polygon_boundaries.empty()) {
      continue;
    }

    std::map<size_t, CGAL::Polygon_2<EK>> polygon_hole_by_region;
    for (typename Arrangement_2::Hole_iterator hole = face->holes_begin();
         hole != face->holes_end(); ++hole) {
      typename Arrangement_2::Ccb_halfedge_circulator start = *hole;
      typename Arrangement_2::Ccb_halfedge_circulator edge = start;
      do {
        polygon_hole_by_region[edge->data()].push_back(edge->source()->point());
      } while (++edge != start);
    }

    std::vector<CGAL::Polygon_2<EK>> polygon_holes;
    for (auto& [region, polygon] : polygon_hole_by_region) {
      CGAL::Polygon_2<EK> original = polygon;
      removeRepeatedPointsInPolygon(polygon);
      if (polygon.size() < 3) {
        continue;
      }
      polygon_holes.push_back(std::move(polygon));
    }

    toSimplePolygonsWithHolesFromBoundariesAndHoles(polygon_boundaries,
                                                    polygon_holes, out);
  }
  return true;
}

template <typename Arrangement_2>
static bool convertArrangementToPolygonsWithHolesEvenOdd(
    Arrangement_2& arrangement,
    std::vector<CGAL::Polygon_with_holes_2<EK>>& out) {
  std::vector<CGAL::Segment_2<EK>> non_simple;
  return convertArrangementToPolygonsWithHolesEvenOdd(arrangement, out,
                                                      non_simple);
}
