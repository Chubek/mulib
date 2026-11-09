module mull.mu3ac.Mu3AC;
import MuDSL : SExpr, EGraph, ERewriteRule, SaturationLimits, sexpr_rules, MuLLError;
import mull.mu3ac.DefaultRules;
import std.conv : to;
import std.algorithm : canFind, max;

private SExpr num(size_t n) { return SExpr.atom(n.to!string); }
private SExpr quote(string s) { return SExpr.atom(s, true); }
size_t integer(SExpr s)
{
    if (!s.is_atom || s.quoted || !s.text.length) throw new MuLLError("Mu3AC: expected unsigned integer");
    foreach (c; s.text) if (c < '0' || c > '9') throw new MuLLError("Mu3AC: invalid integer");
    try { return s.text.to!size_t; } catch (Exception e) { throw new MuLLError("Mu3AC: invalid integer"); }
}
struct Site { size_t line, column; }
struct Instruction
{
    string op; SExpr[] args; Site site;
    SExpr sexpr()
    {
        return SExpr.list([SExpr.atom(op)] ~ args ~ SExpr.list([SExpr.atom("site"), num(site.line), num(site.column)]));
    }
}
class Function { string[] params; size_t registers; Instruction[] code; }
struct Program
{
    Function[] functions;
    string dump()
    {
        string s = "(mu3ac 1\n";
        foreach (i, f; functions)
        {
            SExpr[] names = [SExpr.atom("params")]; foreach (name; f.params) names ~= quote(name);
            s ~= "  (function " ~ i.to!string ~ " " ~ f.registers.to!string ~ " " ~ SExpr.list(names).dump() ~ "\n";
            foreach (ins; f.code) s ~= "    " ~ ins.sexpr().dump() ~ "\n";
            s ~= "  )\n";
        }
        return s ~ ")\n";
    }
}

