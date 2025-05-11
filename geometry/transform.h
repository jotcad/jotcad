#pragma once

#include <CGAL/Aff_transformation_3.h>
#include <CGAL/rational_rotation.h>

#include <cmath>
#include <optional>

#include "geometry.h"

template <typename Kernel>
static void ComputeTurn(const typename Kernel::FT& tau,
                        typename Kernel::RT& sin_alpha,
                        typename Kernel::RT& cos_alpha,
                        typename Kernel::RT& w) {
  // Convert tau to radians.
  double radians = CGAL::to_double(tau) * 2 * CGAL_PI;
  CGAL::rational_rotation_approximation(radians, sin_alpha, cos_alpha, w,
                                        typename Kernel::RT(1),
                                        typename Kernel::RT(1000));
}

template <typename Kernel>
static Tf ComputeXTurnTf(const typename Kernel::FT& tau) {
  typename Kernel::RT sin_alpha, cos_alpha, w;
  ComputeTurn<Kernel>(tau, sin_alpha, cos_alpha, w);
  return Tf(w, 0, 0, 0, 0, cos_alpha, -sin_alpha, 0, 0, sin_alpha, cos_alpha, 0,
            w);
}

template <typename Kernel>
static Tf ComputeYTurnTf(const typename Kernel::FT& tau) {
  typename Kernel::RT sin_alpha, cos_alpha, w;
  ComputeTurn<Kernel>(tau, sin_alpha, cos_alpha, w);
  return Tf(cos_alpha, 0, -sin_alpha, 0, 0, w, 0, 0, sin_alpha, 0, cos_alpha, 0,
            w);
}

template <typename Kernel>
static Tf ComputeZTurnTf(const typename Kernel::FT& tau) {
  typename Kernel::RT sin_alpha, cos_alpha, w;
  ComputeTurn<Kernel>(tau, sin_alpha, cos_alpha, w);
  return Tf(cos_alpha, sin_alpha, 0, 0, -sin_alpha, cos_alpha, 0, 0, 0, 0, w, 0,
            w);
}

template <typename Kernel>
CGAL::Aff_transformation_3<Kernel> ComputeScaleTf(
    const typename Kernel::FT& x_scale, const typename Kernel::FT& y_scale,
    const typename Kernel::FT& z_scale) {
  using FT = typename Kernel::FT;
  FT zero = FT(0);
  FT one = FT(1);
  CGAL::Aff_transformation_3<Kernel> transform(x_scale, zero, zero, zero,
                                               zero, y_scale, zero, zero,
                                               zero, zero, z_scale, zero,
                                               one);
  return transform;
}

static Tf DecodeTf(Napi::Value value) {
  if (value.IsUndefined()) {
    return Tf(CGAL::IDENTITY);
  } else if (value.IsString()) {
    std::string text = value.As<Napi::String>().Utf8Value();
    std::istringstream ss(text);
    char code;
    ss >> code;
    switch (code) {
      case 't': {
        CGAL::Vector_3<EK> v;
        ss >> v;
        return Tf(CGAL::TRANSLATION, v);
      }
      case 'x': {
        EK::FT t;
        ss >> t;
        return ComputeXTurnTf<EK>(t);
      }
      case 'y': {
        EK::FT t;
        ss >> t;
        return ComputeYTurnTf<EK>(t);
      }
      case 'z': {
        EK::FT t;
        ss >> t;
        return ComputeZTurnTf<EK>(t);
      }
      case 's': {
        EK::FT x, y, z;
        ss >> x;
        ss >> y;
        ss >> z;
        return ComputeScaleTf<EK>(x, y, z);
      }
      case 'm': {
        EK::FT a, b, c, d, e, f, g, h, i, j, k, l, m;
        ss >> a;
        ss >> b;
        ss >> c;
        ss >> d;
        ss >> e;
        ss >> f;
        ss >> g;
        ss >> h;
        ss >> i;
        ss >> j;
        ss >> k;
        ss >> l;
        ss >> m;
        return Tf(a, b, c, d, e, f, g, h, i, j, k, l, m);
      }
    }
  } else if (value.IsArray()) {
    Napi::Array array = value.As<Napi::Array>();
    Napi::Value a = array.Get((uint32_t)0);
    Tf bt = DecodeTf(array.Get((uint32_t)1));
    if (a.IsString() && a.As<Napi::String>().Utf8Value() == "i") {
      auto result = bt.inverse();
      return result;
    } else {
      Tf at = DecodeTf(a);
      return at * bt;
    }
  }
  // Really should throw an error.
  return Tf(CGAL::IDENTITY);
}

