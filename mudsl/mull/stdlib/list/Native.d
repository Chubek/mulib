module mull.stdlib.list.Native;
import MuDSL;
import std.math : floor, fabs;
private alias V = MuLLValue;
void installList(MuLLRuntime runtime)
{
    V[string] lib;
    lib["Range"] = V((V[] a) {
        mullArity(a,2); auto start = a[0].number(), end = a[1].number();
        if (start != floor(start) || end != floor(end) || fabs(start) > 9007199254740991.0 ||
            fabs(end) > 9007199254740991.0 || end < start || end-start > 1000000) throw new MuLLError("invalid range");
        V[] out_; for (auto i = start; i < end; ++i) out_ ~= V(i); return V(out_);
    });
    lib["Map"] = V((V[] a) {
        mullArity(a,2); V[] out_; foreach (v; a[0].list()) out_ ~= a[1].call([v]); return V(out_);
    });
    lib["Filter"] = V((V[] a) {
        mullArity(a,2); V[] out_; foreach (v; a[0].list()) if (a[1].call([v]).truth()) out_ ~= v; return V(out_);
    });
    lib["Fold"] = V((V[] a) {
        mullArity(a,3); auto result = a[1]; foreach (v; a[0].list()) result = a[2].call([result,v]); return result;
    });
    lib["Push"] = V((V[] a) { mullArity(a,2); return V(a[0].list() ~ a[1]); });
    lib["Set"] = V((V[] a) {
        mullArity(a,3); auto out_ = a[0].list(); auto i = a[1].number();
        if (i < 0 || i != floor(i) || i >= cast(double) out_.length) throw new MuLLError("index out of bounds");
        out_[cast(size_t) i] = a[2]; return V(out_);
    });
    runtime.registerLibrary("list", V(lib));
}