class Lowering
{
    private Program output;
    private size_t functionIndex, labels, scopeDepth, total;
    private struct Loop { size_t end, scopeDepth; }
    private Loop[] loops;
    private Function fn() { return output.functions[functionIndex]; }
    private size_t reg()
    {
        if (fn.registers >= 4096) throw new MuLLError("Mu3AC: register limit exceeded (4096 per function)");
        return fn.registers++;
    }
    private void emit(string op, SExpr[] args, Site site)
    {
        if (++total > 262144) throw new MuLLError("Mu3AC: instruction limit exceeded");
        fn.code ~= Instruction(op, args, site);
    }
    private void fail(string message, Site site) { throw new MuLLError("Mu3AC: " ~ message, site.line, site.column); }
    private size_t lower(SExpr term, size_t depth = 0)
    {
        if (depth > 512 || term.is_atom || term.items.length < 4) throw new MuLLError("Mu3AC: malformed or excessively nested MuLLIER node");
        auto a = term.items;
        if (!a[0].is_atom || !a[1].is_atom || a[2].is_atom || !a[2].items.length || a[2].items[0].text != "names" ||
            a[3].is_atom || a[3].items.length != 3 || a[3].items[0].text != "site")
            throw new MuLLError("Mu3AC: malformed MuLLIER metadata");
        auto site = Site(integer(a[3].items[1]), integer(a[3].items[2]));
        auto op = a[0].text, text = a[1].text;
        size_t child(size_t i) { if (i+4 >= a.length) fail("missing child of " ~ op, site); return lower(a[i+4], depth+1); }
        void count(size_t n) { if (a.length != n+4) fail("invalid child count for " ~ op, site); }
        emit("tick", null, site); auto dst = reg();
        if (op == "program" || op == "block")
        {
            emit("nil", [num(dst)], site);
            if (op == "block") { emit("enter", null, site); ++scopeDepth; }
            foreach (i; 0 .. a.length-4) { auto v = child(i); emit("move", [num(dst), num(v)], site); }
            if (op == "block") { emit("leave", null, site); --scopeDepth; }
        }
        else if (["number", "string", "literal"].canFind(op)) { count(0); emit(op, [num(dst), quote(text)], site); }
        else if (op == "name") { count(0); emit("load", [num(dst), quote(text)], site); }
        else if (["val", "var", "mulib", "ref"].canFind(op))
        {
            count(1);
            if (op == "ref")
            {
                auto target = a[4];
                if (target.is_atom || target.items.length < 4 || target.items[0].text != "name") fail("ref requires a name", site);
                emit("alias", [quote(text), quote(target.items[1].text)], site);
                emit("load", [num(dst), quote(text)], site);
            }
            else
            {
                auto value = child(0); emit("bind", [quote(op), quote(text), num(value)], site);
                emit("move", [num(dst), num(value)], site);
            }
        }
        else if (op == "assign")
        {
            count(2);
            if (a[4].is_atom || a[4].items.length < 4 || a[4].items[0].text != "name") fail("assignment requires a name", site);
            emit("check-store", [quote(a[4].items[1].text)], site);
            auto value = child(1); emit("store", [quote(a[4].items[1].text), num(value)], site);
            emit("move", [num(dst), num(value)], site);
        }
        else if (op == "unary") { count(1); auto value = child(0); emit("unary", [num(dst), quote(text), num(value)], site); }
        else if (op == "binary")
        {
            count(2); auto left = child(0);
            if (text == "and" || text == "or")
            {
                auto end = labels++, rhs = labels++;
                emit("move", [num(dst), num(left)], site);
                emit("branch-false", [num(left), num(text == "and" ? end : rhs)], site);
                if (text == "or") { emit("jump", [num(end)], site); emit("label", [num(rhs)], site); }
                auto right = child(1); emit("move", [num(dst), num(right)], site); emit("label", [num(end)], site);
            }
            else { auto right = child(1); emit("binary", [num(dst), quote(text), num(left), num(right)], site); }
        }
        else if (op == "if")
        {
            if (a.length != 6 && a.length != 7) fail("invalid if", site);
            auto other = labels++, end = labels++, test = child(0);
            emit("nil", [num(dst)], site); emit("branch-false", [num(test), num(other)], site);
            auto yes = child(1); emit("move", [num(dst), num(yes)], site); emit("jump", [num(end)], site);
            emit("label", [num(other)], site);
            if (a.length == 7) { auto no = child(2); emit("move", [num(dst), num(no)], site); }
            emit("label", [num(end)], site);
        }
        else if (op == "while")
        {
            count(2); auto begin = labels++, end = labels++;
            emit("nil", [num(dst)], site); emit("label", [num(begin)], site);
            auto test = child(0); emit("branch-false", [num(test), num(end)], site);
            loops ~= Loop(end, scopeDepth); auto value = child(1); loops.length--;
            emit("move", [num(dst), num(value)], site); emit("jump", [num(begin)], site); emit("label", [num(end)], site);
        }
        else if (op == "break")
        {
            count(0); if (!loops.length) fail("break outside loop", site);
            foreach (i; loops[$-1].scopeDepth .. scopeDepth) emit("leave", null, site);
            emit("jump", [num(loops[$-1].end)], site);
        }
        else if (op == "return")
        {
            if (a.length > 5) fail("invalid return", site);
            if (a.length == 5) { auto value = child(0); emit("return", [num(value)], site); }
            else { emit("nil", [num(dst)], site); emit("return", [num(dst)], site); }
        }
        else if (op == "fn")
        {
            count(1); if (output.functions.length >= 4096) fail("function limit exceeded", site);
            auto parent = functionIndex, savedScope = scopeDepth, index = output.functions.length;
            auto savedLoops = loops; loops = null; scopeDepth = 0;
            output.functions ~= new Function; functionIndex = index;
            foreach (name; a[2].items[1 .. $]) fn.params ~= name.text;
            if (fn.params.length > 256) fail("parameter limit exceeded", site);
            auto value = child(0); emit("return", [num(value)], site);
            functionIndex = parent; scopeDepth = savedScope; loops = savedLoops;
            emit("closure", [num(dst), num(index)], site);
        }
        else if (["call", "list", "object"].canFind(op))
        {
            size_t[] values; size_t callee, start;
            if (op == "call") { callee = child(0); start = 1; }
            foreach (i; start .. a.length-4) values ~= child(i);
            if (values.length > 256) fail("argument/collection literal limit exceeded (256)", site);
            foreach (value; values) emit("param", [num(value)], site);
            if (op == "call") emit("call", [num(dst), num(callee), num(values.length)], site);
            else if (op == "list") emit("list", [num(dst), num(values.length)], site);
            else
            {
                if (a[2].items.length != values.length+1) fail("invalid record keys", site);
                emit("record", [num(dst), a[2], num(values.length)], site);
            }
        }
        else if (op == "member") { count(1); auto value = child(0); emit("member", [num(dst), num(value), quote(text)], site); }
        else if (op == "index") { count(2); auto value = child(0), key = child(1); emit("index", [num(dst), num(value), num(key)], site); }
        else fail("unsupported MuLLIER operation: " ~ op, site);
        return dst;
    }
    Program compile(SExpr tree)
    {
        output = Program.init; functionIndex = labels = scopeDepth = total = 0; loops = null; output.functions ~= new Function;
        auto result = lower(tree); emit("return", [num(result)], Site.init); return output;
    }
}

