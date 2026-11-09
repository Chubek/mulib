#include "Runtime.hpp"
#include "../stdlib/Stdlib.hpp"

namespace mull::mu3ac {
static std::string string(View v) { return {v.data,static_cast<std::size_t>(v.size)}; }
struct Cell { Value value; bool mutable_ = false; };
struct Environment {
  Environment *parent;
  std::map<std::string,std::shared_ptr<Cell>> values;
  std::shared_ptr<Cell> lookup(const std::string &name) {
    for (auto e = this; e; e = e->parent) {
      auto i = e->values.find(name); if (i != e->values.end()) return i->second;
    }
    throw Error("undefined binding: " + name);
  }
};
struct Context {
  Runtime runtime;
  const Config &config;
  std::vector<Value> values{Value{}};
  std::vector<std::unique_ptr<Environment>> environments;
  std::size_t steps = 0, calls = 0, frames = 0, line = 0, column = 0;
  explicit Context(const Config &c) : runtime(std::vector<std::filesystem::path>{}), config(c) {
    runtime.maxSteps = c.maxSteps; runtime.maxCallDepth = c.maxCalls;
    runtime.maxFrames = c.maxFrames; runtime.maxSourceBytes = c.maxSource;
    runtime.mullier.optimize = c.flags & 4;
    if (c.mullierRules.size) runtime.mullier.loadDirectory(string(c.mullierRules));
    runtime.stepObserver = [this](std::size_t l, std::size_t col) {
      if (++steps > config.maxSteps) throw Error("execution limit exceeded",l,col);
    };
    runtime.frameObserver = [this] { frameBudget(); };
    runtime.callObserver = [this](bool enter) {
      if (!enter) { --calls; return; }
      if (calls >= config.maxCalls) throw Error("call depth limit exceeded");
      ++calls;
    };
    for (std::size_t i = 0; i < c.pathCount; ++i) runtime.addLibraryPath(string(c.paths[i]));
    if (c.flags & 1) stdlib::install(runtime);
  }
  Environment *frame(Environment *parent) {
    frameBudget();
    environments.push_back(std::make_unique<Environment>(Environment{parent,{}})); return environments.back().get();
  }
  void frameBudget() { if (frames >= config.maxFrames) throw Error("environment limit exceeded"); ++frames; }
  Handle keep(Value value) {
    if (values.size() >= config.maxValues) throw Error("Mu3AC value limit exceeded");
    values.push_back(std::move(value)); return values.size()-1;
  }
  const Value &get(Handle value) const { return values.at(value); }
  void bind(Environment *env, const std::string &name, Value value, std::uint64_t kind = 0) {
    if (env->values.contains(name)) throw Error("duplicate binding: " + name);
    if (kind == 2 && value.type() != "mulib") throw Error("mulib declaration requires a mulib value");
    env->values[name] = std::make_shared<Cell>(Cell{std::move(value),kind == 1});
  }
  Value::List arguments(const Handle *args, std::size_t count) const {
    Value::List result; result.reserve(count); for (std::size_t i = 0; i < count; ++i) result.push_back(get(args[i])); return result;
  }
  template<class F> auto guard(F &&f) -> decltype(f()) {
    try { return f(); }
    catch (const Error &e) { if (e.line) throw; throw Error(e.what(),line,column); }
    catch (const std::exception &e) { throw Error(e.what(),line,column); }
  }
};
static Value foreignValue(void *address, std::size_t count) {
  if (!address || count > 8) throw Error("invalid foreign function");
  return Value(Value::Native([address,count](const Value::List &a) {
    arity(a,count); double x[8]{}; for (std::size_t i = 0; i < count; ++i) x[i] = a[i].number();
    double result;
    switch (count) {
      case 0: result = reinterpret_cast<double(*)()>(address)(); break;
      case 1: result = reinterpret_cast<double(*)(double)>(address)(x[0]); break;
      case 2: result = reinterpret_cast<double(*)(double,double)>(address)(x[0],x[1]); break;
      case 3: result = reinterpret_cast<double(*)(double,double,double)>(address)(x[0],x[1],x[2]); break;
      case 4: result = reinterpret_cast<double(*)(double,double,double,double)>(address)(x[0],x[1],x[2],x[3]); break;
      case 5: result = reinterpret_cast<double(*)(double,double,double,double,double)>(address)(x[0],x[1],x[2],x[3],x[4]); break;
      case 6: result = reinterpret_cast<double(*)(double,double,double,double,double,double)>(address)(x[0],x[1],x[2],x[3],x[4],x[5]); break;
      case 7: result = reinterpret_cast<double(*)(double,double,double,double,double,double,double)>(address)(x[0],x[1],x[2],x[3],x[4],x[5],x[6]); break;
      default: result = reinterpret_cast<double(*)(double,double,double,double,double,double,double,double)>(address)(x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7]);
    }
    return Value(result);
  }));
}
int nativeMain(const Config &config, Entry entry, int argc, char **argv) {
  try {
    if (config.version != 1) throw Error("unsupported Mu3AC native ABI");
    Context context(config); auto env = context.frame(nullptr);
    context.bind(env,"G",context.runtime.get("G"));
    Value::List args; for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    context.bind(env,"Args",Value(std::move(args)));
    for (std::size_t i = 0; i < config.foreignCount; ++i) {
      const auto &f = config.foreign[i]; context.bind(env,string(f.name),foreignValue(f.address,f.arity));
    }
    for (std::size_t i = 0; i < config.preloadCount; ++i)
      if (!context.runtime.loadLibrary(string(config.preloads[i])).truth()) throw Error(context.runtime.lastError());
    auto result = context.get(entry(&context,env));
    if (config.flags & 2) std::cout << result.text() << '\n';
    return 0;
  } catch (const Error &e) {
    std::cerr << string(config.source) << ':' << e.line << ':' << e.column << ": " << e.what() << '\n'; return 1;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
} // namespace mull::mu3ac

using namespace mull;
using namespace mull::mu3ac;
extern "C" {
void mu3ac_site(Context *c, std::uint64_t line, std::uint64_t column) { c->line = line; c->column = column; }
void mu3ac_tick(Context *c, std::uint64_t line, std::uint64_t column) {
  mu3ac_site(c,line,column); c->guard([&] { if (++c->steps > c->config.maxSteps) throw Error("execution limit exceeded"); });
}
Environment *mu3ac_enter(Context *c, Environment *e) { return c->guard([&] { return c->frame(e); }); }
Environment *mu3ac_leave(Context *, Environment *e) { return e->parent; }
Handle mu3ac_number(Context *c, const char *s, std::uint64_t n) {
  return c->guard([&] {
    double value; auto r = std::from_chars(s,s+n,value);
    if (r.ec != std::errc{} || r.ptr != s+n) throw Error("invalid number"); return c->keep(Value(value));
  });
}
Handle mu3ac_string(Context *c, const char *s, std::uint64_t n) {
  return c->guard([&] {
    std::string value(s,n); std::size_t p = 0;
    while ((p = value.find("$E?",p)) != std::string::npos) { value.replace(p,3,c->runtime.lastError()); p += c->runtime.lastError().size(); }
    return c->keep(Value(value));
  });
}
Handle mu3ac_literal(Context *c, const char *s, std::uint64_t n) {
  return c->guard([&] { auto v = std::string_view(s,n); if (v == "nil") return Handle(0);
    if (v != "true" && v != "false") throw Error("invalid Mu3AC literal"); return c->keep(Value(v == "true")); });
}
Handle mu3ac_load(Context *c, Environment *e, const char *name, std::uint64_t n) {
  return c->guard([&] { return c->keep(e->lookup(std::string(name,n))->value); });
}
void mu3ac_bind(Context *c, Environment *e, const char *name, std::uint64_t n, Handle value, std::uint64_t kind) {
  c->guard([&] { c->bind(e,std::string(name,n),c->get(value),kind); });
}
void mu3ac_store(Context *c, Environment *e, const char *name, std::uint64_t n, Handle value) {
  c->guard([&] { auto cell = e->lookup(std::string(name,n)); if (!cell->mutable_) throw Error("cannot assign immutable binding"); cell->value = c->get(value); });
}
void mu3ac_check_store(Context *c, Environment *e, const char *name, std::uint64_t n) {
  c->guard([&] { if (!e->lookup(std::string(name,n))->mutable_) throw Error("cannot assign immutable binding"); });
}
void mu3ac_alias(Context *c, Environment *e, const View *names) {
  c->guard([&] { auto name = string(names[0]); if (e->values.contains(name)) throw Error("duplicate binding: " + name);
    auto cell = e->lookup(string(names[1])); if (!cell->mutable_) throw Error("cannot reference immutable binding"); e->values[name] = cell; });
}
Handle mu3ac_unary(Context *c, std::uint64_t op, Handle a) {
  return c->guard([&] { const auto &v = c->get(a); if (op == 2) return c->keep(Value(!v.truth()));
    return c->keep(Value(op == 1 ? -v.number() : v.number())); });
}
Handle mu3ac_binary(Context *c, std::uint64_t op, Handle a, Handle b) {
  return c->guard([&] {
    // Copies survive arena growth while retaining shared immutable collections.
    auto x = c->get(a), y = c->get(b);
    if (op == 5 || op == 6) return c->keep(Value(op == 5 ? x == y : !(x == y)));
    if (op == 0 && x.type() == "string" && y.type() == "string") return c->keep(Value(x.string()+y.string()));
    if (op == 0 && x.type() == "list" && y.type() == "list") {
      auto list = x.list(); list.insert(list.end(),y.list().begin(),y.list().end()); return c->keep(Value(std::move(list)));
    }
    auto l = x.number(), r = y.number();
    if ((op == 3 || op == 4) && r == 0) throw Error("division by zero");
    switch (op) {
      case 0: return c->keep(Value(l+r)); case 1: return c->keep(Value(l-r)); case 2: return c->keep(Value(l*r));
      case 3: return c->keep(Value(l/r)); case 4: return c->keep(Value(std::fmod(l,r)));
      case 7: return c->keep(Value(l<r)); case 8: return c->keep(Value(l>r)); case 9: return c->keep(Value(l<=r)); case 10: return c->keep(Value(l>=r));
      default: throw Error("invalid Mu3AC binary operator");
    }
  });
}
Handle mu3ac_member(Context *c, Handle a, const char *name, std::uint64_t n) {
  return c->guard([&] { return c->keep(c->get(a).member(std::string(name,n))); });
}
Handle mu3ac_index(Context *c, Handle a, Handle b) {
  return c->guard([&] {
    auto value = c->get(a), key = c->get(b);
    if (value.type() == "mulib") {
      if (key.type() == "string") return c->keep(value.member(key.string()));
      if (!value.mulib()->index) throw Error("mulib value is not indexable"); return c->keep(value.mulib()->index(key));
    }
    if (value.type() == "record") return c->keep(value.member(key.string()));
    auto index = key.number(); auto size = value.type() == "string" ? value.string().size() : value.list().size();
    if (index < 0 || std::floor(index) != index || index >= double(size)) throw Error("index out of bounds");
    return c->keep(value.type() == "string" ? Value(value.string().substr(static_cast<std::size_t>(index),1)) : value.list()[static_cast<std::size_t>(index)]);
  });
}
Handle mu3ac_truth(Context *c, Handle a) { return c->guard([&] { return Handle(c->get(a).truth()); }); }
Handle mu3ac_call(Context *c, Handle a, const Handle *args, std::uint64_t n) {
  return c->guard([&] { auto function = c->get(a); auto values = c->arguments(args,n); return c->keep(function.call(values)); });
}
Handle mu3ac_list(Context *c, void *, const Handle *args, std::uint64_t n) {
  return c->guard([&] { return c->keep(Value(c->arguments(args,n))); });
}
Handle mu3ac_record(Context *c, const View *keys, const Handle *args, std::uint64_t n) {
  return c->guard([&] { Value::Object result; for (std::size_t i = 0; i < n; ++i)
    if (!result.emplace(string(keys[i]),c->get(args[i])).second) throw Error("duplicate record key");
    return c->keep(Value(std::move(result))); });
}
Handle mu3ac_closure(Context *c, Environment *e, Entry entry, const View *params, std::uint64_t count) {
  return c->guard([&] { return c->keep(Value(Value::Native([c,e,entry,params,count](const Value::List &args) {
    arity(args,count); if (c->calls >= c->config.maxCalls) throw Error("call depth limit exceeded");
    ++c->calls; struct Guard { Context *c; ~Guard() { --c->calls; } } guard{c};
    auto scope = c->frame(e); for (std::size_t i = 0; i < count; ++i) c->bind(scope,string(params[i]),args[i],1);
    return c->get(entry(c,scope));
  }))); });
}
}
