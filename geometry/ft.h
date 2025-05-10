#pragma once

#include <cmath>
#include <optional>

#include "geometry.h"

static EK::FT DecodeFT(Napi::Value value) {
  if (value.IsNumber()) {
    EK::FT ft = value.As<Napi::Number>().DoubleValue();
    return ft;
  } else if (value.IsString()) {
    std::string text = value.As<Napi::String>().Utf8Value();
    std::istringstream ss(text);
    EK::FT ft;
    ss >> ft;
    return ft;
  }
}
