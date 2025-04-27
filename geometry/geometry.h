#pragma once
#include "hash.h"

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Aff_transformation_3<EK> Tf;

class Geometry {
 public:
  Geometry() {}

  void Decode(const std::string& text) {
    std::istringstream ss(text);
    while (!ss.eof()) {
      char key;
      ss >> key;
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
        case 'f': {
          size_t v;
          faces_.emplace_back();
          auto& face_with_holes = faces_.back();
          auto& face = faces_.back().first;
          while (ss >> v) {
            face.push_back(v);
          }
        }
        case 'h': {
          size_t v;
          auto& face = faces_.back();
          auto& holes = face.second;
          holes.emplace_back();
          auto& hole = holes.back();
          while (ss >> v) {
            hole.push_back(v);
          }
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

 public:
  std::vector<CGAL::Point_3<EK>> vertices_;
  std::vector<size_t> points_;
  std::vector<std::pair<size_t, size_t>> segments_;
  std::vector<std::pair<std::vector<size_t>, std::vector<std::vector<size_t>>>> faces_;
};

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
