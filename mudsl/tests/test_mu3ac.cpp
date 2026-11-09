#include "mull/mu3ac/Mu3AC.hpp"
#include <cassert>

int main() {
  mull::Runtime vm;
  vm.define("effect",0,true);
  auto ast = vm.parse("effect <- 9; return 42;");
  auto ir = vm.compile("effect <- 9; return 42;");
  assert(vm.get("effect").number() == 0); // Neither compile-only API executes.
  auto program = mull::mu3ac::Lowering{}.compile(ir);
  assert(program.dump().find("check-store") != std::string::npos);
  auto assembly = mull::mu3ac::X86_64{}.emit(program);
  assert(assembly.find("mu3ac_entry:") != std::string::npos);
  assert(assembly.find(".cfi_startproc") != std::string::npos);
  assert(assembly.find(".note.GNU-stack") != std::string::npos);
  vm.mullier.optimize = false;
  auto rewriteProgram = mull::mu3ac::Lowering{}.compile(vm.compile("return -(-(+4));"));
  mull::mu3ac::Rewriter rewrite;
  rewrite.optimize(rewriteProgram);
  assert(rewrite.changes >= 1);
  // Failed replacement is transactional, just like the MuLLIER loader.
  try { rewrite.loadRules("("); assert(false); } catch (const std::exception &) {}
  auto after = mull::mu3ac::Lowering{}.compile(vm.compile("return +4;"));
  rewrite.optimize(after); assert(rewrite.changes == 1);
  for (const auto &source : {"break;", "val a <- 1; a <- 2;"}) {
    try { vm.compile(source); assert(false); } catch (const mull::Error &) {}
  }
  auto closure = mull::mu3ac::Lowering{}.compile(vm.compile("val f <- fn(x){return x+1;}; return f(41);"));
  assert(closure.functions.size() == 2 && closure.functions[1].params[0] == "x");
  auto invalid = program;
  invalid.functions[0].code.push_back({"jump",{mull::mu3ac::num(999)}, {}});
  try { mull::mu3ac::X86_64{}.emit(invalid); assert(false); } catch (const mull::Error &) {}
  invalid = program; invalid.functions[0].code.insert(invalid.functions[0].code.begin(),{"leave",{}, {}});
  try { mull::mu3ac::X86_64{}.emit(invalid); assert(false); } catch (const mull::Error &) {}
  invalid = program; invalid.functions[0].code.push_back({"move",{}, {}});
  try { mull::mu3ac::X86_64{}.emit(invalid); assert(false); } catch (const mull::Error &) {}
  invalid = program; invalid.functions[0].code.push_back({"move",{mull::mu3ac::num(99999),mull::mu3ac::num(0)}, {}});
  try { mull::mu3ac::X86_64{}.emit(invalid); assert(false); } catch (const mull::Error &) {}
}