class Rewriter
{
    private ERewriteRule[] rules;
    SaturationLimits limits = SaturationLimits(8, 512, 1024);
    size_t changes;
    this() { rules = sexpr_rules(default_rules); }
    void loadRules(string source)
    {
        if (source.length > 1024*1024) throw new MuLLError("Mu3AC rewrite file too large");
        auto next = sexpr_rules(source); rules = next;
    }
    void optimize(ref Program p)
    {
        changes = 0;
        foreach (f; p.functions)
        {
            SExpr[size_t] facts;
            bool[size_t] written;
            foreach (ref i; f.code)
            {
                SExpr get(size_t n) { auto r = integer(i.args[n]); if (auto v = r in facts) return *v; return SExpr.atom("unknown"); }
                if (["label", "jump", "branch-false", "return"].canFind(i.op)) { facts = null; written = null; continue; }
                if (["number", "literal", "move", "unary", "load", "string", "nil", "binary", "call", "list", "record", "member", "index", "closure"].canFind(i.op))
                {
                    auto dest = integer(i.args[0]); if (dest in written) facts = null; written[dest] = true;
                }
                if (i.op == "unary")
                {
                    auto original = i.args; auto operand = get(2), operation = i.args[1];
                    if (!["+", "-", "!", "not"].canFind(operation.text)) throw new MuLLError("Mu3AC: invalid unary operator");
                    auto tag = operation.text == "+" ? "positive" : operation.text == "-" ? "negative" : "not";
                    auto expression = SExpr.list([SExpr.atom(tag), operand]);
                    auto graph = new EGraph; auto root = graph.add(expression); graph.saturate(rules, limits); auto result = graph.extract(root);
                    if (!result.is_atom && result.items.length == 2 && result.items[0].text == "use")
                    {
                        auto r = integer(result.items[1]); if (r >= f.registers) throw new MuLLError("Mu3AC rewrite produced invalid register");
                        i.op = "move"; i.args = [original[0], num(r)]; ++changes;
                    }
                    else if (!result.is_atom && result.items.length == 2 && result.items[0].text == "literal")
                    {
                        if (!["true", "false", "nil"].canFind(result.items[1].text)) throw new MuLLError("Mu3AC rewrite produced invalid literal");
                        i.op = "literal"; i.args = [original[0], quote(result.items[1].text)]; ++changes;
                    }
                }
                if (i.op == "number") facts[integer(i.args[0])] = SExpr.list([SExpr.atom("number"), i.args[0]]);
                else if (i.op == "literal") facts[integer(i.args[0])] = SExpr.list([SExpr.atom("literal"), i.args[1]]);
                else if (i.op == "move") facts[integer(i.args[0])] = get(1);
                else if (i.op == "unary") facts[integer(i.args[0])] = i.args[1].text == "-" ? SExpr.list([SExpr.atom("negative"), num(integer(i.args[2]))]) : SExpr.atom("unknown");
                else if (["load", "string", "nil", "binary", "call", "list", "record", "member", "index", "closure"].canFind(i.op)) facts.remove(integer(i.args[0]));
            }
        }
    }
}

struct Foreign { string name, symbol; size_t arity; }
struct Options
{
    string source = "<input>", mullierRules;
    string[] paths, preload;
    Foreign[] foreign;
    size_t maxSteps = 1000000, maxCalls = 128, maxFrames = 100000, maxSource = 4194304, maxValues = 2000000;
    bool stdlib = true, printResult, optimize = true;
}
bool identifier(string s)
{
    if (!s.length || !(s[0] >= 'A' && s[0] <= 'Z' || s[0] >= 'a' && s[0] <= 'z' || s[0] == '_')) return false;
    foreach (c; s) if (!"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789".canFind(c)) return false;
    return true;
}

