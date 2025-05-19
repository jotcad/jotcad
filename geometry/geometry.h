#pragma once

#include <CGAL/Cartesian_converter.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_triangulation_decomposition_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>

#include "hash.h"

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Exact_predicates_inexact_constructions_kernel IK;
typedef CGAL::Simple_cartesian<double> CK;
typedef CGAL::Aff_transformation_3<EK> Tf;

template <typename K>
static typename CGAL::Surface_mesh<typename K::Point_3>::Vertex_index
EnsureVertex(
    CGAL::Surface_mesh<typename K::Point_3>& mesh,
    std::map<typename K::Point_3,
             typename CGAL::Surface_mesh<typename K::Point_3>::Vertex_index>&
        vertices,
    const typename K::Point_3& point) {
  auto it = vertices.find(point);
  if (it == vertices.end()) {
    auto new_vertex = mesh.add_vertex(point);
    vertices[point] = new_vertex;
    return new_vertex;
  }
  return it->second;
}

class Geometry;
std::ostream& operator<<(std::ostream& os, const Geometry& g);

class Geometry {
 public:
  Geometry() {}

  template <typename K>
  Geometry(CGAL::Surface_mesh<typename K::Point_3> mesh) {
    CGAL::Cartesian_converter<K, EK> to_EK;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    if (mesh.has_garbage()) {
      mesh.collect_garbage();
    }

    for (const auto& vertex : mesh.vertices()) {
      const auto& point = mesh.point(vertex);
      vertices_.push_back(to_EK(point));
    }
    for (const auto& facet : mesh.faces()) {
      const auto a = mesh.halfedge(facet);
      const auto b = mesh.next(a);
      const auto c = mesh.next(b);
      triangles_.push_back({size_t(mesh.source(a)), size_t(mesh.source(b)),
                            size_t(mesh.source(c))});
    }
  }

  template <typename K>
  void DecodeSurfaceMesh(const CGAL::Surface_mesh<typename K::Point_3>& mesh) {
    CGAL::Cartesian_converter<K, EK> to_EK;
    for (const auto& vertex : mesh.vertices()) {
      AddVertex(mesh.point(vertex));
    }
    for (const auto& face : mesh.faces()) {
      const auto a = mesh.halfedge(face);
      const auto b = mesh.next(a);
      const auto c = mesh.next(b);
      AddTriangle(mesh.source(a), mesh.source(b), mesh.source(c));
    }
  }

  template <typename K>
  void EncodeSurfaceMesh(CGAL::Surface_mesh<typename K::Point_3>& mesh) {
    typedef typename CGAL::Surface_mesh<typename K::Point_3>::Vertex_index
        Vertex_index;
    CGAL::Cartesian_converter<EK, K> to_K;
    for (const auto& vertex : vertices_) {
      mesh.add_vertex(to_K(vertex));
    }
    for (const auto& triangle : triangles_) {
      if (mesh.add_face(Vertex_index(triangle[0]), Vertex_index(triangle[1]),
                        Vertex_index(triangle[2])) == mesh.null_face()) {
      }
    }
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
  }

  bool EncodeFaceSurfaceMesh(CGAL::Surface_mesh<EK::Point_3>& mesh) {
    const EK::Plane_3 plane(0, 0, 1, 0);
    std::vector<CGAL::Polygon_with_holes_2<EK>> pwhs;
    std::map<EK::Point_3, CGAL::Surface_mesh<EK::Point_3>::Vertex_index>
        vertex_map;
    CGAL::Polygon_triangulation_decomposition_2<EK> triangulator;
    for (const auto& [face, holes] : faces_) {
      CGAL::Polygon_2<EK> pwh_boundary;
      std::vector<CGAL::Polygon_2<EK>> pwh_holes;
      for (const auto& point : face) {
        pwh_boundary.push_back(plane.to_2d(vertices_[point]));
      }
      for (const auto& hole : holes) {
        CGAL::Polygon_2<EK> polygon;
        for (const auto& point : hole) {
          polygon.push_back(plane.to_2d(vertices_[point]));
        }
        pwh_holes.push_back(std::move(polygon));
      }
      pwhs.emplace_back(pwh_boundary, pwh_holes.begin(), pwh_holes.end());
    }
    for (const auto& pwh : pwhs) {
      std::vector<CGAL::Polygon_2<EK>> facets;
      triangulator(pwh, std::back_inserter(facets));
      for (auto& facet : facets) {
        if (facet.orientation() != CGAL::Sign::POSITIVE) {
          facet.reverse_orientation();
        }
        std::vector<CGAL::Surface_mesh<EK::Point_3>::Vertex_index> vertices;
        for (const auto& point : facet) {
          vertices.push_back(
              EnsureVertex<EK>(mesh, vertex_map, plane.to_3d(point)));
        }
        if (mesh.add_face(vertices) ==
            CGAL::Surface_mesh<CGAL::Point_3<EK>>::null_face()) {
          return false;
        }
      }
    }
    return true;
  }

