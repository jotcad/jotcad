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
#include "clean.h"
#include "clip.h"
#include "clip_open.h"
#include "close.h"
#include "curve.h"
#include "cut.h"
#include "cut_open.h"
#include "edges.h"
#include "extrude.h"
#include "fill.h"
#include "footprint.h"
#include "ft.h"
#include "geometry.h"
#include "grow.h"
#include "hull.h"
#include "join.h"
#include "link.h"
#include "make_absolute.h"
#include "orb.h"
#include "relief.h"
#include "rule.h"
#include "shell.h"
#include "simplify.h"
#include "smooth.h"
#include "surface_mesh.h"
#include "sweep.h"
#include "test.h"
#include "transform.h"
#include "triangulate.h"
#include "wrap.h"  // Added for Wrap3 function

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

static Napi::Value Close3Binding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 2);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  return geometry::Close3(assets, shape);
}

static Napi::Value CurveBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 4);
  Assets assets(info[0].As<Napi::Object>());
  Napi::Array jsPoints(info[1].As<Napi::Array>());
  bool closed = info[2].As<Napi::Boolean>().Value();
  double resolution = info[3].As<Napi::Number>().DoubleValue();

  std::vector<Shape> points;
  for (uint32_t nth = 0; nth < jsPoints.Length(); nth++) {
    points.emplace_back(jsPoints.Get(nth).As<Napi::Object>());
  }
  try {
    return geometry::Curve<EK>(assets, points, closed, resolution);
  } catch (const std::exception& e) {
    Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
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

static Napi::Value ClipOpenBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsTools(info[2].As<Napi::Array>());
  std::vector<Shape> tools;
  for (uint32_t nth = 0; nth < jsTools.Length(); nth++) {
    tools.emplace_back(jsTools.Get(nth).As<Napi::Object>());
  }
  return ClipOpen(assets, shape, tools);
}

static Napi::Value CleanBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 7);

  Assets assets(info[0].As<Napi::Object>());

  Shape shape(info[1].As<Napi::Object>());

  double angle_threshold = info[2].As<Napi::Number>().DoubleValue();

  bool use_angle_constrained = info[3].As<Napi::Boolean>().Value();

  bool regularize = info[4].As<Napi::Boolean>().Value();

  bool collapse = info[5].As<Napi::Boolean>().Value();

  double plane_distance_threshold = info[6].As<Napi::Number>().DoubleValue();

  return geometry::Clean(assets, shape, angle_threshold, use_angle_constrained,
                         regularize, collapse, plane_distance_threshold);
}
static Napi::Value Cut2Binding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsTools(info[2].As<Napi::Array>());
  std::vector<Shape> tools;
  for (uint32_t nth = 0; nth < jsTools.Length(); nth++) {
    tools.emplace_back(jsTools.Get(nth).As<Napi::Object>());
  }
  return Cut2(assets, shape, tools);
}

static Napi::Value Cut3Binding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsTools(info[2].As<Napi::Array>());
  std::vector<Shape> tools;
  for (uint32_t nth = 0; nth < jsTools.Length(); nth++) {
    tools.emplace_back(jsTools.Get(nth).As<Napi::Object>());
  }
  return Cut3(assets, shape, tools);
}

static Napi::Value CutOpenBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsTools(info[2].As<Napi::Array>());
  std::vector<Shape> tools;
  for (uint32_t nth = 0; nth < jsTools.Length(); nth++) {
    tools.emplace_back(jsTools.Get(nth).As<Napi::Object>());
  }
  return CutOpen(assets, shape, tools);
}

static Napi::Value ExtractEdgesBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  double angle_threshold = info[2].As<Napi::Number>().DoubleValue();

  return geometry::ExtractEdges(assets, shape, angle_threshold);
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

static Napi::Value HullBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 2);
  Assets assets(info[0].As<Napi::Object>());
  Napi::Array jsShapes(info[1].As<Napi::Array>());
  std::vector<Shape> shapes;
  for (uint32_t nth = 0; nth < jsShapes.Length(); nth++) {
    shapes.emplace_back(jsShapes.Get(nth).As<Napi::Object>());
  }
  return geometry::Hull(assets, shapes);
}

static Napi::Value JoinBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 4);  // Now expects 4 arguments
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsTools(info[2].As<Napi::Array>());
  std::vector<Shape> tools;
  for (uint32_t nth = 0; nth < jsTools.Length(); nth++) {
    tools.emplace_back(jsTools.Get(nth).As<Napi::Object>());
  }
  double envelope_size =
      info[3].As<Napi::Number>().DoubleValue();      // Extract envelope_size
  return Join(assets, shape, tools, envelope_size);  // Pass envelope_size
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

