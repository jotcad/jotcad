#pragma once

#include <CGAL/Random.h>

static void make_deterministic() {
  CGAL::get_default_random() = CGAL::Random(0);
  std::srand(0);
}