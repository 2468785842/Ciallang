//
// Created by LiDong on 2026/1/7.
//
#pragma once

#include "Types.hpp"
#include "common/Ret.hpp"

namespace cial::TypeConverter {
    String octetToString(const Octet &oct);
    String objectToString(const Object &obj);
    String integerToString(Integer integer);
    String realToString(Real real);
    String realToHexString(Real real);

    Ret<Integer> stringToInteger(const String &str);
    Ret<Real> stringToReal(const String &str);

} // namespace cial::TypeConverter