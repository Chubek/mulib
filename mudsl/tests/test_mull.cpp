#include "MuDSL.hpp"
#include "mull/stdlib/Stdlib.hpp"
#include <cassert>
#include <cstdlib>
#include <chrono>

static void mullierChecks() {
  mull::Runtime vm;
  vm.mullier.loadDirectory(std::filesystem::path(__FILE__).parent_path() / "../mull/rewrite");
  assert(vm.run("return true and (false or +42);").number() == 42);
  assert(vm.mullier.lastSaturation.saturated());
  auto optimized = vm.mullier.lastIR.dump();
  assert(optimized.find("binary") == std::string::npos);
  assert(dsl::decode_sexpr(dsl::encode_sexpr(vm.mullier.lastIR)) == vm.mullier.lastIR);
  vm.mullier.optimize = false;
  assert(vm.run("return true and (false or +42);").number() == 42);
  assert(vm.mullier.lastIR.dump().find("binary") != std::string::npos);
  vm.define("effect", 0, true);
  for (auto source : {"effect <- 1; val a <- 1; a <- 2;", "effect <- 1; while true { val f <- fn(){break;}; break; }"}) {
    try { vm.run(source, "analysis.mull"); assert(false); }
    catch (const mull::Error &e) { assert(e.line == 1 && e.column > 0); }
    assert(vm.get("effect").number() == 0);
  }
  // Closures can see bindings declared later in their captured environment.
  assert(vm.run("val late <- 0; { val f <- fn(){late <- 7;}; var late <- 1; f(); return late; }").number() == 7);
  vm.mullier.optimize = true;
  vm.mullier.limits.nodes = 1;
  assert(vm.run("return true and 9;").number() == 9);
  assert(vm.mullier.lastSaturation.stop == dsl::SaturationStop::NodeLimit);
  vm.mullier.limits.nodes = 20000;
  // A failed reload must leave the previous rule set intact.
  try { vm.mullier.loadDirectory(std::filesystem::path(__FILE__).parent_path()); assert(false); }
  catch (const std::invalid_argument &) {}
  assert(vm.run("return false or 8;").number() == 8);
  assert(vm.mullier.lastIR.dump().find("binary") == std::string::npos);
  auto directory = std::filesystem::temp_directory_path() / ("mullier-cpp-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(directory);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); }
  } cleanup{directory};
  auto write = [&](const char *name, const std::string &source) {
    std::ofstream out(directory / name, std::ios::binary);
    out << source;
    out.close();
    assert(out);
  };
  std::string analysis(mull::mullier::default_analysis);
  analysis.resize(1024 * 1024, ' ');
  write("01.a", analysis);
  write("02.t", std::string(mull::mullier::default_transforms));
  vm.mullier.loadDirectory(directory); // Exactly the byte limit is accepted.
  for (const auto &source : {analysis + " ", std::string("(check unknown)")}) {
    write("01.a", source);
    try { vm.mullier.loadDirectory(directory); assert(false); }
    catch (const std::invalid_argument &e) {
      assert(std::string(e.what()).find("01.a") != std::string::npos);
    }
    assert(vm.run("return false or 8;").number() == 8);
    assert(vm.mullier.lastIR.dump().find("binary") == std::string::npos);
    try { vm.run("if false { break; }"); assert(false); }
    catch (const mull::Error &) {} // Analysis policies survive failed reloads too.
  }
  write("01.a", "(check lexical)");
  write("02.t", "("); // Failure after an analysis file has already been parsed.
  try { vm.mullier.loadDirectory(directory); assert(false); }
  catch (const std::invalid_argument &) {}
  try { vm.run("if false { break; }"); assert(false); }
  catch (const mull::Error &) {}
}

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
  std::string nested = "1", chain;
  for (int i = 0; i < 200; ++i) chain += "+1";
  for (int i = 0; i < 3; ++i) nested = "(" + nested + chain + ")";
  fails(nested + ";", "IR nesting limit");
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
    if (argc == 1) { hostChecks(); mullierChecks(); return 0; }
    if (argc != 3) return 2;
    mull::Runtime vm({argv[1]}); mull::stdlib::install(vm);
    vm.mullier.optimize = std::getenv("MULL_NO_OPTIMIZE") == nullptr;
    vm.addLibraryPath(std::filesystem::path(argv[2]).parent_path() / "packages");
    auto value = vm.runFile(argv[2]);
    std::cout << "=> " << value.text() << '\n';
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
