#pragma once
#include "MuDSL.hpp"
namespace mull::stdlib {
inline void installText(Runtime &runtime) {
  Value::Object lib;
  lib["Split"] = Value(Value::Native([](const auto &a) {
    arity(a,2); const auto &s = a[0].string(), &sep = a[1].string();
    if (sep.empty()) throw Error("empty separator");
    Value::List out; std::size_t start = 0, end;
    while ((end = s.find(sep, start)) != std::string::npos) { out.emplace_back(s.substr(start, end-start)); start = end+sep.size(); }
    out.emplace_back(s.substr(start)); return Value(out);
  }));
  lib["Join"] = Value(Value::Native([](const auto &a) {
    arity(a,2); std::string out; bool first = true; const auto &sep = a[1].string();
    for (const auto &v : a[0].list()) { if (!first) out += sep; first = false; out += v.string(); } return Value(out);
  }));
  lib["Replace"] = Value(Value::Native([](const auto &a) {
    arity(a,3); auto s = a[0].string(); const auto &from = a[1].string(), &to = a[2].string();
    if (from.empty()) throw Error("empty search string");
    std::size_t p = 0; while ((p = s.find(from,p)) != std::string::npos) { s.replace(p,from.size(),to); p += to.size(); }
    return Value(s);
  }));
  for (bool upper : {false, true}) {
    lib[upper ? "Upper" : "Lower"] = Value(Value::Native([upper](const auto &a) {
      arity(a,1); auto s = a[0].string();
      for (char &c : s) { if (upper && c >= 'a' && c <= 'z') c -= 32; if (!upper && c >= 'A' && c <= 'Z') c += 32; }
      return Value(s);
    }));
  }
  runtime.registerLibrary("text", Value(lib));
}
}
