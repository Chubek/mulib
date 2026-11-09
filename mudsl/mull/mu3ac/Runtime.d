module mull.mu3ac.Runtime;
import MuDSL;
import mull.stdlib.Stdlib;
import std.conv : to;
import std.math : floor, fmod;

alias Handle = ulong;
struct View { const(char)* data; ulong size; }
struct ForeignEntry { View name; void* address; ulong arity; }
struct Config
{
    ulong abiVersion;
    View source, mullierRules;
    ulong maxSteps, maxCalls, maxFrames, maxSource, maxValues;
    ulong pathCount, preloadCount, foreignCount;
    const(View)* paths, preloads;
    const(ForeignEntry)* foreign;
    ulong flags;
}
static assert(View.sizeof == 16 && ForeignEntry.sizeof == 32 && Config.sizeof == 136);
alias Entry = extern(C) Handle function(void*, void*);
private string str(View v) { return v.size ? cast(string) v.data[0 .. v.size] : ""; }
private class Cell { MuLLValue value; bool mutable_; this(MuLLValue v, bool m) { value = v; mutable_ = m; } }
private class Environment
{
    Environment parent; Cell[string] values;
    this(Environment p) { parent = p; }
    Cell lookup(string name)
    {
        for (auto e = this; e !is null; e = e.parent) if (auto v = name in e.values) return *v;
        throw new MuLLError("undefined binding: " ~ name);
    }
}
private class Context
{
    MuLLRuntime runtime;
    Config config;
    MuLLValue[] values;
    Environment[] environments;
    size_t steps, calls, frames, line, column;
    this(Config c)
    {
        config = c; values = [MuLLValue.init]; runtime = new MuLLRuntime([]);
        runtime.maxSteps = c.maxSteps; runtime.maxCallDepth = c.maxCalls;
        runtime.maxFrames = c.maxFrames; runtime.maxSourceBytes = c.maxSource; runtime.mullier.optimize = (c.flags & 4) != 0;
        if (c.mullierRules.size) runtime.mullier.loadDirectory(str(c.mullierRules));
        runtime.stepObserver = (size_t l, size_t col) { if (++steps > config.maxSteps) throw new MuLLError("execution limit exceeded", l, col); };
        runtime.frameObserver = () { frameBudget(); };
        runtime.callObserver = (bool enter) {
            if (!enter) { --calls; return; }
            if (calls >= config.maxCalls) throw new MuLLError("call depth limit exceeded"); ++calls;
        };
        foreach (i; 0 .. c.pathCount) runtime.addLibraryPath(str(c.paths[i]));
        if (c.flags & 1) install(runtime);
    }
    void frameBudget() { if (frames >= config.maxFrames) throw new MuLLError("environment limit exceeded"); ++frames; }
    Environment frame(Environment parent)
    {
        frameBudget(); auto e = new Environment(parent); environments ~= e; return e;
    }
    Handle keep(MuLLValue value)
    {
        if (values.length >= config.maxValues) throw new MuLLError("Mu3AC value limit exceeded");
        values ~= value; return values.length-1;
    }
    MuLLValue get(Handle h)
    { if (h >= values.length) throw new MuLLError("invalid Mu3AC value handle"); return values[h]; }
    void bind(Environment env, string name, MuLLValue value, ulong kind = 0)
    {
        if (name in env.values) throw new MuLLError("duplicate binding: " ~ name);
        if (kind == 2 && value.type() != "mulib") throw new MuLLError("mulib declaration requires a mulib value");
        env.values[name] = new Cell(value, kind == 1);
    }
    MuLLValue[] arguments(const(Handle)* args, size_t count)
    { MuLLValue[] result; foreach (i; 0 .. count) result ~= get(args[i]); return result; }
    auto guard(F)(F action)
    {
        try { return action(); }
        catch (MuLLError e) { if (e.line) throw e; throw new MuLLError(e.msg, line, column); }
        catch (Exception e) { throw new MuLLError(e.msg, line, column); }
    }
}
private alias CFunction(T...) = extern(C) double function(T);
private MuLLValue foreignValue(void* address, size_t count)
{
    if (address is null || count > 8) throw new MuLLError("invalid foreign function");
    return MuLLValue((MuLLValue[] a) {
        mullArity(a, count); double[8] x; foreach (i; 0 .. count) x[i] = a[i].number();
        double result;
        switch (count)
        {
            case 0: result = (cast(CFunction!()) address)(); break;
            case 1: result = (cast(CFunction!double) address)(x[0]); break;
            case 2: result = (cast(CFunction!(double,double)) address)(x[0],x[1]); break;
            case 3: result = (cast(CFunction!(double,double,double)) address)(x[0],x[1],x[2]); break;
            case 4: result = (cast(CFunction!(double,double,double,double)) address)(x[0],x[1],x[2],x[3]); break;
            case 5: result = (cast(CFunction!(double,double,double,double,double)) address)(x[0],x[1],x[2],x[3],x[4]); break;
            case 6: result = (cast(CFunction!(double,double,double,double,double,double)) address)(x[0],x[1],x[2],x[3],x[4],x[5]); break;
            case 7: result = (cast(CFunction!(double,double,double,double,double,double,double)) address)(x[0],x[1],x[2],x[3],x[4],x[5],x[6]); break;
            default: result = (cast(CFunction!(double,double,double,double,double,double,double,double)) address)(x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7]);
        }
        return MuLLValue(result);
    });
}
int nativeMain(Config config, Entry entry, string[] args)
{
    import std.stdio : stdout, stderr;
    try
    {
        if (config.abiVersion != 1) throw new MuLLError("unsupported Mu3AC native ABI");
        auto c = new Context(config); auto env = c.frame(null);
        c.bind(env, "G", c.runtime.get("G"));
        MuLLValue[] values; foreach (arg; args[1 .. $]) values ~= MuLLValue(arg);
        c.bind(env, "Args", MuLLValue(values));
        foreach (i; 0 .. config.foreignCount)
        {
            auto f = config.foreign[i]; c.bind(env, str(f.name), foreignValue(cast(void*) f.address, f.arity));
        }
        foreach (i; 0 .. config.preloadCount)
            if (!c.runtime.loadLibrary(str(config.preloads[i])).truth()) throw new MuLLError(c.runtime.lastError());
        auto value = c.get(entry(cast(void*) c, cast(void*) env));
        if (config.flags & 2) { stdout.rawWrite(value.text()); stdout.rawWrite("\n"); }
        return 0;
    }
    catch (MuLLError e) { stderr.writeln(str(config.source), ":", e.line, ":", e.column, ": ", e.msg); return 1; }
    catch (Exception e) { stderr.writeln(e.msg); return 1; }
}

