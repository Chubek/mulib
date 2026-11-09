#pragma once
#include "MuDSL.hpp"

namespace mull::stdlib {
inline std::string format(const Value::List &args) {
  if (args.empty()) throw Error("Format requires a template");
  const auto &s = args[0].string(); std::string out; std::size_t arg = 1;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '{' || s[i] == '}') {
      if (i + 1 < s.size() && s[i+1] == s[i]) { out += s[i++]; continue; }
      if (s[i] == '{' && i + 1 < s.size() && s[i+1] == '}') {
        if (arg == args.size()) throw Error("not enough format arguments");
        out += args[arg++].text(); ++i; continue;
      }
      throw Error("invalid format template");
    }
    out += s[i];
  }
  if (arg != args.size()) throw Error("too many format arguments");
  return out;
}
inline void installPrintf(Runtime &runtime) {
  struct State { Value sink; bool writing = false; };
  auto state = runtime.retainState(std::make_shared<State>(State{runtime.get("G").member("GetStdout").call()}));
  auto write = [state](std::string text) {
    if (state->writing) throw Error("recursive output sink");
    state->writing = true;
    struct Guard { bool &flag; ~Guard() { flag = false; } } guard{state->writing};
    // Keep the selected callable alive if it replaces itself during a write.
    auto sink = state->sink; sink.call({Value(std::move(text))});
  };
  Value::Object lib;
  lib["SetMainSink"] = Value(Value::Native([state](const auto &a) {
    arity(a,1); if (a[0].type() != "function") throw Error("sink must be a function"); state->sink = a[0]; return Value{};
  }));
  lib["Format"] = Value(Value::Native([](const auto &a) { return Value(format(a)); }));
  lib["Printf"] = Value(Value::Native([write](const auto &a) { write(format(a)); return Value{}; }));
  lib["Print"] = Value(Value::Native([write](const auto &a) {
    std::string s; for (const auto &v : a) s += v.text(); write(s); return Value{};
  }));
  lib["Println"] = Value(Value::Native([write](const auto &a) {
    std::string s; for (const auto &v : a) s += v.text(); write(s + "\n"); return Value{};
  }));
  runtime.registerLibrary("printf", Value(lib));
}
}
