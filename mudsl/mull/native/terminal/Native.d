module mull.native.terminal.Native;
import MuDSL;
import std.conv : to;
import std.math : floor;
private alias V = MuLLValue;
void installTerminal(MuLLRuntime runtime)
{
    V[string] lib;
    lib["Write"] = V((V[] a) { mullArity(a,1); runtime.stdoutSink(a[0].stringValue()); return V.init; });
    lib["Clear"] = V((V[] a) { mullArity(a,0); runtime.stdoutSink("\x1b[2J\x1b[H"); return V.init; });
    lib["Color"] = V((V[] a) {
        mullArity(a,2); auto n = a[0].number();
        if (n < 0 || n > 107 || n != floor(n)) throw new MuLLError("invalid ANSI color");
        return V("\x1b[" ~ (cast(int) n).to!string ~ "m" ~ a[1].stringValue() ~ "\x1b[0m");
    });
    runtime.registerLibrary("terminal", V(lib));
}
