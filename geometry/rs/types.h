#pragma once

#include <array>
#include <vector>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Point_3.h>
#include <CGAL/Triangle_3.h>
#include <CGAL/Vector_3.h>
#include <CGAL/Surface_mesh.h>

namespace ruled_surfaces {

constexpr double kEpsilon = 1e-9;

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using PointCgal = Kernel::Point_3;
using VectorCgal = Kernel::Vector_3;
using TriangleCgal = Kernel::Triangle_3;
using PolygonalChain = std::vector<PointCgal>;
using Mesh = CGAL::Surface_mesh<PointCgal>;
using PolygonSoup = std::vector<std::array<PointCgal, 3>>;

} // namespace ruled_surfaces


