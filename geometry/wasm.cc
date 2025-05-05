#include <stddef.h>
#include <napi.h>

#include "CGAL/Exact_predicates_exact_constructions_kernel.h"

#include "arc_slice.h"
#include "fill.h"
#include "ft.h"
#include "geometry.h"
#include "link.h"
#include "make_absolute.h"
#include "surface_mesh.h"
#include "transform.h"
#include "triangulate.h"

namespace jot_cgal {

using namespace Napi;

static void assertArgCount(const Napi::CallbackInfo& info, uint32_t count) {
  if (info.Length() != count) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments")
        .ThrowAsJavaScriptException();
  }
}

static Napi::Value ArcSlice2Binding(const Napi::CallbackInfo& info) {
  assertArgCount(info, 4);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  EK::FT start = DecodeFT(info[2].As<Napi::Value>());
  EK::FT end = DecodeFT(info[3].As<Napi::Value>());
  return ArcSlice2(assets, shape, start, end);
}

static Napi::Value ComputeTextHashBinding(const Napi::CallbackInfo& info) {
  assertArgCount(info, 1);
  Napi::String text = info[0].As<Napi::String>();
  return Napi::String::New(info.Env(), ComputeTextHash(text.Utf8Value()));
}

static Napi::Value FillBinding(const Napi::CallbackInfo& info) {
  assertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Napi::Array jsShapes = info[1].As<Napi::Array>();
  bool holes = info[2].As<Napi::Boolean>().Value();
  std::vector<Shape> shapes;
  for (uint32_t nth = 0; nth < jsShapes.Length(); nth++) {
    shapes.emplace_back(jsShapes.Get(nth).As<Napi::Object>());
  }
  return Fill(assets, shapes, holes);
}

static Napi::Value LinkBinding(const Napi::CallbackInfo& info) {
  assertArgCount(info, 4);
  Assets assets(info[0].As<Napi::Object>());
  Napi::Array jsShapes = info[1].As<Napi::Array>();
  bool close = info[2].As<Napi::Boolean>().Value();
  bool reverse = info[3].As<Napi::Boolean>().Value();
  std::vector<Shape> shapes;
  for (uint32_t nth = 0; nth < jsShapes.Length(); nth++) {
    shapes.emplace_back(jsShapes.Get(nth).As<Napi::Object>());
  }
  return Link(assets, shapes, close, reverse);
}

static GeometryId MakeAbsoluteBinding(const Napi::CallbackInfo& info) {
  assertArgCount(info, 2);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  auto result = MakeAbsolute(assets, shape);
  return result;
}

static Napi::Value SimplifyTransformBinding(const Napi::CallbackInfo& info) {
  assertArgCount(info, 1);
  Napi::Env env = info.Env();
  Tf tf = DecodeTf(info[0].As<Napi::Value>());
  return EncodeTf(tf, env);
}

static Napi::Value TriangulateBinding(const Napi::CallbackInfo& info) {
  assertArgCount(info, 2);
  Napi::Env env = info.Env();
  Assets assets(info[0].As<Napi::Object>());
  GeometryId id = info[1].As<Napi::Value>();
  return Triangulate(assets, id);
}

Napi::String World(const Napi::CallbackInfo& info) {
  assertArgCount(info, 1);
  Napi::Env env = info.Env();
  return Napi::String::New(env, "world");
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  GeometryWrapper::Init(env, exports);
  SurfaceMeshWrapper::Init(env, exports);
  exports.Set(Napi::String::New(env, "ArcSlice2"), Napi::Function::New(env, ArcSlice2Binding));
  exports.Set(Napi::String::New(env, "Fill"), Napi::Function::New(env, FillBinding));
  exports.Set(Napi::String::New(env, "Link"), Napi::Function::New(env, LinkBinding));
  exports.Set(Napi::String::New(env, "MakeAbsolute"), Napi::Function::New(env, MakeAbsoluteBinding));
  exports.Set(Napi::String::New(env, "SimplifyTransform"), Napi::Function::New(env, SimplifyTransformBinding));
  exports.Set(Napi::String::New(env, "Triangulate"), Napi::Function::New(env, TriangulateBinding));
  exports.Set(Napi::String::New(env, "World"), Napi::Function::New(env, World));
  exports.Set(Napi::String::New(env, "ComputeTextHash"), Napi::Function::New(env, ComputeTextHashBinding));
  return exports;
}

NODE_API_MODULE(jot_cgal_addon, Init);
}  // namespace jot_cgal
