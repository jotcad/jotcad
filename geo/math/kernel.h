#pragma once
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Aff_transformation_3.h>

namespace jotcad {
namespace geo {

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef EK::Point_3 Point_3;
typedef EK::Point_2 Point_2;
typedef EK::Vector_3 Vector_3;
typedef EK::FT FT;
typedef CGAL::Aff_transformation_3<EK> Transformation;

} // namespace geo
} // namespace jotcad
