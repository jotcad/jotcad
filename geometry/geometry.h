#pragma once
#include "hash.h"

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;

class Geometry {
 public:
  Geometry(Napi::Value napi): napi_(napi) {}

  virtual const std::string& Type() const = 0;
  virtual Napi::Value ToNapi(Napi::Env env) const = 0;
  virtual void FillPoints(const CGAL::Aff_transformation_3<EK>&, std::vector<CGAL::Point_3<EK>>&) const = 0;

 public:
  const Napi::Value napi_;
};

class Points : public Geometry {
 public:
  Points(Napi::Value napi): Geometry(napi) {
    auto text = napi_.As<Napi::Object>().Get("points").As<Napi::String>().Utf8Value();
    std::istringstream ss(text);
    while (!ss.eof()) {
      CGAL::Point_3<EK> point;
      ss >> point;
      points_.push_back(point);
    }
  }

  Points(Napi::Env env, const std::vector<CGAL::Point_3<EK>>& points): Geometry(env.Undefined()), points_(points) {}

  const std::string& Type() const override {
    return type_name_;
  }

  Napi::Value ToNapi(Napi::Env env) const override {
    std::string out;
    std::ostringstream ss(out);
    for (const auto& point : points_) {
      ss << point << "  ";
    }
    auto shape = Napi::Object::New(env);
    shape.Set("type", Type());
    shape.Set("points", ss.str());
    return shape;
  }

  void FillPoints(const CGAL::Aff_transformation_3<EK>& tf, std::vector<CGAL::Point_3<EK>>& out) const override {
    for (const auto& point : points_) {
      out.push_back(point);
    }
  }

 private:
  const std::string type_name_ = "points";
  std::vector<CGAL::Point_3<EK>> points_;
};

class Segments : public Geometry {
 public:
  Segments(Napi::Value napi): Geometry(napi) {
    auto text = napi_.As<Napi::Object>().Get("segments").As<Napi::String>().Utf8Value();
    std::istringstream ss(text);
    while (!ss.eof()) {
      CGAL::Segment_3<EK> segment;
      ss >> segment;
      segments_.push_back(segment);
    }
  }

  Segments(Napi::Env env, const std::vector<CGAL::Segment_3<EK>>& segments): Geometry(env.Undefined()), segments_(segments) {}

  const std::string& Type() const override {
    return type_name_;
  }

  Napi::Value ToNapi(Napi::Env env) const override {
    std::string out;
    std::ostringstream ss(out);
    for (const auto& segment : segments_) {
      ss << segment << "  ";
    }
    auto shape = Napi::Object::New(env);
    shape.Set("type", Type());
    shape.Set("segments", ss.str());
    return shape;
  }

  void FillPoints(const CGAL::Aff_transformation_3<EK>& tf, std::vector<CGAL::Point_3<EK>>& out) const override {
    bool first = true;
    for (const auto& segment : segments_) {
      if (first) {
        out.push_back(segment.source());
        first = false;
      }
      out.push_back(segment.target());
    }
  }

 private:
  const std::string type_name_ = "segments";
  std::vector<CGAL::Segment_3<EK>> segments_;
};

std::shared_ptr<Geometry> GeometryFromNapi(Napi::Object napi) {
  auto text = napi.Get("type").As<Napi::String>().Utf8Value();
  if (text == "points") {
    return std::make_shared<Points>(napi);
  } else if (text == "segments") {
    return std::make_shared<Segments>(napi);
  } else {
    return std::make_shared<Points>(napi.Env().Undefined());
  }
}

class Geometries {
 public:
  Geometries(Napi::Object napi): napi_(napi) {
  }

  std::shared_ptr<Geometry> Get(const std::string& id) const {
    auto obj = napi_.Get(id).As<Napi::Object>();
    return GeometryFromNapi(obj);
  }

  Napi::Env Env() const {
    return napi_.Env();
  }

  std::string Add(const std::shared_ptr<Geometry>& geometry) {
    auto napi_geometry = geometry->ToNapi(Env());
    auto hash = ComputeGeometryHash(napi_geometry);
    napi_.Set(hash, napi_geometry);
    return hash;
  }

 public:
  Napi::Object napi_;
};

