// #define CGAL_SURFACE_MESH_APPROXIMATION_DEBUG

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Simple_cartesian.h>
#include <napi.h>
#include <stddef.h>

std::string NapiToJson(Napi::Value value) {
  auto json = value.Env().Global().Get("JSON").As<Napi::Object>();
  auto stringify = json.Get("stringify").As<Napi::Function>();
  return stringify.Call(json, {value}).As<Napi::String>().Utf8Value();
}

static void AssertArgCount(const Napi::CallbackInfo& info, uint32_t count) {
  if (info.Length() != count) {
    Napi::TypeError::New(info.Env(), "Wrong number of arguments")
        .ThrowAsJavaScriptException();
  }
}

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Exact_predicates_inexact_constructions_kernel IK;
typedef CGAL::Simple_cartesian<double> CK;
typedef CGAL::Aff_transformation_3<EK> Tf;

#include "arc_slice.h"
#include "clip.h"
#include "cut.h"
#include "extrude.h"
#include "fill.h"
#include "footprint.h"
#include "ft.h"
#include "geometry.h"
#include "grow.h"
#include "join.h"
#include "link.h"
#include "make_absolute.h"
#include "orb.h"
#include "rule.h"
#include "simplify.h"
#include "surface_mesh.h"
#include "test.h"
#include "transform.h"
#include "triangulate.h"
#include "footprint.h"

namespace jot_cgal {

using namespace Napi;

static Napi::Value ArcSlice2Binding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 4);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  EK::FT start = DecodeFT(info[2].As<Napi::Value>());
  EK::FT end = DecodeFT(info[3].As<Napi::Value>());
  return ArcSlice2(assets, shape, start, end);
}

static Napi::Value ComputeTextHashBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 1);
  Napi::String text = info[0].As<Napi::String>();
  return Napi::String::New(info.Env(), ComputeTextHash(text.Utf8Value()));
}

static Napi::Value ClipBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsTools(info[2].As<Napi::Array>());
  std::vector<Shape> tools;
  for (uint32_t nth = 0; nth < jsTools.Length(); nth++) {
    tools.emplace_back(jsTools.Get(nth).As<Napi::Object>());
  }
  return Clip(assets, shape, tools);
}

static Napi::Value CutBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsTools(info[2].As<Napi::Array>());
  std::vector<Shape> tools;
  for (uint32_t nth = 0; nth < jsTools.Length(); nth++) {
    tools.emplace_back(jsTools.Get(nth).As<Napi::Object>());
  }
  return Cut(assets, shape, tools);
}

static Napi::Value Extrude2Binding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 4);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Shape top(info[2].As<Napi::Object>());
  Shape bottom(info[3].As<Napi::Object>());
  return Extrude2(assets, shape, top, bottom);
}

static Napi::Value Extrude3Binding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 4);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Shape top(info[2].As<Napi::Object>());
  Shape bottom(info[3].As<Napi::Object>());
  return Extrude3(assets, shape, top, bottom);
}

static Napi::Value Fill2Binding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Napi::Array jsShapes = info[1].As<Napi::Array>();
  bool holes = info[2].As<Napi::Boolean>().Value();
  std::vector<Shape> shapes;
  for (uint32_t nth = 0; nth < jsShapes.Length(); nth++) {
    shapes.emplace_back(jsShapes.Get(nth).As<Napi::Object>());
  }
  return Fill2(assets, shapes, holes);
}

static Napi::Value Fill3Binding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 2);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  return Fill3(assets, shape);
}

static Napi::Value GrowBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsTools(info[2].As<Napi::Array>());
  std::vector<Shape> tools;
  for (uint32_t nth = 0; nth < jsTools.Length(); nth++) {
    tools.emplace_back(jsTools.Get(nth).As<Napi::Object>());
  }
  return Grow(assets, shape, tools);
}

static Napi::Value JoinBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsTools(info[2].As<Napi::Array>());
  std::vector<Shape> tools;
  for (uint32_t nth = 0; nth < jsTools.Length(); nth++) {
    tools.emplace_back(jsTools.Get(nth).As<Napi::Object>());
  }
  return Join(assets, shape, tools);
}

static Napi::Value LinkBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 4);
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
  AssertArgCount(info, 2);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  auto result = MakeAbsolute(assets, shape);
  return result;
}

static Napi::Value SimplifyBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  uint32_t face_count = info[2].As<Napi::Number>().Uint32Value();
  return Simplify(assets, shape, face_count);
}

static Napi::Value SimplifyTransformBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 1);
  Napi::Env env = info.Env();
  Tf tf = DecodeTf(info[0].As<Napi::Value>());
  return EncodeTf(tf, env);
}

static Napi::Value TestBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  bool si = info[2].As<Napi::Boolean>().Value();
  return Napi::Boolean::New(info.Env(), Test(assets, shape, si));
}

static Napi::Value TextIdBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 2);
  Assets assets(info[0].As<Napi::Object>());
  Napi::Object obj = info[1].As<Napi::Object>();
  GeometryWrapper* wrapper = Napi::ObjectWrap<GeometryWrapper>::Unwrap(obj);
  return assets.TextId(wrapper->Get());
}

