#pragma once

#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Surface_mesh.h>

#include "hash.h"

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Aff_transformation_3<EK> Tf;

class Geometry {
 public:
  Geometry() {}

  Geometry (CGAL::Surface_mesh<EK::Point_3> mesh) {
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    if (mesh.has_garbage()) {
      mesh.collect_garbage();
    }

    for (const auto& vertex : mesh.vertices()) {
      const auto& point = mesh.point(vertex);
      vertices_.push_back(point);
    }
    for (const auto& facet : mesh.faces()) {
      const auto a = mesh.halfedge(facet);
      const auto b = mesh.next(a);
      const auto c = mesh.next(b);
      triangles_.push_back({ size_t(mesh.source(a)), size_t(mesh.source(b)), size_t(mesh.source(c)) });
    }
  }

  void FillSurfaceMesh(CGAL::Surface_mesh<EK::Point_3>& mesh) {
#if 0
    CGAL::Polygon_mesh_processing::repair_polygon_soup(vertices_, triangles_);
    std::cout << "is_polygon_soup_a_polygon_mesh: " << CGAL::Polygon_mesh_processing::is_polygon_soup_a_polygon_mesh(triangles_) << std::endl;
    CGAL::Polygon_mesh_processing::orient_polygon_soup(vertices_, triangles_);
    CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(vertices_, triangles_, mesh);
    CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(mesh);
#else
    typedef CGAL::Surface_mesh<EK::Point_3>::Vertex_index Vertex_index;
    for (const auto& vertex : vertices_) {
      mesh.add_vertex(vertex);
    }
    for (const auto& triangle : triangles_) {
      if (mesh.add_face(Vertex_index(triangle[0]), Vertex_index(triangle[1]), Vertex_index(triangle[2])) == mesh.null_face()) {
      }
    }
#endif
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
#if 0
    std::cout << "is_closed: " << CGAL::is_closed(mesh) << std::endl;
    std::cout << "does_bound_a_volume: " << CGAL::Polygon_mesh_processing::does_bound_a_volume(mesh) << std::endl;
    std::cout << "orientation: " << CGAL::Polygon_mesh_processing::is_outward_oriented(mesh) << std::endl;
#endif
  }
    
  void Decode(const std::string& text) {
    std::istringstream ss(text);
    for (;;) {
      char key;
      ss >> key;
      if (ss.eof()) {
        break;
      }
      switch (key) {
        case 'v': {
          CGAL::Point_3<EK> point;
          ss >> point;
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
    for (const auto& v : vertices_) {
      ss << "v " << v.x().exact() << " " << v.y().exact() << " " << v.z().exact() << "\n";
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
      for (const auto& triangle : triangles_) {
        ss << "t " << triangle[0] << " " << triangle[1] << " " << triangle[2] << "\n";
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

  size_t AddVertex(const CGAL::Point_3<EK>& point) {
    size_t index = vertices_.size();
    vertices_.push_back(point);
    return index;
  }

 public:
  std::vector<CGAL::Point_3<EK>> vertices_;
  std::vector<size_t> points_;
  std::vector<std::pair<size_t, size_t>> segments_;
  std::vector<std::vector<size_t>> triangles_;
  std::vector<std::pair<std::vector<size_t>, std::vector<std::vector<size_t>>>> faces_;
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

    Napi::Function func = DefineClass(env, "Geometry", {}); // No methods exposed

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("wrapNative", Napi::Function::New(env, [](const Napi::CallbackInfo& info) -> Napi::Value {
      if (info.Length() != 1 || !info[0].IsExternal()) {
        Napi::TypeError::New(info.Env(), "Expected an external for wrapNative").ThrowAsJavaScriptException();
        return info.Env().Undefined();
      }
      Geometry* native = static_cast<Geometry*>(info[0].As<Napi::External<Geometry>>().Data());
      return WrapNativeObject(info.Env(), native);
    }));

    return exports;
  }

  static Napi::Object WrapNativeObject(Napi::Env env, Geometry* nativeObject) {
    auto external = Napi::External<Geometry>::New(env, nativeObject);
    Napi::Object obj = constructor.New({ external });
    GeometryWrapper* wrapper = Napi::ObjectWrap<GeometryWrapper>::Unwrap(obj);
    if (wrapper) {
      wrapper->_geometry = nativeObject;
    }
    return obj;
  }

  Geometry& Get() { return *_geometry; }

  GeometryWrapper(const Napi::CallbackInfo& info) : Napi::ObjectWrap<GeometryWrapper>(info) {
    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);
    if (info.Length() == 1 && info[0].IsExternal()) {
      _geometry = static_cast<Geometry*>(info[0].As<Napi::External<Geometry>>().Data());
    } else {
      Napi::TypeError::New(env, "Internal constructor expects a single external").ThrowAsJavaScriptException();
    }
  }

  ~GeometryWrapper() {
    delete _geometry;
  }

  static Napi::FunctionReference constructor;
  Geometry* _geometry;
};

Napi::FunctionReference GeometryWrapper::constructor;
