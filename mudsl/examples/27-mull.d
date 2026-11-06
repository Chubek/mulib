module mull_example;
import MuDSL;
import mull.stdlib.Stdlib;
import std.stdio : stderr;

int main(string[] args)
{
    try
    {
        auto runtime = new MuLL.Runtime([args.length > 1 ? args[1] : "mull/stdlib"]);
        install(runtime);
        runtime.run(q{
            val Printf <- G:LoadLib("printf") or G:DieWith("Import Error: $E?");
            val Terminal <- G:LoadLib("terminal") or G:DieWith("Import Error: $E?");
            Printf:SetMainSink(G:GetStdout());
            Printf:Println("Hello, MuLL!");
        });
        return 0;
    }
    catch (MuLL.Error e) { stderr.writeln(e.msg); return 1; }
}