static Napi::Value ShellBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 9);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  double inner_offset = info[2].As<Napi::Number>().DoubleValue();
  double outer_offset = info[3].As<Napi::Number>().DoubleValue();
  bool protect = info[4].As<Napi::Boolean>().Value();
  double angle = info[5].As<Napi::Number>().DoubleValue();
  double sizing = info[6].As<Napi::Number>().DoubleValue();
  double approx = info[7].As<Napi::Number>().DoubleValue();
  double edge_size = info[8].As<Napi::Number>().DoubleValue();

  return geometry::Shell(assets, shape, inner_offset, outer_offset, protect,
                         angle, sizing, approx, edge_size);
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

static Napi::Value SmoothBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 9);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Array jsPolylines(info[2].As<Napi::Array>());
  double radius = info[3].As<Napi::Number>().DoubleValue();
  double angle_threshold = info[4].As<Napi::Number>().DoubleValue();
  double resolution = info[5].As<Napi::Number>().DoubleValue();
  bool skip_fairing = info[6].As<Napi::Boolean>().Value();
  bool skip_refine = info[7].As<Napi::Boolean>().Value();
  int fairing_continuity = info[8].As<Napi::Number>().Int32Value();

  std::vector<Shape> polylines;
  for (uint32_t nth = 0; nth < jsPolylines.Length(); nth++) {
    polylines.emplace_back(jsPolylines.Get(nth).As<Napi::Object>());
  }

  return geometry::Smooth(assets, shape, polylines, radius, angle_threshold,
                          resolution, skip_fairing, skip_refine,
                          fairing_continuity);
}

static Napi::Value SweepBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 10);

  Assets assets(info[0].As<Napi::Object>());
  Shape target(info[1].As<Napi::Object>());
  Napi::Array jsProfile(info[2].As<Napi::Array>());
  Shape path(info[3].As<Napi::Object>());

  bool closed_path = info[4].As<Napi::Boolean>().Value();
  bool closed_profile = info[5].As<Napi::Boolean>().Value();

  int strategy = info[6].As<Napi::Number>().Int32Value();
  bool solid = info[7].As<Napi::Boolean>().Value();
  double iota = info[8].As<Napi::Number>().DoubleValue();
  double min_turn_radius = info[9].As<Napi::Number>().DoubleValue();

  std::vector<Shape> profile;
  for (uint32_t nth = 0; nth < jsProfile.Length(); nth++) {
    profile.emplace_back(jsProfile.Get(nth).As<Napi::Object>());
  }

  try {
    std::vector<std::string> error_tokens;

    GeometryId id = geometry::Sweep<EK>(assets, profile, path, closed_path,

                                        closed_profile, strategy, solid, iota,
                                        min_turn_radius, &error_tokens);

    target.napi_.Set("geometry", id);

    if (!error_tokens.empty()) {
      Napi::Array js_errors = Napi::Array::New(info.Env(), error_tokens.size());

      for (size_t i = 0; i < error_tokens.size(); ++i) {
        js_errors.Set(i, Napi::String::New(info.Env(), error_tokens[i]));
      }

      target.SetTag("invalid", js_errors);

      std::cout << "wasm.cc: Added 'invalid' tag with " << error_tokens.size()
                << " tokens." << std::endl;
    }

    return target.napi_;

  } catch (const std::exception& e) {
    Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
}

static Napi::Value ReliefBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 10);

  Assets assets(info[0].As<Napi::Object>());

  Napi::Array jsPoints(info[1].As<Napi::Array>());

  int rows = info[2].As<Napi::Number>().Int32Value();

  int cols = info[3].As<Napi::Number>().Int32Value();
  std::string mapping = info[4].As<Napi::String>().Utf8Value();
  double subdivisions = info[5].As<Napi::Number>().DoubleValue();
  bool closed_u = info[6].As<Napi::Boolean>().Value();
  bool closed_v = info[7].As<Napi::Boolean>().Value();
  double target_edge_length = info[8].As<Napi::Number>().DoubleValue();
  Shape target(info[9].As<Napi::Object>());

  std::vector<Shape> points;
  for (uint32_t nth = 0; nth < jsPoints.Length(); nth++) {
    points.emplace_back(jsPoints.Get(nth).As<Napi::Object>());
  }

  try {
    std::vector<std::string> error_tokens;
    GeometryId id = geometry::Relief<EK>(assets, points, rows, cols, mapping,
                                         subdivisions, closed_u, closed_v,
                                         target_edge_length, &error_tokens);
    target.napi_.Set("geometry", id);

    if (!error_tokens.empty()) {
      Napi::Array js_errors = Napi::Array::New(info.Env(), error_tokens.size());
      for (size_t i = 0; i < error_tokens.size(); ++i) {
        js_errors.Set(i, Napi::String::New(info.Env(), error_tokens[i]));
      }
      target.SetTag("invalid", js_errors);
    }

    return target.napi_;
  } catch (const std::exception& e) {
    Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    return info.Env().Undefined();
  }
}

