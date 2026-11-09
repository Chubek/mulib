#pragma once
#include "../../MuDSL.hpp"
#include "DefaultRules.hpp"
#include <set>
#include <optional>

/// Mu3AC v1: non-SSA value registers, explicit scopes/control flow and call params.
/// Arithmetic instructions have at most two value inputs. Calls consume adjacent
/// param instructions. Registers contain opaque runtime handles, never raw Values.
namespace mull::mu3ac {
using dsl::SExpr;
inline SExpr num(std::size_t n) { return SExpr::atom(std::to_string(n)); }
inline SExpr quote(const std::string &s) { return SExpr::atom(s, true); }
inline std::size_t integer(const SExpr &s) {
  std::size_t n = 0;
  if (!s.is_atom || s.quoted || s.text.empty()) throw Error("Mu3AC: expected unsigned integer");
  auto r = std::from_chars(s.text.data(), s.text.data() + s.text.size(), n);
  if (r.ec != std::errc{} || r.ptr != s.text.data() + s.text.size()) throw Error("Mu3AC: invalid integer");
  return n;
}
struct Site { std::size_t line = 0, column = 0; };
struct Instruction {
  std::string op;
  std::vector<SExpr> args;
  Site site;
  SExpr sexpr() const {
    std::vector<SExpr> a{SExpr::atom(op)};
    a.insert(a.end(), args.begin(), args.end());
    a.push_back(SExpr::list({SExpr::atom("site"), num(site.line), num(site.column)}));
    return SExpr::list(std::move(a));
  }
};
struct Function {
  std::vector<std::string> params;
  std::size_t registers = 0;
  std::vector<Instruction> code;
};
struct Program {
  std::vector<Function> functions;
  std::string dump() const {
    std::string s = "(mu3ac 1\n";
    for (std::size_t i = 0; i < functions.size(); ++i) {
      const auto &f = functions[i];
      std::vector<SExpr> names{SExpr::atom("params")};
      for (const auto &n : f.params) names.push_back(quote(n));
      s += "  (function " + std::to_string(i) + " " + std::to_string(f.registers) + " " + SExpr::list(names).dump() + "\n";
      for (const auto &ins : f.code) s += "    " + ins.sexpr().dump() + "\n";
      s += "  )\n";
    }
    return s + ")\n";
  }
};

class Lowering {
  Program out_;
  std::size_t function_ = 0, labels_ = 0, scope_ = 0, total_ = 0;
  struct Loop { std::size_t end, scope; };
  std::vector<Loop> loops_;
  Function &fn() { return out_.functions.at(function_); }
  std::size_t reg() {
    if (fn().registers >= 4096) throw Error("Mu3AC: register limit exceeded (4096 per function)");
    return fn().registers++;
  }
  void emit(std::string op, std::vector<SExpr> args, Site site) {
    if (++total_ > 262144) throw Error("Mu3AC: instruction limit exceeded");
    fn().code.push_back({std::move(op), std::move(args), site});
  }
  [[noreturn]] void fail(const std::string &message, Site site) {
    throw Error("Mu3AC: " + message, site.line, site.column);
  }
  std::size_t lower(const SExpr &term, std::size_t depth = 0) {
    if (depth > 512 || term.is_atom || term.items.size() < 4) throw Error("Mu3AC: malformed or excessively nested MuLLIER node");
    const auto &a = term.items;
    if (!a[0].is_atom || !a[1].is_atom || a[2].is_atom || a[2].items.empty() ||
        a[2].items[0].text != "names" || a[3].is_atom || a[3].items.size() != 3 || a[3].items[0].text != "site")
      throw Error("Mu3AC: malformed MuLLIER metadata");
    Site site{integer(a[3].items[1]), integer(a[3].items[2])};
    const auto &op = a[0].text, &text = a[1].text;
    auto child = [&](std::size_t i) { if (i + 4 >= a.size()) fail("missing child of " + op, site); return lower(a[i+4], depth+1); };
    auto count = [&](std::size_t n) { if (a.size() != n+4) fail("invalid child count for " + op, site); };
    emit("tick", {}, site);
    auto dst = reg();
    if (op == "program" || op == "block") {
      emit("nil", {num(dst)}, site);
      if (op == "block") { emit("enter", {}, site); ++scope_; }
      for (std::size_t i = 0; i+4 < a.size(); ++i) { auto v = child(i); emit("move", {num(dst),num(v)}, site); }
      if (op == "block") { emit("leave", {}, site); --scope_; }
    } else if (op == "number" || op == "string" || op == "literal") {
      count(0); emit(op, {num(dst),quote(text)}, site);
    } else if (op == "name") {
      count(0); emit("load", {num(dst),quote(text)}, site);
    } else if (op == "val" || op == "var" || op == "mulib" || op == "ref") {
      count(1);
      if (op == "ref") {
        const auto &target = a[4];
        if (target.is_atom || target.items.size() < 4 || target.items[0].text != "name") fail("ref requires a name", site);
        emit("alias", {quote(text),quote(target.items[1].text)}, site);
        emit("load", {num(dst),quote(text)}, site);
      } else {
        auto value = child(0);
        emit("bind", {quote(op),quote(text),num(value)}, site);
        emit("move", {num(dst),num(value)}, site);
      }
    } else if (op == "assign") {
      count(2);
      if (a[4].is_atom || a[4].items.size() < 4 || a[4].items[0].text != "name") fail("assignment requires a name", site);
      emit("check-store", {quote(a[4].items[1].text)}, site);
      auto value = child(1); emit("store", {quote(a[4].items[1].text),num(value)}, site);
      emit("move", {num(dst),num(value)}, site);
    } else if (op == "unary") {
      count(1); auto value = child(0); emit("unary", {num(dst),quote(text),num(value)}, site);
    } else if (op == "binary") {
      count(2); auto left = child(0);
      if (text == "and" || text == "or") {
        auto end = labels_++, rhs = labels_++;
        emit("move", {num(dst),num(left)}, site);
        emit("branch-false", {num(left),num(text == "and" ? end : rhs)}, site);
        if (text == "or") { emit("jump", {num(end)}, site); emit("label", {num(rhs)}, site); }
        auto right = child(1); emit("move", {num(dst),num(right)}, site); emit("label", {num(end)}, site);
      } else {
        auto right = child(1); emit("binary", {num(dst),quote(text),num(left),num(right)}, site);
      }
    } else if (op == "if") {
      if (a.size() != 6 && a.size() != 7) fail("invalid if", site);
      auto other = labels_++, end = labels_++, test = child(0);
      emit("nil", {num(dst)}, site); emit("branch-false", {num(test),num(other)}, site);
      auto yes = child(1); emit("move", {num(dst),num(yes)}, site); emit("jump", {num(end)}, site);
      emit("label", {num(other)}, site);
      if (a.size() == 7) { auto no = child(2); emit("move", {num(dst),num(no)}, site); }
      emit("label", {num(end)}, site);
    } else if (op == "while") {
      count(2); auto begin = labels_++, end = labels_++;
      emit("nil", {num(dst)}, site); emit("label", {num(begin)}, site);
      auto test = child(0); emit("branch-false", {num(test),num(end)}, site);
      loops_.push_back({end,scope_}); auto body = child(1); loops_.pop_back();
      emit("move", {num(dst),num(body)}, site); emit("jump", {num(begin)}, site); emit("label", {num(end)}, site);
    } else if (op == "break") {
      count(0); if (loops_.empty()) fail("break outside loop", site);
      for (auto i = scope_; i > loops_.back().scope; --i) emit("leave", {}, site);
      emit("jump", {num(loops_.back().end)}, site);
    } else if (op == "return") {
      if (a.size() > 5) fail("invalid return", site);
      if (a.size() == 5) { auto value = child(0); emit("return", {num(value)}, site); }
      else { emit("nil", {num(dst)}, site); emit("return", {num(dst)}, site); }
    } else if (op == "fn") {
      count(1); if (out_.functions.size() >= 4096) fail("function limit exceeded", site);
      auto parent = function_, savedScope = scope_, index = out_.functions.size();
      auto savedLoops = std::move(loops_); loops_.clear(); scope_ = 0;
      out_.functions.emplace_back(); function_ = index;
      for (std::size_t i = 1; i < a[2].items.size(); ++i) fn().params.push_back(a[2].items[i].text);
      if (fn().params.size() > 256) fail("parameter limit exceeded", site);
      auto value = child(0); emit("return", {num(value)}, site);
      function_ = parent; scope_ = savedScope; loops_ = std::move(savedLoops);
      emit("closure", {num(dst),num(index)}, site);
    } else if (op == "call" || op == "list" || op == "object") {
      std::vector<std::size_t> values; std::size_t callee = 0, start = 0;
      if (op == "call") { callee = child(0); start = 1; }
      for (std::size_t i = start; i+4 < a.size(); ++i) values.push_back(child(i));
      if (values.size() > 256) fail("argument/collection literal limit exceeded (256)", site);
      for (auto value : values) emit("param", {num(value)}, site);
      if (op == "call") emit("call", {num(dst),num(callee),num(values.size())}, site);
      else if (op == "list") emit("list", {num(dst),num(values.size())}, site);
      else {
        if (a[2].items.size() != values.size()+1) fail("invalid record keys", site);
        emit("record", {num(dst),a[2],num(values.size())}, site);
      }
    } else if (op == "member") {
      count(1); auto value = child(0); emit("member", {num(dst),num(value),quote(text)}, site);
    } else if (op == "index") {
      count(2); auto value = child(0), key = child(1); emit("index", {num(dst),num(value),num(key)}, site);
    } else fail("unsupported MuLLIER operation: " + op, site);
    return dst;
  }
public:
  Program compile(const SExpr &tree) {
    out_ = {}; function_ = labels_ = scope_ = total_ = 0; loops_.clear(); out_.functions.emplace_back();
    auto result = lower(tree); emit("return", {num(result)}, {}); return std::move(out_);
  }
};

/// Bounded, local equality saturation. Facts never cross control-flow labels.
class Rewriter {
  std::vector<dsl::ERewriteRule> rules_ = dsl::sexpr_rules(default_rules);
public:
  dsl::SaturationLimits limits{8,512,1024};
  std::size_t changes = 0;
  void loadRules(std::string_view source) {
    if (source.size() > 1024*1024) throw Error("Mu3AC rewrite file too large");
    auto next = dsl::sexpr_rules(source); rules_ = std::move(next);
  }
  void optimize(Program &p) {
    changes = 0;
    for (auto &f : p.functions) {
      std::map<std::size_t,SExpr> facts;
      std::set<std::size_t> written;
      for (auto &i : f.code) {
        auto get = [&](std::size_t n) { auto r = integer(i.args.at(n)); auto it = facts.find(r); return it == facts.end() ? SExpr::atom("unknown") : it->second; };
        if (i.op == "label" || i.op == "jump" || i.op == "branch-false" || i.op == "return") { facts.clear(); written.clear(); continue; }
        bool writes = i.op == "number" || i.op == "literal" || i.op == "move" || i.op == "unary" || i.op == "load" ||
          i.op == "string" || i.op == "nil" || i.op == "binary" || i.op == "call" || i.op == "list" ||
          i.op == "record" || i.op == "member" || i.op == "index" || i.op == "closure";
        if (writes && !written.insert(integer(i.args.at(0))).second) facts.clear();
        if (i.op == "unary") {
          auto original = i.args; auto operand = get(2); auto operation = i.args[1].text;
          if (operation != "+" && operation != "-" && operation != "!" && operation != "not") throw Error("Mu3AC: invalid unary operator");
          std::string tag = operation == "+" ? "positive" : operation == "-" ? "negative" : "not";
          auto expression = SExpr::list({SExpr::atom(tag),operand});
          dsl::EGraph graph; auto root = graph.add(expression); graph.saturate(rules_,limits); auto result = graph.extract(root);
          if (!result.is_atom && result.items.size() == 2 && result.items[0].text == "use") {
            auto r = integer(result.items[1]); if (r >= f.registers) throw Error("Mu3AC rewrite produced invalid register");
            i.op = "move"; i.args = {original[0],num(r)}; ++changes;
          } else if (!result.is_atom && result.items.size() == 2 && result.items[0].text == "literal") {
            if (result.items[1].text != "true" && result.items[1].text != "false" && result.items[1].text != "nil") throw Error("Mu3AC rewrite produced invalid literal");
            i.op = "literal"; i.args = {original[0],quote(result.items[1].text)}; ++changes;
          }
        }
        if (i.op == "number") facts[integer(i.args[0])] = SExpr::list({SExpr::atom("number"),i.args[0]});
        else if (i.op == "literal") facts[integer(i.args[0])] = SExpr::list({SExpr::atom("literal"),i.args[1]});
        else if (i.op == "move") facts[integer(i.args[0])] = get(1);
        else if (i.op == "unary") {
          auto source = integer(i.args[2]);
          facts[integer(i.args[0])] = i.args[1].text == "-" ? SExpr::list({SExpr::atom("negative"),num(source)}) : SExpr::atom("unknown");
        } else if (i.op == "load" || i.op == "string" || i.op == "nil" || i.op == "binary" || i.op == "call" ||
                   i.op == "list" || i.op == "record" || i.op == "member" || i.op == "index" || i.op == "closure") facts.erase(integer(i.args[0]));
      }
    }
  }
};

struct Foreign { std::string name, symbol; std::size_t arity = 0; };
struct Options {
  std::string source = "<input>";
  std::string mullierRules;
  std::vector<std::string> paths, preload;
  std::vector<Foreign> foreign;
  std::size_t maxSteps = 1000000, maxCalls = 128, maxFrames = 100000, maxSource = 4194304, maxValues = 2000000;
  bool stdlib = true, printResult = false, optimize = true;
};
inline bool identifier(std::string_view s) {
  if (s.empty() || !((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z') || s[0] == '_')) return false;
  return s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789") == std::string_view::npos;
}

/// GNU assembler syntax, ELF/System V AMD64, SSE2 baseline. No shell or tool
/// invocation occurs here. Runtime helpers are provided by the selected host.
class X86_64 {
  std::string text_, data_, constants_;
  std::size_t strings_ = 0;
  std::string bytes(const std::string &s) {
    auto label = ".Lmu3ac_string" + std::to_string(strings_++);
    constants_ += label + ":\n  .byte ";
    for (unsigned char c : s) constants_ += std::to_string(c) + ",";
    constants_ += "0\n"; return label;
  }
  std::string views(const std::vector<std::string> &names) {
    auto label = ".Lmu3ac_views" + std::to_string(strings_++);
    std::vector<std::string> labels; for (const auto &s : names) labels.push_back(bytes(s));
    data_ += "  .p2align 3\n" + label + ":\n";
    for (std::size_t i = 0; i < names.size(); ++i) data_ += "  .quad " + labels[i] + "," + std::to_string(names[i].size()) + "\n";
    if (names.empty()) data_ += "  .quad 0,0\n";
    return label;
  }
  void line(const std::string &s) { text_ += "  " + s + "\n"; }
public:
  std::string emit(const Program &p, const Options &options = {}) {
    if (p.functions.empty() || p.functions.size() > 4096) throw Error("Mu3AC: invalid function count");
    text_ = ".text\n"; data_.clear(); constants_.clear(); strings_ = 0;
    const std::map<std::string,std::size_t> unary{{"+",0},{"-",1},{"!",2},{"not",2}};
    const std::map<std::string,std::size_t> binary{{"+",0},{"-",1},{"*",2},{"/",3},{"%",4},{"==",5},{"!=",6},{"<",7},{">",8},{"<=",9},{">=",10}};
    const std::map<std::string,std::string> schema{{"tick",""},{"nil","r"},{"number","rs"},{"string","rs"},
      {"literal","rs"},{"move","rr"},{"load","rs"},{"bind","ssr"},{"store","sr"},{"check-store","s"},
      {"alias","ss"},{"unary","rsr"},{"binary","rsrr"},{"member","rrs"},{"index","rrr"},
      {"enter",""},{"leave",""},{"label","n"},{"jump","n"},{"branch-false","rn"},{"return","r"},
      {"param","r"},{"call","rrn"},{"list","rn"},{"record","rkn"},{"closure","rn"}};
    std::size_t total = 0;
    for (std::size_t fno = 0; fno < p.functions.size(); ++fno) {
      const auto &f = p.functions[fno];
      if (!f.registers || f.registers > 4096 || f.params.size() > 256) throw Error("Mu3AC: invalid function dimensions");
      std::set<std::size_t> labels, targets;
      std::size_t maxArgs = 0, pending = 0;
      for (const auto &i : f.code) {
        if (++total > 262144) throw Error("Mu3AC: instruction limit exceeded");
        auto shape = schema.find(i.op);
        if (shape == schema.end() || i.args.size() != shape->second.size()) throw Error("Mu3AC: invalid instruction shape: " + i.op);
        for (std::size_t j = 0; j < i.args.size(); ++j) {
          const auto &arg = i.args[j]; auto type = shape->second[j];
          if (type == 'r' && integer(arg) >= f.registers) throw Error("Mu3AC: register out of range");
          if (type == 'n') integer(arg);
          if (type == 's' && (!arg.is_atom || !arg.quoted)) throw Error("Mu3AC: expected quoted payload");
          if (type == 'k') {
            if (arg.is_atom || arg.items.empty() || arg.items[0].text != "names") throw Error("Mu3AC: invalid record keys");
            for (std::size_t k = 1; k < arg.items.size(); ++k)
              if (!arg.items[k].is_atom || !arg.items[k].quoted) throw Error("Mu3AC: invalid record key");
          }
        }
        if (i.op == "label" && !labels.insert(integer(i.args.at(0))).second) throw Error("Mu3AC: duplicate label");
        if (i.op == "jump") targets.insert(integer(i.args.at(0)));
        if (i.op == "branch-false") targets.insert(integer(i.args.at(1)));
        if (i.op == "param") { if (++pending > 256) throw Error("Mu3AC: too many parameters"); maxArgs = std::max(maxArgs,pending); }
        else if (i.op == "call" || i.op == "list" || i.op == "record") {
          auto count = integer(i.args.at(i.op == "list" ? 1 : 2));
          if (pending != count) throw Error("Mu3AC: unbalanced call parameters"); pending = 0;
        } else if (pending) throw Error("Mu3AC: parameters must immediately precede their consumer");
      }
      if (pending) throw Error("Mu3AC: unterminated parameters");
      for (auto t : targets) if (!labels.contains(t)) throw Error("Mu3AC: unknown branch label");
      std::map<std::size_t,std::size_t> positions;
      for (std::size_t j = 0; j < f.code.size(); ++j)
        if (f.code[j].op == "label") positions[integer(f.code[j].args[0])] = j;
      std::vector<std::optional<std::size_t>> depths(f.code.size());
      std::vector<std::size_t> work;
      auto offer = [&](std::size_t j, std::size_t depth) {
        if (j >= f.code.size()) return;
        if (depths[j]) { if (*depths[j] != depth) throw Error("Mu3AC: inconsistent scope depth at branch join"); }
        else { depths[j] = depth; work.push_back(j); }
      };
      offer(0,0);
      for (std::size_t j = 0; j < work.size(); ++j) {
        auto at = work[j], depth = *depths[at]; const auto &ins = f.code[at];
        if (ins.op == "enter") ++depth;
        if (ins.op == "leave") { if (!depth) throw Error("Mu3AC: scope underflow"); --depth; }
        if (ins.op == "return") continue;
        if (ins.op == "jump") { offer(positions.at(integer(ins.args[0])),depth); continue; }
        if (ins.op == "branch-false") offer(positions.at(integer(ins.args[1])),depth);
        offer(at+1,depth);
      }
      auto stack = ((f.registers+2+maxArgs)*8+15)/16*16;
      auto slot = [&](std::size_t r) { if (r >= f.registers) throw Error("Mu3AC: register out of range"); return "-" + std::to_string((r+3)*8) + "(%rbp)"; };
      auto reg = [&](const Instruction &i, std::size_t n) { return slot(integer(i.args.at(n))); };
      auto label = [&](std::size_t n) { return ".Lmu3ac_" + std::to_string(fno) + "_" + std::to_string(n); };
      auto context = [&] { line("movq -8(%rbp), %rdi"); };
      auto environment = [&] { line("movq -16(%rbp), %rsi"); };
      auto address = [&](const std::string &s, const std::string &r) { line("leaq " + s + "(%rip), " + r); };
      auto immediate = [&](std::size_t n, const std::string &r) { line("movabsq $" + std::to_string(n) + ", " + r); };
      auto call = [&](const std::string &name) { line("call mu3ac_" + name + "@PLT"); };
      auto payload = [&](const std::string &s, const std::string &ptr, const std::string &len) { address(bytes(s),ptr); immediate(s.size(),len); };
      auto argv = [&] { line("leaq -" + std::to_string(stack) + "(%rbp), %rdx"); };
      if (!fno) text_ += ".globl mu3ac_entry\n.type mu3ac_entry,@function\nmu3ac_entry:\n";
      text_ += ".Lmu3ac_fn" + std::to_string(fno) + ":\n  .cfi_startproc\n";
      line("pushq %rbp"); line(".cfi_def_cfa_offset 16"); line(".cfi_offset %rbp, -16"); line("movq %rsp, %rbp"); line(".cfi_def_cfa_register %rbp");
      line("subq $" + std::to_string(stack) + ", %rsp"); line("movq %rdi, -8(%rbp)"); line("movq %rsi, -16(%rbp)");
      for (std::size_t r = 0; r < f.registers; ++r) line("movq $0, " + slot(r));
      pending = 0;
      for (const auto &i : f.code) {
        const auto &op = i.op;
        if (op == "label") { text_ += label(integer(i.args.at(0))) + ":\n"; continue; }
        if (op == "jump") { line("jmp " + label(integer(i.args.at(0)))); continue; }
        if (op == "move") { line("movq " + reg(i,1) + ", %rax"); line("movq %rax, " + reg(i,0)); continue; }
        if (op == "nil") { line("movq $0, " + reg(i,0)); continue; }
        if (op == "return") { line("movq " + reg(i,0) + ", %rax"); line("jmp .Lmu3ac_return" + std::to_string(fno)); continue; }
        if (op == "param") {
          line("movq " + reg(i,0) + ", %rax"); line("movq %rax, -" + std::to_string(stack-pending*8) + "(%rbp)"); ++pending; continue;
        }
        context(); immediate(i.site.line,"%rsi"); immediate(i.site.column,"%rdx"); call(op == "tick" ? "tick" : "site");
        if (op == "tick") continue;
        context();
        bool result = true;
        if (op == "enter" || op == "leave") { environment(); call(op); line("movq %rax, -16(%rbp)"); result = false; }
        else if (op == "number" || op == "string" || op == "literal") {
          payload(i.args.at(1).text,"%rsi","%rdx"); call(op);
        } else if (op == "load") { environment(); payload(i.args.at(1).text,"%rdx","%rcx"); call("load"); }
        else if (op == "bind") {
          environment(); payload(i.args.at(1).text,"%rdx","%rcx"); line("movq " + reg(i,2) + ", %r8");
          immediate(i.args.at(0).text == "var" ? 1 : i.args[0].text == "mulib" ? 2 : 0,"%r9"); call("bind"); result = false;
        } else if (op == "store") {
          environment(); payload(i.args.at(0).text,"%rdx","%rcx"); line("movq " + reg(i,1) + ", %r8"); call("store"); result = false;
        } else if (op == "check-store") {
          environment(); payload(i.args.at(0).text,"%rdx","%rcx"); call("check_store"); result = false;
        } else if (op == "alias") {
          environment(); address(views({i.args.at(0).text,i.args.at(1).text}),"%rdx"); call("alias"); result = false;
        } else if (op == "unary" || op == "binary") {
          const auto &table = op == "unary" ? unary : binary;
          auto it = table.find(i.args.at(1).text); if (it == table.end()) throw Error("Mu3AC: invalid operator");
          immediate(it->second,"%rsi"); line("movq " + reg(i,2) + ", %rdx");
          if (op == "binary") line("movq " + reg(i,3) + ", %rcx"); call(op);
        } else if (op == "member") { line("movq " + reg(i,1) + ", %rsi"); payload(i.args.at(2).text,"%rdx","%rcx"); call("member"); }
        else if (op == "index") { line("movq " + reg(i,1) + ", %rsi"); line("movq " + reg(i,2) + ", %rdx"); call("index"); }
        else if (op == "branch-false") {
          line("movq " + reg(i,0) + ", %rsi"); call("truth"); line("testq %rax, %rax"); line("jz " + label(integer(i.args.at(1)))); result = false;
        } else if (op == "call" || op == "list" || op == "record") {
          if (op == "call") line("movq " + reg(i,1) + ", %rsi");
          else if (op == "record") {
            std::vector<std::string> keys;
            const auto &names = i.args.at(1);
            if (names.is_atom || names.items.size() != pending+1) throw Error("Mu3AC: invalid record keys");
            for (std::size_t k = 1; k < names.items.size(); ++k) keys.push_back(names.items[k].text);
            address(views(keys),"%rsi");
          }
          argv(); immediate(pending,"%rcx"); call(op); pending = 0;
        } else if (op == "closure") {
          auto index = integer(i.args.at(1)); if (index >= p.functions.size()) throw Error("Mu3AC: invalid function reference");
          environment(); address(".Lmu3ac_fn" + std::to_string(index),"%rdx");
          address(views(p.functions[index].params),"%rcx"); immediate(p.functions[index].params.size(),"%r8"); call("closure");
        } else throw Error("Mu3AC: unknown instruction " + op);
        if (result) line("movq %rax, " + reg(i,0));
      }
      line("xorq %rax, %rax"); text_ += ".Lmu3ac_return" + std::to_string(fno) + ":\n";
      line("leave"); line(".cfi_def_cfa %rsp, 8"); line("ret"); line(".cfi_endproc");
    }
    auto source = bytes(options.source), rules = bytes(options.mullierRules), paths = views(options.paths), preloads = views(options.preload);
    std::vector<std::string> foreignNames;
    for (const auto &f : options.foreign) {
      if (!identifier(f.name) || !identifier(f.symbol) || f.arity > 8) throw Error("Mu3AC: invalid foreign declaration");
      foreignNames.push_back(bytes(f.name));
    }
    auto foreign = ".Lmu3ac_foreign";
    data_ += "  .p2align 3\n" + std::string(foreign) + ":\n";
    for (std::size_t i = 0; i < options.foreign.size(); ++i) {
      const auto &f = options.foreign[i];
      data_ += "  .quad " + foreignNames[i] + "," + std::to_string(f.name.size()) + "," + f.symbol + "," + std::to_string(f.arity) + "\n";
    }
    if (options.foreign.empty()) data_ += "  .quad 0,0,0,0\n";
    data_ += ".globl mu3ac_config\n  .p2align 3\nmu3ac_config:\n  .quad 1," + source + "," + std::to_string(options.source.size()) + "\n";
    data_ += "  .quad " + rules + "," + std::to_string(options.mullierRules.size()) + "\n";
    for (auto n : {options.maxSteps,options.maxCalls,options.maxFrames,options.maxSource,options.maxValues,
                   options.paths.size(),options.preload.size(),options.foreign.size()}) data_ += "  .quad " + std::to_string(n) + "\n";
    data_ += "  .quad " + paths + "," + preloads + "," + foreign + "\n  .quad " +
      std::to_string((options.stdlib ? 1 : 0) | (options.printResult ? 2 : 0) | (options.optimize ? 4 : 0)) + "\n";
    return text_ + ".section .rodata\n" + constants_ + ".section .data.rel.ro\n" + data_ + ".section .note.GNU-stack,\"\",@progbits\n";
  }
};
} // namespace mull::mu3ac
