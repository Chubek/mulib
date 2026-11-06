#include "MuDSL.hpp"
#include "mull/stdlib/Stdlib.hpp"
#include <cassert>

static void hostChecks() {
  mull::Runtime vm;
  vm.define("Double", mull::Value(mull::Value::Native([](const auto &a) {
    mull::arity(a,1); return mull::Value(a[0].number()*2);
  })));
  assert(vm.run("var x <- Double(21); ref y <- x; y <- y + 1; x;").number() == 43);
  assert(vm.get("x").number() == 43);
  assert(vm.run("val make <- fn(a) { return fn(b) { return a + b; }; }; val add <- make(10); add(5);").number() == 15);
  assert(vm.get("add").call({mull::Value(2)}).number() == 12);
  auto fails = [&](std::string source, std::string message) {
    try { vm.run(source); assert(false); }
    catch (const mull::Error &e) { assert(std::string(e.what()).find(message) != std::string::npos); }
  };
  fails("x <- 0; val broken <- ;", "expected identifier");
  assert(vm.get("x").number() == 43);
  fails("add <- 2;", "immutable");
  vm.maxSteps = 30;
  fails("while true {}", "execution limit");
  vm.maxSteps = 100000;
  assert(vm.run("2+2;").number() == 4);
  vm.maxCallDepth = 5;
  fails("val recurse <- fn() { return recurse(); }; recurse();", "call depth");
  assert(vm.run("add(3);").number() == 13);
  vm.maxSourceBytes = 3;
  fails("12345;", "source size");
  vm.maxSourceBytes = 10000;
  fails(std::string(300, '(') + "1" + std::string(300, ')') + ";", "nesting limit");
  assert(!vm.loadLibrary("../secret").truth());
  assert(vm.lastError().find("invalid library") != std::string::npos);
  mull::Runtime other;
  try { other.get("x"); assert(false); } catch (const mull::Error &) {}
  fails("G:MulibWrap(\"Parsec\");", "wrapper is not installed");
#ifdef MULL_ENABLE_MULIB
  mull::stdlib::installMulib(vm);
  mull::stdlib::installMulib(other);
  vm.define("foreign", other.run("G:MulibWrap(\"Parsec\"):New(\"uint8_t\", 65);"));
  fails("G:MulibWrap(\"Parsec\"):Byte(foreign);", "another MuLIB runtime");
  mull::Runtime limited;
  mull::stdlib::installMulib(limited,1);
  try { limited.run("G:MulibWrap(\"Parsec\"):New(\"result\");"); assert(false); }
  catch (const mull::Error &e) { assert(std::string(e.what()).find("storage limit")!=std::string::npos); }
  // Native callback slots must be reusable after their owning runtime dies.
  for (int i=0;i<16;++i) {
    mull::Runtime temporary;
    mull::stdlib::installMulib(temporary);
    assert(temporary.run("val P <- G:MulibWrap(\"Parsec\"); val cb <- P:Callback(\"mulib_parsec_predicate_fn\", fn(b,u){return b;}); cb(42,nil);").number()==42);
  }
#endif
}

int main(int argc, char **argv) {
  try {
    if (argc == 1) { hostChecks(); return 0; }
    if (argc != 3) return 2;
    mull::Runtime vm({argv[1]}); mull::stdlib::install(vm);
    vm.addLibraryPath(std::filesystem::path(argv[2]).parent_path() / "packages");
    auto value = vm.runFile(argv[2]);
    std::cout << "=> " << value.text() << '\n';
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