template <typename Kernel>
bool IsIdentityTf(const CGAL::Aff_transformation_3<Kernel>& tf) {
  using FT = typename Kernel::FT;
  FT one = FT(1);
  FT zero = FT(0);
  return (tf.cartesian(0, 0) == one && tf.cartesian(0, 1) == zero &&
          tf.cartesian(0, 2) == zero && tf.cartesian(0, 3) == zero &&
          tf.cartesian(1, 0) == zero && tf.cartesian(1, 1) == one &&
          tf.cartesian(1, 2) == zero && tf.cartesian(1, 3) == zero &&
          tf.cartesian(2, 0) == zero && tf.cartesian(2, 1) == zero &&
          tf.cartesian(2, 2) == one && tf.cartesian(2, 3) == zero &&
          tf.cartesian(3, 0) == zero && tf.cartesian(3, 1) == zero &&
          tf.cartesian(3, 2) == zero && tf.cartesian(3, 3) == one);
}

template <typename Kernel>
std::optional<typename Kernel::FT> DecodeXTurnTf(
    const CGAL::Aff_transformation_3<Kernel>& tf) {
  using FT = typename Kernel::FT;
  FT one = FT(1);
  FT zero = FT(0);
  // 1. Check for zero translation and correct last row
  if (tf.cartesian(0, 3) != zero || tf.cartesian(1, 3) != zero ||
      tf.cartesian(2, 3) != zero || tf.cartesian(3, 0) != zero ||
      tf.cartesian(3, 1) != zero || tf.cartesian(3, 2) != zero ||
      tf.cartesian(3, 3) != one) {
    return std::nullopt;
  }
  // 2. Check for the specific structure of an x-rotation matrix and extract
  // cos/sin
  FT m00 = tf.cartesian(0, 0);
  FT m01 = tf.cartesian(0, 1);
  FT m02 = tf.cartesian(0, 2);
  FT m10 = tf.cartesian(1, 0);
  FT m11 = tf.cartesian(1, 1);
  FT m12 = tf.cartesian(1, 2);
  FT m20 = tf.cartesian(2, 0);
  FT m21 = tf.cartesian(2, 1);
  FT m22 = tf.cartesian(2, 2);
  if (m00 != one || m01 != zero || m02 != zero || m10 != zero || m20 != zero ||
      m11 != m22 || m12 != -m21) {
    return std::nullopt;
  }
  FT cos_theta = m11;
  FT sin_theta = m21;
  // 3. Calculate the angle in radians using atan2
  FT angle_radians =
      atan2(CGAL::to_double(sin_theta), CGAL::to_double(cos_theta));
  // 4. Convert radians to tau and normalize
  FT x_tau_value = angle_radians / (2 * CGAL_PI);
  if (x_tau_value < zero) {
    x_tau_value += one;
  } else if (x_tau_value >= one) {
    x_tau_value -= one;
  }
  return x_tau_value;
}

