#include "../mu3ac/Mu3AC.hpp"
#include "../stdlib/Stdlib.hpp"

int main(int argc, char **argv) {
  try {
    mull::mu3ac::Options options;
    std::string stage = "run", file, rules, mu3acRules;
    bool repl = false; std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
      std::string key = argv[i];
      auto value = [&]() { if (++i == argc) throw mull::Error("missing worker option value: " + key); return std::string(argv[i]); };
      if (key == "--") { while (++i < argc) args.emplace_back(argv[i]); break; }
      if (key == "--stage") stage = value();
      else if (key == "--file") file = value();
      else if (key == "--name") options.source = value();
      else if (key == "--path") options.paths.push_back(value());
      else if (key == "--preload") options.preload.push_back(value());
      else if (key == "--rules") options.mullierRules = rules = value();
      else if (key == "--mu3ac-rules") mu3acRules = value();
      else if (key == "--no-stdlib") options.stdlib = false;
      else if (key == "--print-result") options.printResult = true;
      else if (key == "--no-optimize") options.optimize = false;
      else if (key == "--repl") repl = true;
      else if (key == "--foreign") {
        auto name = value(), symbol = value(), arity = value();
        options.foreign.push_back({name,symbol,mull::mu3ac::integer(dsl::SExpr::atom(arity))});
      } else if (key == "--max-steps") options.maxSteps = mull::mu3ac::integer(dsl::SExpr::atom(value()));
      else if (key == "--max-call-depth") options.maxCalls = mull::mu3ac::integer(dsl::SExpr::atom(value()));
      else if (key == "--max-frames") options.maxFrames = mull::mu3ac::integer(dsl::SExpr::atom(value()));
      else if (key == "--max-source-bytes") options.maxSource = mull::mu3ac::integer(dsl::SExpr::atom(value()));
      else if (key == "--max-values") options.maxValues = mull::mu3ac::integer(dsl::SExpr::atom(value()));
      else throw mull::Error("unknown worker option: " + key);
    }
    mull::Runtime vm(std::vector<std::filesystem::path>{});
    vm.maxSteps = options.maxSteps; vm.maxCallDepth = options.maxCalls;
    vm.maxFrames = options.maxFrames; vm.maxSourceBytes = options.maxSource;
    vm.mullier.optimize = options.optimize;
    if (!rules.empty()) vm.mullier.loadDirectory(rules);
    for (const auto &path : options.paths) vm.addLibraryPath(path);
    if (options.stdlib) mull::stdlib::install(vm);
    mull::Value::List values; for (const auto &arg : args) values.emplace_back(arg); vm.define("Args",mull::Value(values));
    for (const auto &f : options.foreign) {
      if (!mull::mu3ac::identifier(f.name) || !mull::mu3ac::identifier(f.symbol) || f.arity > 8) throw mull::Error("invalid foreign declaration");
      vm.define(f.name,mull::Value(mull::Value::Native([](const auto &) { throw mull::Error("foreign calls require the native backend"); return mull::Value{}; })));
    }
    if (stage == "run") {
      for (const auto &name : options.preload) if (!vm.loadLibrary(name).truth()) throw mull::Error(vm.lastError());
      if (repl) {
        std::string source; std::cerr << "MuLL (C++); one complete program per line; :quit exits\n";
        while (std::cerr << "mull> " && std::getline(std::cin,source)) {
          if (source == ":quit") break;
          if (source.empty()) continue;
          try { std::cout << "=> " << vm.run(source,"<repl>").text() << '\n'; }
          catch (const std::exception &e) { std::cerr << e.what() << '\n'; }
        }
      } else {
        auto result = vm.run(vm.readSource(file),options.source); if (options.printResult) std::cout << result.text() << '\n';
      }
      return 0;
    }
    auto source = vm.readSource(file);
    if (stage == "ast") { std::cout << vm.parse(source,options.source).dump() << '\n'; return 0; }
    auto ir = vm.compile(source,options.source);
    if (stage == "check") return 0;
    if (stage == "mullier") { std::cout << ir.dump() << '\n'; return 0; }
    auto program = mull::mu3ac::Lowering{}.compile(ir);
    if (options.optimize) {
      mull::mu3ac::Rewriter rewrite;
      if (!mu3acRules.empty()) rewrite.loadRules(vm.readSource(mu3acRules));
      rewrite.optimize(program);
    }
    if (stage == "mu3ac") std::cout << program.dump();
    else if (stage == "asm") std::cout << mull::mu3ac::X86_64{}.emit(program,options);
    else throw mull::Error("unknown compilation stage: " + stage);
    return 0;
  } catch (const mull::Error &e) {
    if (e.line) std::cerr << "mull-cpp:" << e.line << ':' << e.column << ": ";
    std::cerr << e.what() << '\n'; return 1;
  } catch (const std::exception &e) { std::cerr << "mull-cpp: " << e.what() << '\n'; return 1; }
}
