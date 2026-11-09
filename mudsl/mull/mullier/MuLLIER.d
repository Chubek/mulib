module mull.mullier.MuLLIER;

import MuDSL : SExpr, EGraph, ERewriteRule, SaturationLimits, SaturationResult,
    parse_sexpressions, sexpr_rule, sexpr_rules;
import mull.mullier.DefaultRules;
import std.conv : to;

class Diagnostic : Exception
{
    size_t line, column;
    this(string message, size_t l, size_t c)
    { super(message); line = l; column = c; }
}

private string op(SExpr term) { return term.items[0].text; }
private string text(SExpr term) { return term.items[1].text; }
private void fail(SExpr term, string message)
{
    auto site = term.items[3].items;
    throw new Diagnostic(message, site[1].text.to!size_t, site[2].text.to!size_t);
}

/// MuLLIER uses the same interchange format and rule files as the C++ runtime.
class Pipeline
{
    private ERewriteRule[] analyses, transforms;
    private bool lexical, control;
    private alias Scope = bool[string];
    bool optimize = true;
    SaturationLimits limits = SaturationLimits(12, 20000, 20000);
    SaturationResult lastSaturation;
    SExpr lastIR;

    this() { readAnalysis(default_analysis); transforms = sexpr_rules(default_transforms); }

    private void readAnalysis(string source)
    {
        foreach (term; parse_sexpressions(source))
        {
            if (!term.is_atom && term.items.length == 2 && term.items[0] == SExpr.atom("check"))
            {
                if (term.items[1] == SExpr.atom("lexical")) lexical = true;
                else if (term.items[1] == SExpr.atom("control-flow")) control = true;
                else throw new Exception("unknown MuLLIER analysis policy");
            }
            else analyses ~= sexpr_rule(term);
        }
    }
    private bool binding(SExpr term)
    {
        auto graph = new EGraph;
        auto root = graph.add(SExpr.list([SExpr.atom("category"), term]));
        auto expected = graph.add("binding");
        if (!graph.saturate(analyses, limits).saturated())
            fail(term, "MuLLIER analysis saturation limit exceeded");
        return graph.equivalent(root, expected);
    }
    private void analyze(SExpr term, ref Scope[] scopes, size_t loops, size_t captureFloor = 0)
    {
        auto kind = op(term);
        if (control && kind == "break" && !loops) fail(term, "break outside loop");
        bool scoped = kind == "block" || kind == "fn";
        if (scoped) scopes ~= Scope.init;
        if (kind == "fn")
        {
            loops = 0;
            captureFloor = scopes.length - 1;
            foreach (name; term.items[2].items[1 .. $]) scopes[$-1][name.text] = true;
        }
        bool* lookup(string name)
        {
            foreach_reverse (i; captureFloor .. scopes.length)
                if (auto found = name in scopes[i]) return found;
            return null;
        }
        bool declaration = kind == "val" || kind == "var" || kind == "ref" || kind == "mulib";
        if (lexical)
        {
            if (declaration && text(term) in scopes[$-1]) fail(term, "duplicate binding: " ~ text(term));
            if (kind == "ref" || kind == "assign")
            {
                auto target = term.items[4];
                if (!binding(target)) fail(term, kind == "ref" ? "ref requires a mutable binding" : "assignment requires a binding");
                auto mut = lookup(text(target));
                if (mut && !*mut) fail(term, kind == "ref" ? "cannot reference immutable binding" : "cannot assign immutable binding");
            }
        }
        foreach (i; 4 .. term.items.length)
            analyze(term.items[i], scopes, loops + (kind == "while" && i == 5), captureFloor);
        if (declaration) scopes[$-1][text(term)] = kind == "var" || kind == "ref";
        if (scoped) scopes.length--;
    }
    void loadDirectory(string directory)
    {
        import std.file : dirEntries, SpanMode, read;
        import std.path : extension;
        import std.algorithm : sort;
        auto next = new Pipeline;
        next.analyses = null; next.transforms = null;
        next.lexical = next.control = false;
        string[] files;
        foreach (entry; dirEntries(directory, SpanMode.shallow))
            if (entry.isFile && (extension(entry.name) == ".a" || extension(entry.name) == ".t")) files ~= entry.name;
        sort(files);
        bool a, t;
        foreach (file; files)
        {
            try
            {
                // Read at most one byte beyond the limit, rather than allocating
                // the entire file before checking its size.
                auto source = cast(string) read(file, 1024 * 1024 + 1);
                if (source.length > 1024 * 1024) throw new Exception("MuLLIER rule file too large");
                if (extension(file) == ".a") { next.readAnalysis(source); a = true; }
                else { next.transforms ~= sexpr_rules(source); t = true; }
            }
            catch (Exception e) { throw new Exception(file ~ ": " ~ e.msg); }
        }
        if (!a || !t) throw new Exception("MuLLIER requires both .a and .t rule files");
        analyses = next.analyses; transforms = next.transforms;
        lexical = next.lexical; control = next.control;
    }
    SExpr process(SExpr program, Scope globals = null)
    {
        Scope[] scopes = [globals.dup];
        analyze(program, scopes, 0);
        lastIR = program; lastSaturation = SaturationResult.init;
        if (optimize)
        {
            auto graph = new EGraph;
            auto root = graph.add(program);
            lastSaturation = graph.saturate(transforms, limits);
            lastIR = graph.extract(root);
        }
        return lastIR;
    }
}