template <typename Kernel>
std::optional<typename Kernel::FT> DecodeYTurnTf(
    const CGAL::Aff_transformation_3<Kernel>& tf) {
  using FT = typename Kernel::FT;
  FT one = FT(1);
  FT zero = FT(0);

  // 1. Check for zero translation and correct last row
  if (tf.cartesian(0, 3) != zero || tf.cartesian(1, 3) != zero ||
      tf.cartesian(2, 3) != zero || tf.cartesian(3, 0) != zero ||
      tf.cartesian(3, 1) != zero || tf.cartesian(3, 2) != zero ||
      tf.cartesian(3, 3) != one) {
    return std::nullopt;
  }

  // 2. Check for the specific structure of a y-rotation matrix and extract
  // cos/sin
  FT m00 = tf.cartesian(0, 0);
  FT m01 = tf.cartesian(0, 1);
  FT m02 = tf.cartesian(0, 2);
  FT m10 = tf.cartesian(1, 0);
  FT m11 = tf.cartesian(1, 1);
  FT m12 = tf.cartesian(1, 2);
  FT m20 = tf.cartesian(2, 0);
  FT m21 = tf.cartesian(2, 1);
  FT m22 = tf.cartesian(2, 2);

  if (m01 != zero || m10 != zero || m12 != zero || m21 != zero || m11 != one ||
      m00 != m22 || m02 != -m20) {
    return std::nullopt;
  }

  FT cos_theta = m00;
  FT sin_theta = m02;

  // 3. Calculate the angle in radians using atan2
  FT angle_radians =
      atan2(CGAL::to_double(sin_theta), CGAL::to_double(cos_theta));

  // 4. Convert radians to tau and normalize
  FT y_tau_value = angle_radians / (2 * CGAL_PI);
  if (y_tau_value < zero) {
    y_tau_value += one;
  } else if (y_tau_value >= one) {
    y_tau_value -= one;
  }

  return y_tau_value;
}

template <typename Kernel>
std::optional<typename Kernel::FT> DecodeZTurnTf(
    const CGAL::Aff_transformation_3<Kernel>& tf) {
  using FT = typename Kernel::FT;
  FT one = FT(1);
  FT zero = FT(0);

  // 1. Check for zero translation and correct last row
  if (tf.cartesian(0, 3) != zero || tf.cartesian(1, 3) != zero ||
      tf.cartesian(2, 3) != zero || tf.cartesian(3, 0) != zero ||
      tf.cartesian(3, 1) != zero || tf.cartesian(3, 2) != zero ||
      tf.cartesian(3, 3) != one) {
    return std::nullopt;
  }

  // 2. Check for the specific structure of a z-rotation matrix and extract
  // cos/sin
  FT m00 = tf.cartesian(0, 0);
  FT m01 = tf.cartesian(0, 1);
  FT m02 = tf.cartesian(0, 2);
  FT m10 = tf.cartesian(1, 0);
  FT m11 = tf.cartesian(1, 1);
  FT m12 = tf.cartesian(1, 2);
  FT m20 = tf.cartesian(2, 0);
  FT m21 = tf.cartesian(2, 1);
  FT m22 = tf.cartesian(2, 2);

  if (m02 != zero || m12 != zero || m20 != zero || m21 != zero || m22 != one ||
      m00 != m11 || m01 != -m10) {
    return std::nullopt;
  }

  FT cos_theta = m00;
  FT sin_theta = m10;

  // 3. Calculate the angle in radians using atan2
  FT angle_radians =
      atan2(CGAL::to_double(sin_theta), CGAL::to_double(cos_theta));

  // 4. Convert radians to tau and normalize
  FT z_tau_value = angle_radians / (2 * CGAL_PI);
  if (z_tau_value < zero) {
    z_tau_value += one;
  } else if (z_tau_value >= one) {
    z_tau_value -= one;
  }

  return z_tau_value;
}

template <typename Kernel>
std::optional<CGAL::Vector_3<Kernel>> DecodeTranslateTf(
    const CGAL::Aff_transformation_3<Kernel>& tf) {
  using FT = typename Kernel::FT;
  FT zero = FT(0);
  FT one = FT(1);

  // Check if the linear part is the identity matrix
  if (tf.cartesian(0, 0) != one || tf.cartesian(0, 1) != zero ||
      tf.cartesian(0, 2) != zero || tf.cartesian(1, 0) != zero ||
      tf.cartesian(1, 1) != one || tf.cartesian(1, 2) != zero ||
      tf.cartesian(2, 0) != zero || tf.cartesian(2, 1) != zero ||
      tf.cartesian(2, 2) != one || tf.cartesian(3, 0) != zero ||
      tf.cartesian(3, 1) != zero || tf.cartesian(3, 2) != zero ||
      tf.cartesian(3, 3) != one) {
    return std::nullopt;
  }

  // If the linear part is identity, the translation vector is in the last
  // column
  CGAL::Vector_3<Kernel> translation(tf.cartesian(0, 3), tf.cartesian(1, 3),
                                     tf.cartesian(2, 3));
  return translation;
}

