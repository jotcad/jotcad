#include <stddef.h>
#include <napi.h>

#include "CGAL/Exact_predicates_exact_constructions_kernel.h"

#include "API.h"

namespace jot_cgal {

using namespace Napi;

static void assertArgCount(const Napi::CallbackInfo& info, uint32_t count) {
  if (info.Length() != count) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments")
        .ThrowAsJavaScriptException();
  }
}

static Napi::Value ComputeGeometryHashBinding(const Napi::CallbackInfo& info) {
  assertArgCount(info, 1);
  Napi::Object jsGeometry = info[0].As<Napi::Object>();
  return Napi::String::New(info.Env(), ComputeGeometryHash(jsGeometry));
}

static Napi::Value LinkBinding(const Napi::CallbackInfo& info) {
  assertArgCount(info, 4);
  Napi::Object jsGeometries = info[0].As<Napi::Object>();
  Napi::Object jsShape = info[1].As<Napi::Object>();
  bool close = info[2].As<Napi::Boolean>().Value();
  bool reverse = info[3].As<Napi::Boolean>().Value();
  Geometries geometries(jsGeometries);
  Shape shape(jsShape);
  auto result = Link2(geometries, shape, close, reverse);
  return result.ToNapi();
}

Napi::String World(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return Napi::String::New(env, "world");
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "Link"), Napi::Function::New(env, LinkBinding));
  exports.Set(Napi::String::New(env, "World"), Napi::Function::New(env, World));
  exports.Set(Napi::String::New(env, "ComputeGeometryHash"), Napi::Function::New(env, ComputeGeometryHashBinding));
  return exports;
}

NODE_API_MODULE(jot_cgal_addon, Init);
}  // namespace jot_cgal
