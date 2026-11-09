#pragma once
// Included by MuDSL.hpp after the toolkit definitions. Public entry point:
// mull/mullier/MuLLIER.hpp. No separate parser or e-graph implementation.
#include "DefaultRules.hpp"
#include <filesystem>
#include <fstream>
#include <map>

namespace mull::mullier {
using dsl::SExpr;

struct Diagnostic : std::runtime_error {
  std::size_t line, column;
  Diagnostic(std::string message, std::size_t l, std::size_t c)
      : std::runtime_error(std::move(message)), line(l), column(c) {}
};

// Every node is (op "text" (names "...") (site line column) child ...).
// Names/text are quoted data, even if they start with a pattern-variable marker.
inline const std::string &op(const SExpr &term) { return term.items.at(0).text; }
inline const std::string &text(const SExpr &term) { return term.items.at(1).text; }
inline void fail(const SExpr &term, const std::string &message) {
  const auto &site = term.items.at(3).items;
  throw Diagnostic(message, std::stoull(site.at(1).text), std::stoull(site.at(2).text));
}

class Pipeline {
  std::vector<dsl::ERewriteRule> analyses_, transforms_;
  bool lexical_ = false, control_ = false;
  using Scope = std::map<std::string, bool>;

  void readAnalysis(std::string_view source) {
    for (const auto &term : dsl::parse_sexpressions(source)) {
      if (!term.is_atom && term.items.size() == 2 && term.items[0] == SExpr::atom("check")) {
        if (term.items[1] == SExpr::atom("lexical")) lexical_ = true;
        else if (term.items[1] == SExpr::atom("control-flow")) control_ = true;
        else throw std::invalid_argument("unknown MuLLIER analysis policy");
      } else analyses_.push_back(dsl::sexpr_rule(term));
    }
  }
  bool binding(const SExpr &term) const {
    dsl::EGraph graph;
    auto root = graph.add(SExpr::list({SExpr::atom("category"), term}));
    auto expected = graph.add("binding");
    if (!graph.saturate(analyses_, limits).saturated())
      fail(term, "MuLLIER analysis saturation limit exceeded");
    return graph.equivalent(root, expected);
  }
  void analyze(const SExpr &term, std::vector<Scope> &scopes, std::size_t loops, std::size_t captureFloor = 0) const {
    const auto &kind = op(term);
    if (control_ && kind == "break" && !loops) fail(term, "break outside loop");
    bool scope = kind == "block" || kind == "fn";
    if (scope) scopes.emplace_back();
    if (kind == "fn") {
      loops = 0;
      captureFloor = scopes.size() - 1;
      for (std::size_t i = 1; i < term.items[2].items.size(); ++i)
        scopes.back()[term.items[2].items[i].text] = true;
    }
    auto lookup = [&](const std::string &name) -> const bool * {
      for (auto i = scopes.size(); i > captureFloor; --i) {
        auto found = scopes[i-1].find(name);
        if (found != scopes[i-1].end()) return &found->second;
      }
      return nullptr; // Late-bound globals/captures are checked by the evaluator.
    };
    bool declaration = kind == "val" || kind == "var" || kind == "ref" || kind == "mulib";
    if (lexical_) {
      if (declaration && scopes.back().contains(text(term))) fail(term, "duplicate binding: " + text(term));
      if (kind == "ref" || kind == "assign") {
        const auto &target = term.items.at(4);
        if (!binding(target)) fail(term, kind == "ref" ? "ref requires a mutable binding" : "assignment requires a binding");
        if (auto mut = lookup(text(target)); mut && !*mut)
          fail(term, kind == "ref" ? "cannot reference immutable binding" : "cannot assign immutable binding");
      }
    }
    for (std::size_t i = 4; i < term.items.size(); ++i)
      analyze(term.items[i], scopes, loops + (kind == "while" && i == 5), captureFloor);
    if (declaration) scopes.back()[text(term)] = kind == "var" || kind == "ref";
    if (scope) scopes.pop_back();
  }

public:
  bool optimize = true;
  dsl::SaturationLimits limits{12, 20000, 20000};
  dsl::SaturationResult lastSaturation;
  SExpr lastIR;
  Pipeline() { readAnalysis(default_analysis); transforms_ = dsl::sexpr_rules(default_transforms); }

  // A complete, transactional rule-set replacement. Rules are trusted compiler
  // extensions: arbitrary user equalities need not preserve MuLL semantics.
  void loadDirectory(const std::filesystem::path &directory) {
    Pipeline next; next.analyses_.clear(); next.transforms_.clear();
    next.lexical_ = next.control_ = false;
    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
      if (entry.is_regular_file() && (entry.path().extension() == ".a" || entry.path().extension() == ".t")) files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    bool a = false, t = false;
    for (const auto &file : files) {
      std::ifstream in(file, std::ios::binary);
      if (!in) throw std::invalid_argument("cannot open MuLLIER rules: " + file.string());
      try {
        // Bound the read itself, including files that grow after being opened.
        constexpr std::size_t maxBytes = 1024 * 1024;
        std::string source(maxBytes + 1, '\0');
        in.read(source.data(), source.size());
        if (in.bad()) throw std::invalid_argument("cannot read MuLLIER rules");
        source.resize(static_cast<std::size_t>(in.gcount()));
        if (source.size() > maxBytes) throw std::invalid_argument("MuLLIER rule file too large");
        if (file.extension() == ".a") { next.readAnalysis(source); a = true; }
        else { auto rules = dsl::sexpr_rules(source); next.transforms_.insert(next.transforms_.end(), rules.begin(), rules.end()); t = true; }
      } catch (const std::exception &e) { throw std::invalid_argument(file.string() + ": " + e.what()); }
    }
    if (!a || !t) throw std::invalid_argument("MuLLIER requires both .a and .t rule files");
    analyses_ = std::move(next.analyses_); transforms_ = std::move(next.transforms_);
    lexical_ = next.lexical_; control_ = next.control_;
  }
  SExpr process(const SExpr &program, const Scope &globals = {}) {
    std::vector<Scope> scopes{globals};
    analyze(program, scopes, 0); // Check the original program, including dead code.
    lastIR = program; lastSaturation = {};
    if (optimize) {
      dsl::EGraph graph;
      auto root = graph.add(program);
      lastSaturation = graph.saturate(transforms_, limits);
      lastIR = graph.extract(root); // Limits retain a valid equivalent representative.
    }
    return lastIR;
  }
};
} // namespace mull::mullier
