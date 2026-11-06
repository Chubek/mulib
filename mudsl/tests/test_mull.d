module test_mull;
import MuDSL;
import mull.stdlib.Stdlib;
import std.stdio : writeln, stderr;
import std.algorithm : canFind;
import std.array : replicate;
import std.path : dirName, buildPath;

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
    assert(!vm.loadLibrary("../secret").truth());
    assert(vm.lastError.canFind("invalid library"));
    auto other = new MuLLRuntime;
    try { other.get("x"); assert(false); } catch (MuLLError e) {}
}

int main(string[] args)
{
    try
    {
        if (args.length == 1) { hostChecks(); return 0; }
        if (args.length != 3) return 2;
        auto vm = new MuLLRuntime([args[1]]); install(vm);
        vm.addLibraryPath(buildPath(dirName(args[2]), "packages"));
        auto value = vm.runFile(args[2]);
        writeln("=> ", value.text()); return 0;
    }
    catch (Exception e) { stderr.writeln(e.msg); return 1; }
}