static Napi::Value TriangulateBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 2);
  Napi::Env env = info.Env();
  Assets assets(info[0].As<Napi::Object>());
  GeometryId id = info[1].As<Napi::Value>();
  return Triangulate(assets, id);
}

static Napi::Value FootprintBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 2);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  return geometry::footprint(assets, shape);
}

static Napi::Value MakeOrbBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 4);
  Assets assets(info[0].As<Napi::Object>());
  double angular_bound = info[1].As<Napi::Number>().DoubleValue();
  double radius_bound = info[2].As<Napi::Number>().DoubleValue();
  double distance_bound = info[3].As<Napi::Number>().DoubleValue();

  // Directly call the updated MakeOrb function and return its GeometryId
  return MakeOrb(assets, angular_bound, radius_bound, distance_bound);
}

static Napi::Value RuleBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 4);
  Assets assets(info[0].As<Napi::Object>());
  Shape from_shape(info[1].As<Napi::Object>());
  Shape to_shape(info[2].As<Napi::Object>());
  Napi::Object js_options = info[3].As<Napi::Object>();
  std::optional<unsigned int> seed;
  if (js_options.Has("seed")) {
    seed = js_options.Get("seed").As<Napi::Number>().Uint32Value();
  }

  uint32_t stopping_rule_max_iterations = 200;  // Default value
  if (js_options.Has("stoppingRuleMaxIterations")) {
    stopping_rule_max_iterations = js_options.Get("stoppingRuleMaxIterations")
                                       .As<Napi::Number>()
                                       .Uint32Value();
  }

  uint32_t stopping_rule_iters_without_improvement = 10000;  // Default value
  if (js_options.Has("stoppingRuleItersWithoutImprovement")) {
    stopping_rule_iters_without_improvement =
        js_options.Get("stoppingRuleItersWithoutImprovement")
            .As<Napi::Number>()
            .Uint32Value();
  }

  GeometryId mesh_id = geometry::Rule(assets, from_shape, to_shape, seed,
                                      stopping_rule_max_iterations,
                                      stopping_rule_iters_without_improvement);
  return mesh_id;
}

Napi::String World(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 0);
  Napi::Env env = info.Env();
  return Napi::String::New(env, "world");
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  GeometryWrapper::Init(env, exports);
  SurfaceMeshWrapper::Init(env, exports);
  exports.Set(Napi::String::New(env, "ArcSlice2"),
              Napi::Function::New(env, ArcSlice2Binding));
  exports.Set(Napi::String::New(env, "Clip"),
              Napi::Function::New(env, ClipBinding));
  exports.Set(Napi::String::New(env, "Cut"),
              Napi::Function::New(env, CutBinding));
  exports.Set(Napi::String::New(env, "Extrude2"),
              Napi::Function::New(env, Extrude2Binding));
  exports.Set(Napi::String::New(env, "Extrude3"),
              Napi::Function::New(env, Extrude3Binding));
  exports.Set(Napi::String::New(env, "Fill2"),
              Napi::Function::New(env, Fill2Binding));
  exports.Set(Napi::String::New(env, "Fill3"),
              Napi::Function::New(env, Fill3Binding));
  exports.Set(Napi::String::New(env, "Grow"),
              Napi::Function::New(env, GrowBinding));
  exports.Set(Napi::String::New(env, "Join"),
              Napi::Function::New(env, JoinBinding));
  exports.Set(Napi::String::New(env, "Link"),
              Napi::Function::New(env, LinkBinding));
  exports.Set(Napi::String::New(env, "MakeAbsolute"),
              Napi::Function::New(env, MakeAbsoluteBinding));
  exports.Set(Napi::String::New(env, "Simplify"),
              Napi::Function::New(env, SimplifyBinding));
  exports.Set(Napi::String::New(env, "SimplifyTransform"),
              Napi::Function::New(env, SimplifyTransformBinding));
  exports.Set(Napi::String::New(env, "Test"),
              Napi::Function::New(env, TestBinding));
  exports.Set(Napi::String::New(env, "TextId"),
              Napi::Function::New(env, TextIdBinding));
  exports.Set(Napi::String::New(env, "Triangulate"),
              Napi::Function::New(env, TriangulateBinding));
  exports.Set(Napi::String::New(env, "footprint"),
              Napi::Function::New(env, FootprintBinding));
  exports.Set(Napi::String::New(env, "MakeOrb"),
              Napi::Function::New(env, MakeOrbBinding));
  exports.Set(Napi::String::New(env, "Rule"),
              Napi::Function::New(env, RuleBinding));

  exports.Set(Napi::String::New(env, "World"), Napi::Function::New(env, World));
  exports.Set(Napi::String::New(env, "ComputeTextHash"),
              Napi::Function::New(env, ComputeTextHashBinding));
  return exports;
}

NODE_API_MODULE(jot_cgal_addon, Init);
}  // namespace jot_cgal
