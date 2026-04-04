#pragma once

Napi::Array GetPoints(Napi::Object geometry) {
  return geometry.Get("points").As<Napi::Array>();
}
