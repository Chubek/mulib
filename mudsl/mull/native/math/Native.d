module mull.native.math.Native;
import MuDSL;
import std.math : fabs, floor, ceil, sqrt, pow;
import std.algorithm : min, max;
private alias V = MuLLValue;
void installMath(MuLLRuntime runtime)
{
    V[string] lib;
    lib["Abs"] = V((V[] a) { mullArity(a,1); return V(fabs(a[0].number())); });
    lib["Floor"] = V((V[] a) { mullArity(a,1); return V(floor(a[0].number())); });
    lib["Ceil"] = V((V[] a) { mullArity(a,1); return V(ceil(a[0].number())); });
    lib["Sqrt"] = V((V[] a) { mullArity(a,1); return V(sqrt(a[0].number())); });
    lib["Pow"] = V((V[] a) { mullArity(a,2); return V(pow(a[0].number(), a[1].number())); });
    lib["Min"] = V((V[] a) { mullArity(a,2); return V(min(a[0].number(), a[1].number())); });
    lib["Max"] = V((V[] a) { mullArity(a,2); return V(max(a[0].number(), a[1].number())); });
    lib["Pi"] = V(3.141592653589793);
    runtime.registerLibrary("math", V(lib));
}