  void Decode(std::istream& ss) {
    for (;;) {
      char key;
      ss >> key;
      if (ss.eof()) {
        break;
      }
      switch (key) {
        case 'V': {
          size_t count;
          ss >> count;
          vertices_.reserve(count);
          break;
        }
        case 'v': {
          CGAL::Point_3<EK> point;
          ss >> point;
          CGAL::Point_3<IK> inexactPoint;
          ss >> inexactPoint;
          vertices_.push_back(point);
          break;
        }
        case 'p': {
          size_t i;
          while (ss >> i) {
            points_.push_back(i);
            if (ss.peek() == '\n') {
              break;
            }
          }
          break;
        }
        case 's': {
          size_t source, target;
          while (ss >> source) {
            ss >> target;
            segments_.emplace_back(source, target);
            if (ss.peek() == '\n') {
              break;
            }
          }
          break;
        }
        case 'T': {
          size_t count;
          ss >> count;
          triangles_.reserve(count);
          break;
        }
        case 't': {
          size_t v;
          triangles_.emplace_back();
          auto& triangle = triangles_.back();
          while (ss >> v) {
            triangle.push_back(v);
            if (ss.peek() == '\n') {
              break;
            }
          }
          break;
        }
        case 'f': {
          size_t v;
          faces_.emplace_back();
          auto& face_with_holes = faces_.back();
          auto& face = faces_.back().first;
          while (ss >> v) {
            face.push_back(v);
            if (ss.peek() == '\n') {
              break;
            }
          }
          break;
        }
        case 'h': {
          size_t v;
          auto& face = faces_.back();
          auto& holes = face.second;
          holes.emplace_back();
          auto& hole = holes.back();
          while (ss >> v) {
            hole.push_back(v);
            if (ss.peek() == '\n') {
              break;
            }
          }
          break;
        }
      }
    }
  }

  void Encode(std::string& text) const {
    std::ostringstream ss;
    if (!vertices_.empty()) {
      ss << "V " << vertices_.size() << "\n";
      for (const auto& v : vertices_) {
        ss << "v ";
        ss << v.x().exact() << " ";
        ss << v.y().exact() << " ";
        ss << v.z().exact() << " ";
        ss << v << "\n";
      }
    }
    if (!points_.empty()) {
      ss << "p";
      for (const auto& p : points_) {
        ss << " " << p;
      }
      ss << "\n";
    }
    if (!segments_.empty()) {
      ss << "s";
      for (const auto& [s, t] : segments_) {
        ss << " " << s << " " << t;
      }
      ss << "\n";
    }
    if (!triangles_.empty()) {
      ss << "T " << triangles_.size() << "\n";
      for (const auto& triangle : triangles_) {
        ss << "t " << triangle[0] << " " << triangle[1] << " " << triangle[2]
           << "\n";
      }
    }
    if (!faces_.empty()) {
      for (const auto& [face, holes] : faces_) {
        ss << "f";
        for (const auto& v : face) {
          ss << " " << v;
        }
        ss << "\n";
        for (const auto& hole : holes) {
          ss << "h";
          for (const auto& v : hole) {
            ss << " " << v;
          }
          ss << "\n";
        }
      }
    }
    text = ss.str();
  }

  Geometry Transform(const Tf& tf) {
    Geometry transformed = *this;
    for (auto& vertex : transformed.vertices_) {
      vertex = vertex.transform(tf);
    }
    return transformed;
  }

  template <typename K>
  size_t AddVertex(const CGAL::Point_3<K>& point, bool deduplicate = false) {
    // TODO: Make this efficient.
    CGAL::Cartesian_converter<K, EK> to_EK;
    CGAL::Point_3<EK> ek_point = to_EK(point);
    if (deduplicate) {
      for (size_t nth = 0; nth < vertices_.size(); nth++) {
        if (vertices_[nth] == ek_point) {
          return nth;
        }
      }
    }
    size_t index = vertices_.size();
    vertices_.push_back(ek_point);
    return index;
  }

  template <typename K>
  void AddSegment(const CGAL::Point_3<K>& source,
                  const CGAL::Point_3<K>& target) {
    if (source == target) {
      return;
    }
    segments_.emplace_back(AddVertex(source), AddVertex(target));
  }

  template <typename K>
  size_t AddTriangle(const CGAL::Point_3<K>& a, const CGAL::Point_3<K>& b,
                     const CGAL::Point_3<K>& c) {
    if (a == b || b == c || c == a) {
      return -1;
    }
    std::vector<size_t> triangle = {AddVertex(a), AddVertex(b), AddVertex(c)};
    size_t index = triangles_.size();
    triangles_.push_back(std::move(triangle));
    return index;
  }