class X86_64
{
    private string text, data, constants;
    private size_t strings;
    private string bytes(string s)
    {
        auto label = ".Lmu3ac_string" ~ (strings++).to!string;
        constants ~= label ~ ":\n  .byte "; foreach (c; cast(ubyte[]) s) constants ~= c.to!string ~ ",";
        constants ~= "0\n"; return label;
    }
    private string views(string[] names)
    {
        auto label = ".Lmu3ac_views" ~ (strings++).to!string; string[] labels;
        foreach (s; names) labels ~= bytes(s);
        data ~= "  .p2align 3\n" ~ label ~ ":\n";
        foreach (i, name; names) data ~= "  .quad " ~ labels[i] ~ "," ~ name.length.to!string ~ "\n";
        if (!names.length) data ~= "  .quad 0,0\n";
        return label;
    }
    private void line(string s) { text ~= "  " ~ s ~ "\n"; }
    string emit(Program p, Options options = Options.init)
    {
        if (!p.functions.length || p.functions.length > 4096) throw new MuLLError("Mu3AC: invalid function count");
        text = ".text\n"; data = constants = ""; strings = 0;
        size_t[string] unary = ["+":0, "-":1, "!":2, "not":2];
        size_t[string] binary = ["+":0, "-":1, "*":2, "/":3, "%":4, "==":5, "!=":6, "<":7, ">":8, "<=":9, ">=":10];
        string[string] schema = ["tick":"", "nil":"r", "number":"rs", "string":"rs", "literal":"rs", "move":"rr", "load":"rs", "bind":"ssr",
            "store":"sr", "check-store":"s", "alias":"ss", "unary":"rsr", "binary":"rsrr", "member":"rrs", "index":"rrr", "enter":"", "leave":"",
            "label":"n", "jump":"n", "branch-false":"rn", "return":"r", "param":"r", "call":"rrn", "list":"rn", "record":"rkn", "closure":"rn"];
        size_t total;
        foreach (fno, f; p.functions)
        {
            if (!f.registers || f.registers > 4096 || f.params.length > 256) throw new MuLLError("Mu3AC: invalid function dimensions");
            bool[size_t] labels, targets; size_t maxArgs, pending;
            foreach (i; f.code)
            {
                if (++total > 262144) throw new MuLLError("Mu3AC: instruction limit exceeded");
                auto shape = i.op in schema;
                if (!shape || i.args.length != (*shape).length) throw new MuLLError("Mu3AC: invalid instruction shape: " ~ i.op);
                foreach (j, arg; i.args)
                {
                    auto type = (*shape)[j];
                    if (type == 'r' && integer(arg) >= f.registers) throw new MuLLError("Mu3AC: register out of range");
                    if (type == 'n') integer(arg);
                    if (type == 's' && (!arg.is_atom || !arg.quoted)) throw new MuLLError("Mu3AC: expected quoted payload");
                    if (type == 'k')
                    {
                        if (arg.is_atom || !arg.items.length || arg.items[0].text != "names") throw new MuLLError("Mu3AC: invalid record keys");
                        foreach (key; arg.items[1 .. $]) if (!key.is_atom || !key.quoted) throw new MuLLError("Mu3AC: invalid record key");
                    }
                }
                if (i.op == "label") { auto n = integer(i.args[0]); if (n in labels) throw new MuLLError("Mu3AC: duplicate label"); labels[n] = true; }
                if (i.op == "jump") targets[integer(i.args[0])] = true;
                if (i.op == "branch-false") targets[integer(i.args[1])] = true;
                if (i.op == "param") { if (++pending > 256) throw new MuLLError("Mu3AC: too many parameters"); maxArgs = max(maxArgs, pending); }
                else if (["call", "list", "record"].canFind(i.op))
                {
                    auto count = integer(i.args[i.op == "list" ? 1 : 2]);
                    if (pending != count) throw new MuLLError("Mu3AC: unbalanced call parameters"); pending = 0;
                }
                else if (pending) throw new MuLLError("Mu3AC: parameters must immediately precede their consumer");
            }
            if (pending) throw new MuLLError("Mu3AC: unterminated parameters");
            foreach (t; targets.byKey) if (!(t in labels)) throw new MuLLError("Mu3AC: unknown branch label");
            size_t[size_t] positions, depths; size_t[] work;
            foreach (j, ins; f.code) if (ins.op == "label") positions[integer(ins.args[0])] = j;
            void offer(size_t j, size_t depth)
            {
                if (j >= f.code.length) return;
                if (auto known = j in depths) { if (*known != depth) throw new MuLLError("Mu3AC: inconsistent scope depth at branch join"); }
                else { depths[j] = depth; work ~= j; }
            }
            offer(0,0);
            for (size_t j = 0; j < work.length; ++j)
            {
                auto at = work[j], depth = depths[at]; auto ins = f.code[at];
                if (ins.op == "enter") ++depth;
                if (ins.op == "leave") { if (!depth) throw new MuLLError("Mu3AC: scope underflow"); --depth; }
                if (ins.op == "return") continue;
                if (ins.op == "jump") { offer(positions[integer(ins.args[0])],depth); continue; }
                if (ins.op == "branch-false") offer(positions[integer(ins.args[1])],depth);
                offer(at+1,depth);
            }
            auto stack = ((f.registers+2+maxArgs)*8+15)/16*16;
            string slot(size_t r) { if (r >= f.registers) throw new MuLLError("Mu3AC: register out of range"); return "-" ~ ((r+3)*8).to!string ~ "(%rbp)"; }
            string reg(Instruction i, size_t n) { return slot(integer(i.args[n])); }
            string label(size_t n) { return ".Lmu3ac_" ~ fno.to!string ~ "_" ~ n.to!string; }
            void context() { line("movq -8(%rbp), %rdi"); }
            void env() { line("movq -16(%rbp), %rsi"); }
            void address(string s, string r) { line("leaq " ~ s ~ "(%rip), " ~ r); }
            void immediate(size_t n, string r) { line("movabsq $" ~ n.to!string ~ ", " ~ r); }
            void call(string name) { line("call mu3ac_" ~ name ~ "@PLT"); }
            void payload(string s, string ptr, string len) { address(bytes(s), ptr); immediate(s.length, len); }
            void argv() { line("leaq -" ~ stack.to!string ~ "(%rbp), %rdx"); }
            if (!fno) text ~= ".globl mu3ac_entry\n.type mu3ac_entry,@function\nmu3ac_entry:\n";
            text ~= ".Lmu3ac_fn" ~ fno.to!string ~ ":\n  .cfi_startproc\n";
            line("pushq %rbp"); line(".cfi_def_cfa_offset 16"); line(".cfi_offset %rbp, -16"); line("movq %rsp, %rbp"); line(".cfi_def_cfa_register %rbp");
            line("subq $" ~ stack.to!string ~ ", %rsp"); line("movq %rdi, -8(%rbp)"); line("movq %rsi, -16(%rbp)");
            foreach (r; 0 .. f.registers) line("movq $0, " ~ slot(r));
            pending = 0;
            foreach (i; f.code)
            {
                auto op = i.op;
                if (op == "label") { text ~= label(integer(i.args[0])) ~ ":\n"; continue; }
                if (op == "jump") { line("jmp " ~ label(integer(i.args[0]))); continue; }
                if (op == "move") { line("movq " ~ reg(i,1) ~ ", %rax"); line("movq %rax, " ~ reg(i,0)); continue; }
                if (op == "nil") { line("movq $0, " ~ reg(i,0)); continue; }
                if (op == "return") { line("movq " ~ reg(i,0) ~ ", %rax"); line("jmp .Lmu3ac_return" ~ fno.to!string); continue; }
                if (op == "param") { line("movq " ~ reg(i,0) ~ ", %rax"); line("movq %rax, -" ~ (stack-pending*8).to!string ~ "(%rbp)"); ++pending; continue; }
                context(); immediate(i.site.line, "%rsi"); immediate(i.site.column, "%rdx"); call(op == "tick" ? "tick" : "site");
                if (op == "tick") continue;
                context(); bool result = true;
                if (op == "enter" || op == "leave") { env(); call(op); line("movq %rax, -16(%rbp)"); result = false; }
                else if (["number", "string", "literal"].canFind(op)) { payload(i.args[1].text, "%rsi", "%rdx"); call(op); }
                else if (op == "load") { env(); payload(i.args[1].text, "%rdx", "%rcx"); call("load"); }
                else if (op == "bind")
                {
                    env(); payload(i.args[1].text, "%rdx", "%rcx"); line("movq " ~ reg(i,2) ~ ", %r8");
                    immediate(i.args[0].text == "var" ? 1 : i.args[0].text == "mulib" ? 2 : 0, "%r9"); call("bind"); result = false;
                }
                else if (op == "store") { env(); payload(i.args[0].text, "%rdx", "%rcx"); line("movq " ~ reg(i,1) ~ ", %r8"); call("store"); result = false; }
                else if (op == "check-store") { env(); payload(i.args[0].text, "%rdx", "%rcx"); call("check_store"); result = false; }
                else if (op == "alias") { env(); address(views([i.args[0].text, i.args[1].text]), "%rdx"); call("alias"); result = false; }
                else if (op == "unary" || op == "binary")
                {
                    auto table = op == "unary" ? unary : binary; auto id = i.args[1].text in table;
                    if (!id) throw new MuLLError("Mu3AC: invalid operator");
                    immediate(*id, "%rsi"); line("movq " ~ reg(i,2) ~ ", %rdx"); if (op == "binary") line("movq " ~ reg(i,3) ~ ", %rcx"); call(op);
                }
                else if (op == "member") { line("movq " ~ reg(i,1) ~ ", %rsi"); payload(i.args[2].text, "%rdx", "%rcx"); call("member"); }
                else if (op == "index") { line("movq " ~ reg(i,1) ~ ", %rsi"); line("movq " ~ reg(i,2) ~ ", %rdx"); call("index"); }
                else if (op == "branch-false") { line("movq " ~ reg(i,0) ~ ", %rsi"); call("truth"); line("testq %rax, %rax"); line("jz " ~ label(integer(i.args[1]))); result = false; }
                else if (["call", "list", "record"].canFind(op))
                {
                    if (op == "call") line("movq " ~ reg(i,1) ~ ", %rsi");
                    else if (op == "record")
                    {
                        string[] keys; auto names = i.args[1];
                        if (names.is_atom || names.items.length != pending+1) throw new MuLLError("Mu3AC: invalid record keys");
                        foreach (key; names.items[1 .. $]) keys ~= key.text; address(views(keys), "%rsi");
                    }
                    argv(); immediate(pending, "%rcx"); call(op); pending = 0;
                }
                else if (op == "closure")
                {
                    auto index = integer(i.args[1]); if (index >= p.functions.length) throw new MuLLError("Mu3AC: invalid function reference");
                    env(); address(".Lmu3ac_fn" ~ index.to!string, "%rdx"); address(views(p.functions[index].params), "%rcx");
                    immediate(p.functions[index].params.length, "%r8"); call("closure");
                }
                else throw new MuLLError("Mu3AC: unknown instruction " ~ op);
                if (result) line("movq %rax, " ~ reg(i,0));
            }
            line("xorq %rax, %rax"); text ~= ".Lmu3ac_return" ~ fno.to!string ~ ":\n";
            line("leave"); line(".cfi_def_cfa %rsp, 8"); line("ret"); line(".cfi_endproc");
        }
        auto source = bytes(options.source), rules = bytes(options.mullierRules), paths = views(options.paths), preloads = views(options.preload);
        string[] foreignNames;
        foreach (f; options.foreign)
        {
            if (!identifier(f.name) || !identifier(f.symbol) || f.arity > 8) throw new MuLLError("Mu3AC: invalid foreign declaration");
            foreignNames ~= bytes(f.name);
        }
        auto foreign = ".Lmu3ac_foreign"; data ~= "  .p2align 3\n" ~ foreign ~ ":\n";
        foreach (i, f; options.foreign) data ~= "  .quad " ~ foreignNames[i] ~ "," ~ f.name.length.to!string ~ "," ~ f.symbol ~ "," ~ f.arity.to!string ~ "\n";
        if (!options.foreign.length) data ~= "  .quad 0,0,0,0\n";
        data ~= ".globl mu3ac_config\n  .p2align 3\nmu3ac_config:\n  .quad 1," ~ source ~ "," ~ options.source.length.to!string ~ "\n";
        data ~= "  .quad " ~ rules ~ "," ~ options.mullierRules.length.to!string ~ "\n";
        foreach (n; [options.maxSteps, options.maxCalls, options.maxFrames, options.maxSource, options.maxValues, options.paths.length, options.preload.length, options.foreign.length]) data ~= "  .quad " ~ n.to!string ~ "\n";
        data ~= "  .quad " ~ paths ~ "," ~ preloads ~ "," ~ foreign ~ "\n  .quad " ~ ((options.stdlib ? 1 : 0) | (options.printResult ? 2 : 0) | (options.optimize ? 4 : 0)).to!string ~ "\n";
        return text ~ ".section .rodata\n" ~ constants ~ ".section .data.rel.ro\n" ~ data ~ ".section .note.GNU-stack,\"\",@progbits\n";
    }
}
