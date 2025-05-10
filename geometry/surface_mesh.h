#pragma once

#include <CGAL/Surface_mesh.h>

class SurfaceMeshWrapper : public Napi::ObjectWrap<SurfaceMeshWrapper> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::HandleScope scope(env);

    Napi::Function func =
        DefineClass(env, "SurfaceMesh", {});  // No methods exposed

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set(
        "wrapNative",
        Napi::Function::New(
            env, [](const Napi::CallbackInfo& info) -> Napi::Value {
              if (info.Length() != 1 || !info[0].IsExternal()) {
                Napi::TypeError::New(info.Env(),
                                     "Expected an external for wrapNative")
                    .ThrowAsJavaScriptException();
                return info.Env().Undefined();
              }
              CGAL::Surface_mesh<EK::Point_3>* native =
                  static_cast<CGAL::Surface_mesh<EK::Point_3>*>(
                      info[0]
                          .As<Napi::External<CGAL::Surface_mesh<EK::Point_3>>>()
                          .Data());
              return WrapNativeObject(info.Env(), native);
            }));

    return exports;
  }

  static Napi::Object WrapNativeObject(
      Napi::Env env, CGAL::Surface_mesh<EK::Point_3>* nativeObject) {
    auto external =
        Napi::External<CGAL::Surface_mesh<EK::Point_3>>::New(env, nativeObject);
    Napi::Object obj = constructor.New({external});
    SurfaceMeshWrapper* wrapper =
        Napi::ObjectWrap<SurfaceMeshWrapper>::Unwrap(obj);
    if (wrapper) {
      wrapper->_mesh = nativeObject;
    }
    return obj;
  }

  CGAL::Surface_mesh<EK::Point_3>& Get() { return *_mesh; }

  SurfaceMeshWrapper(const Napi::CallbackInfo& info)
      : Napi::ObjectWrap<SurfaceMeshWrapper>(info) {
    Napi::Env env = info.Env();
    Napi::HandleScope scope(env);
    if (info.Length() == 1 && info[0].IsExternal()) {
      _mesh = static_cast<CGAL::Surface_mesh<EK::Point_3>*>(
          info[0].As<Napi::External<CGAL::Surface_mesh<EK::Point_3>>>().Data());
    } else {
      Napi::TypeError::New(env,
                           "Internal constructor expects a single external")
          .ThrowAsJavaScriptException();
    }
  }

  ~SurfaceMeshWrapper() { delete _mesh; }

  static Napi::FunctionReference constructor;
  CGAL::Surface_mesh<EK::Point_3>* _mesh;
};

Napi::FunctionReference SurfaceMeshWrapper::constructor;
