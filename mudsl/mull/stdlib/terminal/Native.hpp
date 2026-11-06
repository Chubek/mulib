#pragma once
#include "MuDSL.hpp"
namespace mull::stdlib {
inline void installTerminal(Runtime &runtime) {
  Value::Object lib;
  lib["Write"] = Value(Value::Native([&runtime](const auto &a) {
    arity(a,1); runtime.stdoutSink(a[0].string()); return Value{};
  }));
  lib["Clear"] = Value(Value::Native([&runtime](const auto &a) {
    arity(a,0); runtime.stdoutSink("\x1b[2J\x1b[H"); return Value{};
  }));
  lib["Color"] = Value(Value::Native([](const auto &a) {
    arity(a,2); auto n = a[0].number();
    if (n < 0 || n > 107 || n != std::floor(n)) throw Error("invalid ANSI color");
    return Value("\x1b[" + std::to_string(int(n)) + "m" + a[1].string() + "\x1b[0m");
  }));
  runtime.registerLibrary("terminal", Value(lib));
}
}
