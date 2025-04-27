#pragma once

#include "geometry.h"
typedef Napi::Value GeometryId;

class Assets {
 public:
  Assets(Napi::Object napi):
    napi_(napi), undefined_(napi.Env().Undefined()) {
  }

  Napi::Object Space(const std::string& key) {
    Napi::Object obj = napi_.Get(key).As<Napi::Object>();
    if (obj == undefined_) {
      obj = Napi::Object::New(napi_.Env());
      napi_.Set(key, obj);
    }
    return obj;
  }

  Napi::Env Env() const {
    return napi_.Env();
  }

  GeometryId TextId(const Geometry& geometry) {
    std::string text;
    geometry.Encode(text);
    auto key = Napi::String::New(napi_.Env(), ComputeTextHash(text));
    Space("text").Set(key, Napi::String::New(napi_.Env(), text));
    return key;
  }

  const std::string GetText(const GeometryId& id) {
    // This expects to always find an id.
    Napi::Object space = Space("text");
    return space.Get(id).As<Napi::String>().Utf8Value();
  }

  Geometry& GetGeometry(const GeometryId& id) {
    Napi::Object space = Space("geometry");
    Napi::Object geometry = space.Get(id).As<Napi::Object>();
    if (geometry == undefined_) {
      std::string text = GetText(id);
      auto native = std::make_unique<Geometry>();
      native->Decode(text);
      Geometry* r = native.release();
      geometry = GeometryWrapper::WrapNativeObject(napi_.Env(), r);
      space.Set(id, geometry);
    }
    Geometry& result = Napi::ObjectWrap<GeometryWrapper>::Unwrap(geometry)->Get();
    return result;
  }

 public:
  const Napi::Object napi_;
  const Napi::Value undefined_;
};
