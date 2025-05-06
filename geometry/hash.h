#include <algorithm>  // For std::sort
#include <set>        // Needed if implementing cycle detection
#include <stdexcept>  // For std::runtime_error
#include <string>
#include <vector>

#include "picosha2.h"  // Include the header-only library. Make sure this file is accessible.

void AppendValueData(Napi::Value value,
                     std::vector<unsigned char>& data_accumulator);

void AppendData(std::vector<unsigned char>& accumulator, const void* data,
                size_t len) {
  const unsigned char* byte_data = static_cast<const unsigned char*>(data);
  accumulator.insert(accumulator.end(), byte_data, byte_data + len);
}

void AppendData(std::vector<unsigned char>& accumulator,
                const std::string& str) {
  AppendData(accumulator, str.data(), str.length());
}

void AppendValueData(Napi::Value value,
                     std::vector<unsigned char>& accumulator) {
  if (value.IsUndefined()) {
    AppendData(accumulator, "U:undefined");
  } else if (value.IsNull()) {
    AppendData(accumulator, "N:null");
  } else if (value.IsBoolean()) {
    AppendData(accumulator,
               value.As<Napi::Boolean>().Value() ? "B:true" : "B:false");
  } else if (value.IsNumber()) {
    AppendData(accumulator, "Nb:");
    AppendData(accumulator, value.ToString().Utf8Value());
  } else if (value.IsString()) {
    AppendData(accumulator, "S:");
    AppendData(accumulator, value.As<Napi::String>().Utf8Value());
  } else if (value.IsSymbol()) {
    AppendData(accumulator, "Sy:");
    AppendData(accumulator, value.ToString().Utf8Value());
  } else if (value.IsArray()) {
    Napi::Array arr = value.As<Napi::Array>();
    uint32_t len = arr.Length();
    AppendData(accumulator, "A:[");
    for (uint32_t i = 0; i < len; ++i) {
      AppendData(accumulator, "I:" + std::to_string(i) + ":");
      Napi::Value element = arr.Get(i);
      AppendValueData(element, accumulator);  // Recurse
    }
    AppendData(accumulator, "]A");
  } else if (value.IsObject()) {
    Napi::Object obj = value.As<Napi::Object>();
    Napi::Array keys = obj.GetPropertyNames();
    uint32_t len = keys.Length();
    std::vector<std::string> keyStrings;
    keyStrings.reserve(len);
    for (uint32_t i = 0; i < len; ++i) {
      Napi::Value keyVal = keys.Get(i);
      if (keyVal.IsString()) {
        keyStrings.push_back(keyVal.As<Napi::String>().Utf8Value());
      }
    }
    std::sort(keyStrings.begin(), keyStrings.end());  // Sort keys

    AppendData(accumulator, "O:{");
    for (const std::string& key : keyStrings) {
      AppendData(accumulator, "K:");
      AppendData(accumulator, key);
      AppendData(accumulator, "V:");
      Napi::Value propVal = obj.Get(key);
      AppendValueData(propVal, accumulator);  // Recurse
    }
    AppendData(accumulator, "}O");
  } else {
    Napi::Env env = value.Env();
    throw Napi::TypeError::New(
        env,
        "Unsupported value type encountered during hashing (expected "
        "primitive, object, or array).");
  }
}

std::string ComputeTextHash(const std::string& text) {
  return picosha2::hash256_hex_string(text);
}

std::string ComputeGeometryHash(Napi::Value valueToHash) {
  std::vector<unsigned char> accumulated_data;
  try {
    AppendValueData(valueToHash, accumulated_data);
  } catch (const Napi::Error& e) {
    throw;
  } catch (const std::exception& e) {
    throw Napi::Error::New(
        valueToHash.Env(),
        "Internal C++ exception during hashing: " + std::string(e.what()));
  } catch (...) {
    throw Napi::Error::New(valueToHash.Env(),
                           "Unknown internal C++ exception during hashing.");
  }
  std::string hex_hash_str = picosha2::hash256_hex_string(accumulated_data);
  return hex_hash_str;
}