  size_t AddTriangle(size_t a, size_t b, size_t c) {
    if (a == b || b == c || c == a) {
      return -1;
    }
    std::vector<size_t> triangle = {a, b, c};
    size_t index = triangles_.size();
    triangles_.push_back(std::move(triangle));
    return index;
  }

 public:
  std::vector<CGAL::Point_3<EK>> vertices_;
  std::vector<size_t> points_;
  std::vector<std::pair<size_t, size_t>> segments_;
  std::vector<std::vector<size_t>> triangles_;
  std::vector<std::pair<std::vector<size_t>, std::vector<std::vector<size_t>>>>
      faces_;
};

std::ostream& operator<<(std::ostream& os, const Geometry& g) {
  std::string text;
  g.Encode(text);
  os << text;
  return os;
}

class GeometryWrapper : public Napi::ObjectWrap<GeometryWrapper> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);

    Napi::Function func = DefineClass(
        env, "Geometry",
        {InstanceMethod("AddTriangle", &GeometryWrapper::AddTriangle),
         InstanceMethod("AddVertexInexact", &GeometryWrapper::AddVertexInexact),
         InstanceMethod("ReserveVertices", &GeometryWrapper::ReserveVertices),
         InstanceMethod("ReserveTriangles",
                        &GeometryWrapper::ReserveTriangles)});

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

#if 0
    exports.Set("wrapNative",
                Napi::Function::New(
                    env, [](const Napi::CallbackInfo& info) -> Napi::Value {
                      if (info.Length() != 1 || !info[0].IsExternal()) {
                        Napi::TypeError::New(
                            info.Env(), "Expected an external for wrapNative")
                            .ThrowAsJavaScriptException();
                        return info.Env().Undefined();
                      }
                      Geometry* native = static_cast<Geometry*>(
                          info[0].As<Napi::External<Geometry>>().Data());
                      return WrapNativeObject(info.Env(), native);
                    }));
#endif

    exports.Set("Geometry", func);

    return exports;
  }

  static Napi::Object WrapNativeObject(Napi::Env env, Geometry* nativeObject) {
    auto external = Napi::External<Geometry>::New(env, nativeObject);
    Napi::Object obj = constructor.New({external});
    GeometryWrapper* wrapper = Napi::ObjectWrap<GeometryWrapper>::Unwrap(obj);
    if (wrapper) {
      wrapper->_geometry = nativeObject;
    }
    return obj;
  }

  Geometry& Get() { return *_geometry; }

  GeometryWrapper(const Napi::CallbackInfo& info)
      : Napi::ObjectWrap<GeometryWrapper>(info) {
    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);
    if (info.Length() == 0) {
      _geometry = new Geometry();
    } else if (info.Length() == 1 && info[0].IsExternal()) {
      _geometry =
          static_cast<Geometry*>(info[0].As<Napi::External<Geometry>>().Data());
    } else {
      Napi::TypeError::New(env,
                           "Internal constructor expects a single external")
          .ThrowAsJavaScriptException();
    }
  }

  Napi::Value AddVertexInexact(const Napi::CallbackInfo& info) {
    AssertArgCount(info, 3);
    double x = info[0].As<Napi::Number>().DoubleValue();
    double y = info[1].As<Napi::Number>().DoubleValue();
    double z = info[2].As<Napi::Number>().DoubleValue();
    size_t index = Get().AddVertex(IK::Point_3(x, y, z), false);
    return Napi::Number::New(info.Env(), index);
  }

  Napi::Value AddTriangle(const Napi::CallbackInfo& info) {
    AssertArgCount(info, 3);
    uint32_t a = info[0].As<Napi::Number>().Uint32Value();
    uint32_t b = info[1].As<Napi::Number>().Uint32Value();
    uint32_t c = info[2].As<Napi::Number>().Uint32Value();
    size_t index = Get().AddTriangle(a, b, c);
    return Napi::Number::New(info.Env(), index);
  }

  Napi::Value ReserveVertices(const Napi::CallbackInfo& info) {
    AssertArgCount(info, 1);
    uint32_t count = info[0].As<Napi::Number>().Uint32Value();
    Get().vertices_.reserve(count);
    return info.Env().Undefined();
  }

  Napi::Value ReserveTriangles(const Napi::CallbackInfo& info) {
    AssertArgCount(info, 1);
    uint32_t count = info[0].As<Napi::Number>().Uint32Value();
    Get().triangles_.reserve(count);
    return info.Env().Undefined();
  }

  ~GeometryWrapper() { delete _geometry; }

  static Napi::FunctionReference constructor;
  Geometry* _geometry;
};

Napi::FunctionReference GeometryWrapper::constructor;
