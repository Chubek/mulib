module mull.native.printf.Native;
import MuDSL;
private alias V = MuLLValue;

string mullFormat(V[] args)
{
    if (!args.length) throw new MuLLError("Format requires a template");
    auto s = args[0].stringValue(); string out_; size_t arg = 1;
    for (size_t i; i < s.length; ++i)
    {
        if (s[i] == '{' || s[i] == '}')
        {
            if (i + 1 < s.length && s[i+1] == s[i]) { out_ ~= s[i++]; continue; }
            if (s[i] == '{' && i + 1 < s.length && s[i+1] == '}')
            {
                if (arg == args.length) throw new MuLLError("not enough format arguments");
                out_ ~= args[arg++].text(); ++i; continue;
            }
            throw new MuLLError("invalid format template");
        }
        out_ ~= s[i];
    }
    if (arg != args.length) throw new MuLLError("too many format arguments");
    return out_;
}
void installPrintf(MuLLRuntime runtime)
{
    auto sink = runtime.get("G").member("GetStdout").call(); V[string] lib;
    bool writing;
    void write(string text)
    {
        if (writing) throw new MuLLError("recursive output sink");
        writing = true; scope(exit) writing = false;
        auto selected = sink; selected.call([V(text)]);
    }
    lib["SetMainSink"] = V((V[] a) {
        mullArity(a,1); if (a[0].type() != "function") throw new MuLLError("sink must be a function");
        sink = a[0]; return V.init;
    });
    lib["Format"] = V((V[] a) { return V(mullFormat(a)); });
    lib["Printf"] = V((V[] a) { write(mullFormat(a)); return V.init; });
    lib["Print"] = V((V[] a) {
        string s; foreach (v; a) s ~= v.text(); write(s); return V.init;
    });
    lib["Println"] = V((V[] a) {
        string s; foreach (v; a) s ~= v.text(); write(s ~ "\n"); return V.init;
    });
    runtime.registerLibrary("printf", V(lib));
}
