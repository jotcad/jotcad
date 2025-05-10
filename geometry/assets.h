#pragma once

#include <CGAL/Surface_mesh.h>

#include "geometry.h"
#include "surface_mesh.h"

typedef Napi::Value GeometryId;

class Assets {
 public:
  Assets(Napi::Object napi) : napi_(napi), undefined_(napi.Env().Undefined()) {}

  Napi::Object Space(const std::string& key) {
    Napi::Object obj = napi_.Get(key).As<Napi::Object>();
    if (obj == undefined_) {
      obj = Napi::Object::New(napi_.Env());
      napi_.Set(key, obj);
    }
    return obj;
  }

  Napi::Env Env() const { return napi_.Env(); }

  GeometryId TextId(const Geometry& geometry) {
    std::string text;
    geometry.Encode(text);
    auto key = Napi::String::New(napi_.Env(), ComputeTextHash(text));
    Space("text").Set(key, Napi::String::New(napi_.Env(), text));
    return key;
  }

  GeometryId UndefinedId() { return undefined_; }

  const std::string GetText(const GeometryId& id) {
    // This expects to always find an id.
    Napi::Object space = Space("text");
    auto result = space.Get(id).As<Napi::String>().Utf8Value();
    return result;
  }

  Geometry& GetGeometry(const GeometryId& id) {
    Napi::Object space = Space("geometry");
    Napi::Object wrapped_geometry = space.Get(id).As<Napi::Object>();
    if (wrapped_geometry == undefined_) {
      std::string text = GetText(id);
      auto native = std::make_unique<Geometry>();
      native->Decode(text);
      Geometry* r = native.release();
      wrapped_geometry = GeometryWrapper::WrapNativeObject(napi_.Env(), r);
      space.Set(id, wrapped_geometry);
    }
    Geometry& result =
        Napi::ObjectWrap<GeometryWrapper>::Unwrap(wrapped_geometry)->Get();
    return result;
  }

  CGAL::Surface_mesh<EK::Point_3>& GetSurfaceMesh(const GeometryId& id) {
    Napi::Object space = Space("surface_mesh");
    Napi::Object wrapped_mesh = space.Get(id).As<Napi::Object>();
    if (wrapped_mesh == undefined_) {
      Geometry& geometry = GetGeometry(id);
      auto native = std::make_unique<CGAL::Surface_mesh<EK::Point_3>>();
      geometry.EncodeSurfaceMesh(*native);
      CGAL::Surface_mesh<EK::Point_3>* r = native.release();
      wrapped_mesh = SurfaceMeshWrapper::WrapNativeObject(napi_.Env(), r);
      space.Set(id, wrapped_mesh);
    }
    return Napi::ObjectWrap<SurfaceMeshWrapper>::Unwrap(wrapped_mesh)->Get();
  }

  CGAL::Surface_mesh<EK::Point_3>& GetFaceSurfaceMesh(const GeometryId& id) {
    Napi::Object space = Space("face_surface_mesh");
    Napi::Object wrapped_mesh = space.Get(id).As<Napi::Object>();
    if (wrapped_mesh == undefined_) {
      Geometry& geometry = GetGeometry(id);
      auto native = std::make_unique<CGAL::Surface_mesh<EK::Point_3>>();
      geometry.EncodeFaceSurfaceMesh(*native);
      CGAL::Surface_mesh<EK::Point_3>* r = native.release();
      wrapped_mesh = SurfaceMeshWrapper::WrapNativeObject(napi_.Env(), r);
      space.Set(id, wrapped_mesh);
    }
    return Napi::ObjectWrap<SurfaceMeshWrapper>::Unwrap(wrapped_mesh)->Get();
  }

 public:
  const Napi::Object napi_;
  const Napi::Value undefined_;
};