static Napi::Value TestBinding(const Napi::CallbackInfo& info) {
  AssertArgCount(info, 3);
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());
  Napi::Object jsOptions = info[2].As<Napi::Object>();

  bool checkBoundAVolume = false;
  if (jsOptions.Has("doesBoundAVolume")) {
    checkBoundAVolume =
        jsOptions.Get("doesBoundAVolume").As<Napi::Boolean>().Value();
  }
  bool checkNotSelfIntersect = false;
  if (jsOptions.Has("doesNotSelfIntersect")) {
    checkNotSelfIntersect =
        jsOptions.Get("doesNotSelfIntersect").As<Napi::Boolean>().Value();
  }
  bool checkIsClosed = false;
  if (jsOptions.Has("isClosed")) {
    checkIsClosed = jsOptions.Get("isClosed").As<Napi::Boolean>().Value();
  }

  return Napi::Boolean::New(
      info.Env(), Test(assets, shape, checkBoundAVolume, checkNotSelfIntersect,
                       checkIsClosed));
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
  AssertArgCount(info, 3);  // Now expects assets, shapes_array, options
  Assets assets(info[0].As<Napi::Object>());
  Napi::Array js_shapes(info[1].As<Napi::Array>());  // Extract array of shapes
  Napi::Object js_options = info[2].As<Napi::Object>();

  std::vector<Shape> shapes;
  shapes.reserve(js_shapes.Length());
  for (uint32_t nth = 0; nth < js_shapes.Length(); nth++) {
    shapes.emplace_back(js_shapes.Get(nth).As<Napi::Object>());
  }

  std::optional<unsigned int> seed;
  if (js_options.Has("seed")) {
    seed = js_options.Get("seed").As<Napi::Number>().Uint32Value();
  }

  uint32_t stopping_rule_max_iterations = 20;  // Default value
  if (js_options.Has("stoppingRuleMaxIterations")) {
    stopping_rule_max_iterations = js_options.Get("stoppingRuleMaxIterations")
                                       .As<Napi::Number>()
                                       .Uint32Value();
  }

  uint32_t stopping_rule_iters_without_improvement = 10;  // Default value
  if (js_options.Has("stoppingRuleItersWithoutImprovement")) {
    stopping_rule_iters_without_improvement =
        js_options.Get("stoppingRuleItersWithoutImprovement")
            .As<Napi::Number>()
            .Uint32Value();
  }

  // Call the overloaded C++ Rule function
  GeometryId mesh_id =
      geometry::Rule(assets, shapes, seed, stopping_rule_max_iterations,
                     stopping_rule_iters_without_improvement);
  return mesh_id;
}

static Napi::Value Wrap3Binding(const Napi::CallbackInfo& info) {
  AssertArgCount(
      info, 4);  // Expect exactly 4 arguments: assets, shape, alpha, offset
  Assets assets(info[0].As<Napi::Object>());
  Shape shape(info[1].As<Napi::Object>());

  double alpha = info[2].As<Napi::Number>().DoubleValue();
  double offset = info[3].As<Napi::Number>().DoubleValue();

  return Wrap3(assets, shape, alpha, offset);
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
  exports.Set(Napi::String::New(env, "ClipOpen"),
              Napi::Function::New(env, ClipOpenBinding));
  exports.Set(Napi::String::New(env, "Clean"),
              Napi::Function::New(env, CleanBinding));
  exports.Set(Napi::String::New(env, "Curve"),
              Napi::Function::New(env, CurveBinding));
  exports.Set(Napi::String::New(env, "ExtractEdges"),
              Napi::Function::New(env, ExtractEdgesBinding));
  exports.Set(Napi::String::New(env, "Close3"),
              Napi::Function::New(env, Close3Binding));
  exports.Set(Napi::String::New(env, "Cut2"),
              Napi::Function::New(env, Cut2Binding));
  exports.Set(Napi::String::New(env, "Cut3"),
              Napi::Function::New(env, Cut3Binding));
  exports.Set(Napi::String::New(env, "CutOpen"),
              Napi::Function::New(env, CutOpenBinding));
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
  exports.Set(Napi::String::New(env, "Hull"),
              Napi::Function::New(env, HullBinding));
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
  exports.Set(Napi::String::New(env, "Shell"),
              Napi::Function::New(env, ShellBinding));
  exports.Set(Napi::String::New(env, "Smooth"),
              Napi::Function::New(env, SmoothBinding));
  exports.Set(Napi::String::New(env, "Sweep"),
              Napi::Function::New(env, SweepBinding));
  exports.Set(Napi::String::New(env, "Relief"),
              Napi::Function::New(env, ReliefBinding));
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
  exports.Set(Napi::String::New(env, "Wrap3"),  // Register Wrap3
              Napi::Function::New(env, Wrap3Binding));

  exports.Set(Napi::String::New(env, "World"), Napi::Function::New(env, World));
  exports.Set(Napi::String::New(env, "ComputeTextHash"),
              Napi::Function::New(env, ComputeTextHashBinding));
  return exports;
}

NODE_API_MODULE(jot_cgal_addon, Init);
}  // namespace jot_cgal
