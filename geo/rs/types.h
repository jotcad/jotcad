#pragma once

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Point_3.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Triangle_3.h>
#include <CGAL/Vector_3.h>

#include <array>
#include <vector>

namespace ruled_surfaces {

constexpr double kEpsilon = 1e-9;

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using PointCgal = Kernel::Point_3;
using VectorCgal = Kernel::Vector_3;
using TriangleCgal = Kernel::Triangle_3;
using PolygonalChain = std::vector<PointCgal>;
using Mesh = CGAL::Surface_mesh<PointCgal>;
using PolygonSoup = std::vector<std::array<PointCgal, 3>>;

template <typename TS, typename SR>
class SeamSearchSA;

template <typename TS>
class SeamSearchAll;

template <typename T>
struct is_seam_search_sa : std::false_type {};
template <typename TS, typename SR>
struct is_seam_search_sa<SeamSearchSA<TS, SR>> : std::true_type {};

template <typename T>
struct is_seam_search_all : std::false_type {};
template <typename TS>
struct is_seam_search_all<SeamSearchAll<TS>> : std::true_type {};

}  // namespace ruled_surfaces
