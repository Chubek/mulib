#pragma once
#include "MuDSL.hpp"
namespace mull::stdlib {
inline void installList(Runtime &runtime) {
  Value::Object lib;
  lib["Range"] = Value(Value::Native([](const auto &a) {
    arity(a,2); auto start = a[0].number(), end = a[1].number();
    if (start != std::floor(start) || end != std::floor(end) || std::fabs(start) > 9007199254740991.0 ||
        std::fabs(end) > 9007199254740991.0 || end < start || end-start > 1000000) throw Error("invalid range");
    Value::List out; for (auto i = start; i < end; ++i) out.emplace_back(i); return Value(out);
  }));
  lib["Map"] = Value(Value::Native([](const auto &a) {
    arity(a,2); Value::List out; for (const auto &v : a[0].list()) out.push_back(a[1].call({v})); return Value(out);
  }));
  lib["Filter"] = Value(Value::Native([](const auto &a) {
    arity(a,2); Value::List out; for (const auto &v : a[0].list()) if (a[1].call({v}).truth()) out.push_back(v); return Value(out);
  }));
  lib["Fold"] = Value(Value::Native([](const auto &a) {
    arity(a,3); auto result = a[1]; for (const auto &v : a[0].list()) result = a[2].call({result,v}); return result;
  }));
  lib["Push"] = Value(Value::Native([](const auto &a) {
    arity(a,2); auto out = a[0].list(); out.push_back(a[1]); return Value(out);
  }));
  lib["Set"] = Value(Value::Native([](const auto &a) {
    arity(a,3); auto out = a[0].list(); auto i = a[1].number();
    if (i < 0 || i != std::floor(i) || i >= double(out.size())) throw Error("index out of bounds");
    out[static_cast<std::size_t>(i)] = a[2]; return Value(out);
  }));
  runtime.registerLibrary("list", Value(lib));
}
}
