module test_mull;
import MuDSL;
import mull.stdlib.Stdlib;
version (MULL_ENABLE_MULIB) import mull.bridge.Mulib.Native;
import std.stdio : writeln, stderr;
import std.algorithm : canFind;
import std.array : replicate;
import std.path : dirName, buildPath;
import std.process : environment;

void mullierChecks()
{
    auto vm = new MuLLRuntime;
    vm.mullier.loadDirectory(buildPath(dirName(__FILE__), "../mull/rewrite"));
    assert(vm.run("return true and (false or +42);").number() == 42);
    assert(vm.mullier.lastSaturation.saturated());
    assert(!vm.mullier.lastIR.dump().canFind("binary"));
    assert(decode_sexpr(encode_sexpr(vm.mullier.lastIR)) == vm.mullier.lastIR);
    vm.mullier.optimize = false;
    assert(vm.run("return true and (false or +42);").number() == 42);
    assert(vm.mullier.lastIR.dump().canFind("binary"));
    vm.define("effect", MuLLValue(0), true);
    foreach (source; ["effect <- 1; val a <- 1; a <- 2;", "effect <- 1; while true { val f <- fn(){break;}; break; }"])
    {
        try { vm.run(source, "analysis.mull"); assert(false); }
        catch (MuLLError e) { assert(e.line == 1 && e.column > 0); }
        assert(vm.get("effect").number() == 0);
    }
    assert(vm.run("val late <- 0; { val f <- fn(){late <- 7;}; var late <- 1; f(); return late; }").number() == 7);
    vm.mullier.optimize = true;
    vm.mullier.limits.nodes = 1;
    assert(vm.run("return true and 9;").number() == 9);
    assert(vm.mullier.lastSaturation.stop == SaturationStop.NodeLimit);
    vm.mullier.limits.nodes = 20000;
    try { vm.mullier.loadDirectory(dirName(__FILE__)); assert(false); }
    catch (Exception e) {}
    assert(vm.run("return false or 8;").number() == 8);
    assert(!vm.mullier.lastIR.dump().canFind("binary"));
    import std.file : tempDir, mkdir, rmdirRecurse, write;
    import std.process : thisProcessID;
    import std.conv : to;
    import mull.mullier.DefaultRules;
    auto directory = buildPath(tempDir(), "mullier-d-" ~ thisProcessID.to!string);
    mkdir(directory);
    scope(exit) rmdirRecurse(directory);
    auto analysis = default_analysis ~ " ".replicate(1024 * 1024 - default_analysis.length);
    write(buildPath(directory, "01.a"), analysis);
    write(buildPath(directory, "02.t"), default_transforms);
    vm.mullier.loadDirectory(directory);
    foreach (source; [analysis ~ " ", "(check unknown)"])
    {
        write(buildPath(directory, "01.a"), source);
        try { vm.mullier.loadDirectory(directory); assert(false); }
        catch (Exception e) { assert(e.msg.canFind("01.a")); }
        assert(vm.run("return false or 8;").number() == 8);
        assert(!vm.mullier.lastIR.dump().canFind("binary"));
        try { vm.run("if false { break; }"); assert(false); }
        catch (MuLLError e) {}
    }
    write(buildPath(directory, "01.a"), "(check lexical)");
    write(buildPath(directory, "02.t"), "(");
    try { vm.mullier.loadDirectory(directory); assert(false); }
    catch (Exception e) {}
    try { vm.run("if false { break; }"); assert(false); }
    catch (MuLLError e) {}
}

void hostChecks()
{
    alias V = MuLLValue;
    auto vm = new MuLLRuntime;
    vm.define("Double", V((V[] a) { mullArity(a,1); return V(a[0].number()*2); }));
    assert(vm.run("var x <- Double(21); ref y <- x; y <- y + 1; x;").number() == 43);
    assert(vm.get("x").number() == 43);
    assert(vm.run("val make <- fn(a) { return fn(b) { return a + b; }; }; val add <- make(10); add(5);").number() == 15);
    assert(vm.get("add").call([V(2)]).number() == 12);
    void fails(string source, string message)
    {
        try { vm.run(source); assert(false); }
        catch (MuLLError e) { assert(e.msg.canFind(message), e.msg); }
    }
    fails("x <- 0; val broken <- ;", "expected identifier");
    assert(vm.get("x").number() == 43);
    fails("add <- 2;", "immutable");
    vm.maxSteps = 30;
    fails("while true {}", "execution limit");
    vm.maxSteps = 100000;
    assert(vm.run("2+2;").number() == 4);
    vm.maxCallDepth = 5;
    fails("val recurse <- fn() { return recurse(); }; recurse();", "call depth");
    assert(vm.run("add(3);").number() == 13);
    vm.maxSourceBytes = 3;
    fails("12345;", "source size");
    vm.maxSourceBytes = 10000;
    fails("(".replicate(300) ~ "1" ~ ")".replicate(300) ~ ";", "nesting limit");
    string nested = "1";
    foreach (i; 0 .. 3) nested = "(" ~ nested ~ "+1".replicate(200) ~ ")";
    fails(nested ~ ";", "IR nesting limit");
    assert(!vm.loadLibrary("../secret").truth());
    assert(vm.lastError.canFind("invalid library"));
    auto other = new MuLLRuntime;
    try { other.get("x"); assert(false); } catch (MuLLError e) {}
    fails("G:MulibWrap(\"Parsec\");", "wrapper is not installed");
    version (MULL_ENABLE_MULIB)
    {
        installMulib(vm); installMulib(other);
        vm.define("foreign", other.run("G:MulibWrap(\"Parsec\"):New(\"uint8_t\", 65);"));
        fails("G:MulibWrap(\"Parsec\"):Byte(foreign);", "another MuLIB runtime");
        auto limited=new MuLLRuntime; installMulib(limited,1);
        try { limited.run("G:MulibWrap(\"Parsec\"):New(\"result\");"); assert(false); }
        catch (MuLLError e) { assert(e.msg.canFind("storage limit"),e.msg); }
    }
}

int main(string[] args)
{
    try
    {
        if (args.length == 1) { hostChecks(); mullierChecks(); return 0; }
        if (args.length != 3) return 2;
        auto vm = new MuLLRuntime([args[1]]); install(vm);
        vm.mullier.optimize = environment.get("MULL_NO_OPTIMIZE", "") == "";
        vm.addLibraryPath(buildPath(dirName(args[2]), "packages"));
        auto value = vm.runFile(args[2]);
        writeln("=> ", value.text()); return 0;
    }
    catch (Exception e) { stderr.writeln(e.msg); return 1; }
}
