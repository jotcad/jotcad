#pragma once
#include <CGAL/Aff_transformation_3.h>

#include "assets.h"
#include "geometry.h"
#include "transform.h"

class Shape {
 public:
  Shape() {}
  Shape(Napi::Object shape) : napi_(shape) {}
  Shape(const Shape& source) : napi_(source.ToNapi()) {}

  Napi::Object ToNapi() const { return napi_; }

  Napi::String GeometryId() {
    if (!id_) {  // Only retrieve if not already cached
      Napi::Value geometryValue = napi_.Get("geometry");
      if (geometryValue.IsUndefined()) {
        Napi::Error::New(napi_.Env(),
                         "Shape.GeometryId(): 'geometry' property is "
                         "undefined. Expected a GeometryId string.")
            .ThrowAsJavaScriptException();
        // Fallback to an empty Napi::String for compilation, though exception
        // should halt execution.
        id_ = Napi::String::New(napi_.Env(), "");
      } else {
        // Let Napi's As<Napi::String>() throw if it's not a string.
        id_ = geometryValue.As<Napi::String>();
      }
    }
    return *id_;
  }

  bool HasGeometryId() {
    // Simply check if the 'geometry' property exists and is not undefined.
    return napi_.Get("geometry") != napi_.Env().Undefined();
  }

  bool GetMask(Shape& mask) {
    Napi::Object object = napi_.Get("mask").As<Napi::Object>();
    if (object == napi_.Env().Undefined()) {
      return false;
    }
    mask = Shape(object);
    return true;
  }

  void SetTf(Napi::String tf) { napi_.Set("tf", tf); }

  void SetTf(const std::string& tf) {
    napi_.Set("tf", Napi::String::New(napi_.Env(), tf));
  }

  CGAL::Aff_transformation_3<EK> GetTf() {
    if (!tf_) {
      tf_ = DecodeTf(napi_.Get("tf"));
    }
    return *tf_;
  }

  bool Walk(const std::function<bool(Shape&)>& op) {
    if (!op(*this)) {
      return false;
    }
    Napi::Array shapes = napi_.Get("shapes").As<Napi::Array>();
    if (shapes == napi_.Env().Undefined()) {
      return true;
    }
    for (uint32_t nth = 0; nth < shapes.Length(); nth++) {
      Shape shape(shapes.Get(nth).As<Napi::Object>());
      shape.Walk(op);
    }
    return true;
  }

  bool ForShapes(const std::function<bool(Shape&)> op) {
    Napi::Array shapes = napi_.Get("shapes").As<Napi::Array>();
    if (shapes == napi_.Env().Undefined()) {
      return true;
    }
    for (uint32_t nth = 0; nth < shapes.Length(); nth++) {
      Shape shape(shapes.Get(nth).As<Napi::Object>());
      if (!op(shape)) {
        return false;
      }
    }
    return true;
  }

 public:
  Napi::Object napi_;
  std::optional<Tf> tf_;
  std::optional<Napi::String> id_;
};
