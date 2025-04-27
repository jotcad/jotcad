#pragma once
#include <CGAL/Aff_transformation_3.h>
#include "assets.h"
#include "geometry.h"
#include "transform.h"

class Shape {
 public:
  Shape(Napi::Object shape): napi_(shape) {}

  Napi::Object ToNapi() const {
    return napi_;
  }

  Napi::String GeometryId() {
    if (!id_) {
      id_ = napi_.Get("geometry").As<Napi::String>();
    }
    return *id_;
  }

  void SetTf(Napi::String tf) {
    napi_.Set("tf", tf);
  }

  void SetTf(const std::string& tf) {
    napi_.Set("tf", Napi::String::New(napi_.Env(), tf));
  }

  CGAL::Aff_transformation_3<EK> GetTf() {
    if (!tf_) {
      tf_ = DecodeTf(napi_.Get("tf"));
    }
    return *tf_;
  }

 public:
  const Napi::Object napi_;
  std::optional<Tf> tf_;
  std::optional<Napi::String> id_;
};