extern(C):
void mu3ac_site(Context c, ulong line, ulong column) { c.line = line; c.column = column; }
void mu3ac_tick(Context c, ulong line, ulong column)
{
    mu3ac_site(c, line, column); c.guard(() { if (++c.steps > c.config.maxSteps) throw new MuLLError("execution limit exceeded"); });
}
Environment mu3ac_enter(Context c, Environment e) { return c.guard(() { return c.frame(e); }); }
Environment mu3ac_leave(Context c, Environment e) { return e.parent; }
Handle mu3ac_number(Context c, const(char)* s, ulong n)
{
    return c.guard(() {
        auto text = str(View(s, n)); double value; MuLLValue parsed;
        try
        {
            value = text.to!double;
            if (value == 0) foreach (ch; text) { if (ch == 'e' || ch == 'E') break; if (ch >= '1' && ch <= '9') throw new MuLLError("invalid number"); }
            parsed = MuLLValue(value);
        }
        catch (Exception e) { throw new MuLLError("invalid number"); }
        return c.keep(parsed);
    });
}
Handle mu3ac_string(Context c, const(char)* s, ulong n)
{
    import std.string : replace;
    return c.guard(() { return c.keep(MuLLValue(str(View(s, n)).replace("$E?", c.runtime.lastError()))); });
}
Handle mu3ac_literal(Context c, const(char)* s, ulong n)
{
    return c.guard(() { auto v = str(View(s, n)); if (v == "nil") return cast(Handle) 0;
        if (v != "true" && v != "false") throw new MuLLError("invalid Mu3AC literal"); return c.keep(MuLLValue(v == "true")); });
}
Handle mu3ac_load(Context c, Environment e, const(char)* name, ulong n)
{ return c.guard(() { return c.keep(e.lookup(str(View(name, n))).value); }); }
void mu3ac_bind(Context c, Environment e, const(char)* name, ulong n, Handle value, ulong kind)
{ c.guard(() { c.bind(e, str(View(name, n)), c.get(value), kind); }); }
void mu3ac_check_store(Context c, Environment e, const(char)* name, ulong n)
{ c.guard(() { if (!e.lookup(str(View(name, n))).mutable_) throw new MuLLError("cannot assign immutable binding"); }); }
void mu3ac_store(Context c, Environment e, const(char)* name, ulong n, Handle value)
{
    c.guard(() { auto cell = e.lookup(str(View(name, n))); if (!cell.mutable_) throw new MuLLError("cannot assign immutable binding"); cell.value = c.get(value); });
}
void mu3ac_alias(Context c, Environment e, const(View)* names)
{
    c.guard(() { auto name = str(names[0]); if (name in e.values) throw new MuLLError("duplicate binding: " ~ name);
        auto cell = e.lookup(str(names[1])); if (!cell.mutable_) throw new MuLLError("cannot reference immutable binding"); e.values[name] = cell; });
}
Handle mu3ac_unary(Context c, ulong op, Handle a)
{
    return c.guard(() { auto v = c.get(a); if (op == 2) return c.keep(MuLLValue(!v.truth())); return c.keep(MuLLValue(op == 1 ? -v.number() : v.number())); });
}
Handle mu3ac_binary(Context c, ulong op, Handle a, Handle b)
{
    return c.guard(() {
        auto x = c.get(a), y = c.get(b);
        if (op == 5 || op == 6) return c.keep(MuLLValue(op == 5 ? x == y : x != y));
        if (op == 0 && x.type() == "string" && y.type() == "string") return c.keep(MuLLValue(x.stringValue() ~ y.stringValue()));
        if (op == 0 && x.type() == "list" && y.type() == "list") return c.keep(MuLLValue(x.list() ~ y.list()));
        auto l = x.number(), r = y.number();
        if ((op == 3 || op == 4) && r == 0) throw new MuLLError("division by zero");
        switch (op)
        {
            case 0: return c.keep(MuLLValue(l+r)); case 1: return c.keep(MuLLValue(l-r)); case 2: return c.keep(MuLLValue(l*r));
            case 3: return c.keep(MuLLValue(l/r)); case 4: return c.keep(MuLLValue(fmod(l,r)));
            case 7: return c.keep(MuLLValue(l<r)); case 8: return c.keep(MuLLValue(l>r)); case 9: return c.keep(MuLLValue(l<=r)); case 10: return c.keep(MuLLValue(l>=r));
            default: throw new MuLLError("invalid Mu3AC binary operator");
        }
    });
}
Handle mu3ac_member(Context c, Handle a, const(char)* name, ulong n)
{ return c.guard(() { return c.keep(c.get(a).member(str(View(name, n)))); }); }
Handle mu3ac_index(Context c, Handle a, Handle b)
{
    return c.guard(() {
        auto value = c.get(a), key = c.get(b);
        if (value.type() == "mulib")
        {
            if (key.type() == "string") return c.keep(value.member(key.stringValue()));
            if (value.mulib().index is null) throw new MuLLError("mulib value is not indexable"); return c.keep(value.mulib().index(key));
        }
        if (value.type() == "record") return c.keep(value.member(key.stringValue()));
        auto index = key.number(); auto size = value.type() == "string" ? value.stringValue().length : value.list().length;
        if (index < 0 || floor(index) != index || index >= cast(double) size) throw new MuLLError("index out of bounds");
        auto i = cast(size_t) index;
        return c.keep(value.type() == "string" ? MuLLValue(value.stringValue()[i .. i+1]) : value.list()[i]);
    });
}
Handle mu3ac_truth(Context c, Handle a) { return c.guard(() { return cast(Handle) c.get(a).truth(); }); }
Handle mu3ac_call(Context c, Handle a, const(Handle)* args, ulong n)
{ return c.guard(() { auto f = c.get(a); auto values = c.arguments(args, n); return c.keep(f.call(values)); }); }
Handle mu3ac_list(Context c, void* unused, const(Handle)* args, ulong n)
{ return c.guard(() { return c.keep(MuLLValue(c.arguments(args, n))); }); }
Handle mu3ac_record(Context c, const(View)* keys, const(Handle)* args, ulong n)
{
    return c.guard(() { MuLLValue[string] result;
        foreach (i; 0 .. n) { auto key = str(keys[i]); if (key in result) throw new MuLLError("duplicate record key"); result[key] = c.get(args[i]); }
        return c.keep(MuLLValue(result)); });
}
Handle mu3ac_closure(Context c, Environment e, Entry entry, const(View)* params, ulong count)
{
    return c.guard(() { return c.keep(MuLLValue((MuLLValue[] args) {
        mullArity(args, count); if (c.calls >= c.config.maxCalls) throw new MuLLError("call depth limit exceeded");
        ++c.calls; scope(exit) --c.calls;
        auto frame = c.frame(e); foreach (i; 0 .. count) c.bind(frame, str(params[i]), args[i], 1);
        return c.get(entry(cast(void*) c, cast(void*) frame));
    })); });
}