template <typename Kernel>
std::optional<CGAL::Vector_3<Kernel>> DecodeScaleTf(
    const CGAL::Aff_transformation_3<Kernel>& tf) {
  using FT = typename Kernel::FT;
  FT zero = FT(0);
  FT one = FT(1);

  // Check if the off-diagonal elements of the 3x3 linear part are zero
  if (tf.cartesian(0, 1) != zero || tf.cartesian(0, 2) != zero ||
      tf.cartesian(1, 0) != zero || tf.cartesian(1, 2) != zero ||
      tf.cartesian(2, 0) != zero || tf.cartesian(2, 1) != zero ||
      // Check if the translation components are zero
      tf.cartesian(0, 3) != zero || tf.cartesian(1, 3) != zero ||
      tf.cartesian(2, 3) != zero ||
      // Check the last row
      tf.cartesian(3, 0) != zero || tf.cartesian(3, 1) != zero ||
      tf.cartesian(3, 2) != zero || tf.cartesian(3, 3) != one) {
    return std::nullopt;
  }

  // If the off-diagonal elements are zero and translation is zero,
  // the diagonal elements are the scaling factors
  FT scale_x = tf.cartesian(0, 0);
  FT scale_y = tf.cartesian(1, 1);
  FT scale_z = tf.cartesian(2, 2);

  return CGAL::Vector_3<Kernel>(scale_x, scale_y, scale_z);
}

static Napi::Value EncodeTf(const Tf& tf, Napi::Env env) {
  std::optional<EK::FT> turn;
  std::optional<CGAL::Vector_3<EK>> vector;
  if (IsIdentityTf(tf)) {
    return env.Undefined();
  } else if (turn = DecodeXTurnTf(tf), turn) {
    std::ostringstream ss;
    ss << "x " << turn->exact() << " " << *turn;
    return Napi::String::New(env, ss.str());
  } else if (turn = DecodeYTurnTf(tf), turn) {
    std::ostringstream ss;
    ss << "y " << turn->exact() << " " << *turn;
    return Napi::String::New(env, ss.str());
  } else if (turn = DecodeZTurnTf(tf), turn) {
    std::ostringstream ss;
    ss << "z " << turn->exact() << " " << *turn;
    return Napi::String::New(env, ss.str());
  } else if (vector = DecodeTranslateTf(tf), vector) {
    std::ostringstream ss;
    ss << "t " << vector->x().exact() << " " << vector->y().exact() << " "
       << vector->z().exact() << " " << *vector;
    return Napi::String::New(env, ss.str());
  } else if (vector = DecodeScaleTf(tf), vector) {
    std::ostringstream ss;
    ss << "s " << vector->x().exact() << " " << vector->y().exact() << " "
       << vector->z().exact() << " " << *vector;
    return Napi::String::New(env, ss.str());
  } else {
    std::ostringstream ss;
    ss << "m ";
    ss << tf.cartesian(0, 0).exact() << " ";
    ss << tf.cartesian(1, 0).exact() << " ";
    ss << tf.cartesian(2, 0).exact() << " ";

    ss << tf.cartesian(0, 1).exact() << " ";
    ss << tf.cartesian(1, 1).exact() << " ";
    ss << tf.cartesian(2, 1).exact() << " ";

    ss << tf.cartesian(0, 2).exact() << " ";
    ss << tf.cartesian(1, 2).exact() << " ";
    ss << tf.cartesian(2, 2).exact() << " ";

    ss << tf.cartesian(0, 3).exact() << " ";
    ss << tf.cartesian(1, 3).exact() << " ";
    ss << tf.cartesian(2, 3).exact() << " ";

    ss << tf.cartesian(0, 0) << " ";
    ss << tf.cartesian(1, 0) << " ";
    ss << tf.cartesian(2, 0) << " ";

    ss << tf.cartesian(0, 1) << " ";
    ss << tf.cartesian(1, 1) << " ";
    ss << tf.cartesian(2, 1) << " ";

    ss << tf.cartesian(0, 2) << " ";
    ss << tf.cartesian(1, 2) << " ";
    ss << tf.cartesian(2, 2) << " ";

    ss << tf.cartesian(0, 3) << " ";
    ss << tf.cartesian(1, 3) << " ";
    ss << tf.cartesian(2, 3);

    return Napi::String::New(env, ss.str());
  }
}
