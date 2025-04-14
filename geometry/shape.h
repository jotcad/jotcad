#pragma once
#include <CGAL/Aff_transformation_3.h>
#include "geometry.h"

Napi::Object NewTags(Napi::Env env) { return Napi::Object::New(env); }

Napi::Array NewGeometries(Napi::Env env) { return Napi::Array::New(env); }

Napi::Array NewShapes(Napi::Env env) { return Napi::Array::New(env); }

Napi::Value NewTf(Napi::Env env) { return env.Null(); }

Napi::Object NewShape(Napi::Env env) {
  auto shape = Napi::Object::New(env);
  shape.Set("geometry", NewGeometries(env));
  shape.Set("shapes", NewShapes(env));
  shape.Set("tags", NewTags(env));
  shape.Set("tf", NewTf(env));
  return shape;
}

Napi::Object CopyNapiObject(Napi::Object original) {
  Napi::Object copy = Napi::Object::New(original.Env());
  Napi::Array propertyNames = original.GetPropertyNames();
  uint32_t length = propertyNames.Length();
  for (uint32_t i = 0; i < length; ++i) {
    auto propertyName = propertyNames.Get(i);
    copy.Set(propertyName, original.Get(propertyName));
  }
  return copy;
}

const char* GetNapiValueTypeString(Napi::Value val) {
  napi_valuetype valueType = val.Type();
  switch (valueType) {
    case napi_undefined: return "undefined";
    case napi_null:      return "null";
    case napi_boolean:   return "boolean";
    case napi_number:    return "number";
    case napi_string:    return "string";
    case napi_symbol:    return "symbol";
    case napi_object:    return "object"; // Base object type
    case napi_function:  return "function";
    case napi_external:  return "external";
    case napi_bigint:    return "bigint";
    default:             return "unknown"; // Should not happen in standard usage
  }
}

class Shape {
 public:
  Shape(Napi::Object shape): napi_(shape) {}
  Shape(const Shape& shape): napi_(CopyNapiObject(shape.ToNapi())) {}

  Napi::Object ToNapi() const {
    return napi_;
  }

  Napi::Array Shapes() const {
    return napi_.Get("shapes").As<Napi::Array>();
  }

  void SetShapes(Napi::Array shapes) {
    if (shapes.IsUndefined()) {
      napi_.Set("shapes", NewShapes(napi_.Env()));
    } else {
      napi_.Set("shapes", shapes);
    }
  }

  Napi::Object Tags() const {
    return napi_.Get("tags").As<Napi::Object>();
  }

  void SetTags(Napi::Object tags) {
    napi_.Set("tags", tags);
  }

  Napi::String Tf() const {
    return napi_.Get("tf").As<Napi::String>();
  }

  void SetTf(Napi::String tf) {
    napi_.Set("tf", tf);
  }

  CGAL::Aff_transformation_3<EK> CgalTf() const {
    auto tf = Tf();
    if (tf.IsNull()) {
      return CGAL::Identity_transformation();
    }
    std::string text = Tf().Utf8Value();
    std::istringstream s(text);
    EK::FT v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13;
    s >> v1;
    s >> v2;
    s >> v3;
    s >> v4;
    s >> v5;
    s >> v6;
    s >> v7;
    s >> v8;
    s >> v9;
    s >> v10;
    s >> v11;
    s >> v12;
    s >> v13;
    return CGAL::Aff_transformation_3<EK>(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11,
                                    v12, v13);
  }

  Napi::Array GeometryIds() const {
    return napi_.Get("geometry").As<Napi::Array>();
  }

  void SetGeometryIds(Napi::Array ids) {
    napi_.Set("geometry", ids);
  }

  void SetGeometryIds(const std::vector<std::string>& ids) {
    auto array = Napi::Array::New(napi_.Env(), ids.size());
    uint32_t nth = 0;
    for (const auto& id : ids) {
      array.Set(nth++, id);
    }
    SetGeometryIds(array);
  }

  Shape NthShape(uint32_t nth) const {
    return Shape(Shapes().Get(nth).As<Napi::Object>());
  }

  const std::string NthGeometryId(uint32_t nth) const {
    return GeometryIds().Get(nth).As<Napi::String>().Utf8Value();
  }

  void Walk(const std::function<void(Napi::Object)>& op) const {
    op(napi_);
    auto shapes = Shapes();
    for (uint32_t nth = 0; nth < shapes.Length(); nth++) {
      auto element = NthShape(nth);
      element.Walk(op);
    }
  }

  void Each(const Geometries& geometries,
            const CGAL::Aff_transformation_3<EK>& tf,
            const std::function<void(const CGAL::Aff_transformation_3<EK>&, std::shared_ptr<Geometry>)>& op) const {
    auto geometry_ids = GeometryIds();
    for (uint32_t nth = 0; nth < geometry_ids.Length(); nth++) {
      auto id = NthGeometryId(nth);
      auto geom = geometries.Get(id);
      op(tf, geom);
    }
  }

 public:
  const Napi::Object napi_;
};
