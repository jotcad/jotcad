#pragma once

static double to_double(EK::FT ft) { return CGAL::to_double(ft.exact()); }

static EK::FT to_FT(const double v) { return EK::FT(v); }

static void write_point(const CGAL::Point_3<EK>& p, std::ostringstream& o) {
  o << p.x().exact() << " ";
  o << p.y().exact() << " ";
  o << p.z().exact();
}

static void write_point(const CGAL::Point_3<EK>& p, std::string& s) {
  std::ostringstream o;
  write_point(p, o);
  s = std::move(o.str());
}

// Approximations are in 100ths of a mm.
static void write_approximate_point(const CGAL::Point_3<EK>& p,
                                    std::ostringstream& o) {
  o << round(CGAL::to_double(p.x().exact()) * 1000) << " ";
  o << round(CGAL::to_double(p.y().exact()) * 1000) << " ";
  o << round(CGAL::to_double(p.z().exact()) * 1000);
}

static void read_point(CGAL::Point_3<EK>& point, std::istringstream& input) {
  EK::FT x, y, z;
  input >> x;
  input >> y;
  input >> z;
  point = CGAL::Point_3<EK>(x, y, z);
}

static void read_point(CGAL::Point_3<EK>& point, const std::string& input) {
  std::istringstream stream(input);
  read_point(point, stream);
}

static void read_point_approximate(CGAL::Point_3<EK>& point,
                                   std::istringstream& input) {
  double x, y, z;
  input >> x;
  input >> y;
  input >> z;
  point = CGAL::Point_3<EK>(x, y, z);
}

static void read_segment(CGAL::Segment_3<EK>& segment,
                         std::istringstream& input) {
  CGAL::Point_3<EK> source;
  read_point(source, input);
  CGAL::Point_3<EK> target;
  read_point(target, input);
  segment = CGAL::Segment_3<EK>(source, target);
}

static void write_segment(const CGAL::Segment_3<EK>& s, std::ostringstream& o) {
  write_point(s.source(), o);
  o << " ";
  write_point(s.target(), o);
}

static void write_segment(const CGAL::Segment_3<EK>& s, std::string& str) {
  std::ostringstream o;
  write_segment(s, o);
  str = std::move(o.str());
}

template <typename emit>
static void emitPoint(CGAL::Point_3<EK> p, const emit& emit_point) {
  std::ostringstream exact;
  write_point(p, exact);
  emit_point(CGAL::to_double(p.x().exact()), CGAL::to_double(p.y().exact()),
             CGAL::to_double(p.z().exact()), exact.str());
}

template <typename Emitter>
static void emitPoint2(CGAL::Point_2<EK> p, Emitter emit_point) {
  std::ostringstream x;
  x << p.x().exact();
  std::string xs = x.str();
  std::ostringstream y;
  y << p.y().exact();
  std::string ys = y.str();
  emit_point(CGAL::to_double(p.x().exact()), CGAL::to_double(p.y().exact()), xs,
             ys);
}

template <typename Emitter>
static void emitNthPoint(int nth, CGAL::Point_3<EK> p, Emitter emit_point) {
  std::ostringstream x;
  x << p.x().exact();
  std::string xs = x.str();
  std::ostringstream y;
  y << p.y().exact();
  std::string ys = y.str();
  std::ostringstream z;
  z << p.z().exact();
  std::string zs = z.str();
  emit_point(nth, CGAL::to_double(p.x().exact()),
             CGAL::to_double(p.y().exact()), CGAL::to_double(p.z().exact()), xs,
             ys, zs);
}
