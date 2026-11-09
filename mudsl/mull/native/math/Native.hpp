#pragma once
#include "MuDSL.hpp"
namespace mull::stdlib {
inline void installMath(Runtime &runtime) {
  Value::Object lib;
  auto unary = [&](const char *name, double (*fn)(double)) {
    lib[name] = Value(Value::Native([fn](const auto &a) { arity(a,1); return Value(fn(a[0].number())); }));
  };
  unary("Abs", std::fabs); unary("Floor", std::floor); unary("Ceil", std::ceil); unary("Sqrt", std::sqrt);
  lib["Pow"] = Value(Value::Native([](const auto &a) { arity(a,2); return Value(std::pow(a[0].number(), a[1].number())); }));
  lib["Min"] = Value(Value::Native([](const auto &a) { arity(a,2); return Value(std::min(a[0].number(), a[1].number())); }));
  lib["Max"] = Value(Value::Native([](const auto &a) { arity(a,2); return Value(std::max(a[0].number(), a[1].number())); }));
  lib["Pi"] = Value(3.141592653589793);
  runtime.registerLibrary("math", Value(lib));
}
}
