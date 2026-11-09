module mull.native.text.Native;
import MuDSL;
import std.string : split, replace;
private alias V = MuLLValue;
void installText(MuLLRuntime runtime)
{
    V[string] lib;
    lib["Split"] = V((V[] a) {
        mullArity(a,2); auto sep = a[1].stringValue(); if (!sep.length) throw new MuLLError("empty separator");
        V[] out_; foreach (s; a[0].stringValue().split(sep)) out_ ~= V(s); return V(out_);
    });
    lib["Join"] = V((V[] a) {
        mullArity(a,2); string out_; auto sep = a[1].stringValue();
        foreach (i, v; a[0].list()) { if (i) out_ ~= sep; out_ ~= v.stringValue(); } return V(out_);
    });
    lib["Replace"] = V((V[] a) {
        mullArity(a,3); auto from = a[1].stringValue(); if (!from.length) throw new MuLLError("empty search string");
        return V(a[0].stringValue().replace(from, a[2].stringValue()));
    });
    lib["Upper"] = V((V[] a) {
        mullArity(a,1); auto s = a[0].stringValue().dup;
        foreach (ref char c; s) if (c >= 'a' && c <= 'z') c -= 32;
        return V(s.idup);
    });
    lib["Lower"] = V((V[] a) {
        mullArity(a,1); auto s = a[0].stringValue().dup;
        foreach (ref char c; s) if (c >= 'A' && c <= 'Z') c += 32;
        return V(s.idup);
    });
    runtime.registerLibrary("text", V(lib));
}
