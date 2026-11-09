module mull.cli.worker;
import MuDSL;
import mull.mu3ac.Mu3AC;
import mull.stdlib.Stdlib;
import std.stdio : stdin, stdout, stderr, writeln;
import std.string : chomp;
import std.conv : to;

int main(string[] argv)
{
    try
    {
        Options options;
        string stage = "run", file, rules, mu3acRules;
        bool repl; string[] args;
        for (size_t i = 1; i < argv.length; ++i)
        {
            auto key = argv[i];
            string value() { if (++i == argv.length) throw new MuLLError("missing worker option value: " ~ key); return argv[i]; }
            if (key == "--") { args = argv[i+1 .. $]; break; }
            if (key == "--stage") stage = value();
            else if (key == "--file") file = value();
            else if (key == "--name") options.source = value();
            else if (key == "--path") options.paths ~= value();
            else if (key == "--preload") options.preload ~= value();
            else if (key == "--rules") options.mullierRules = rules = value();
            else if (key == "--mu3ac-rules") mu3acRules = value();
            else if (key == "--no-stdlib") options.stdlib = false;
            else if (key == "--print-result") options.printResult = true;
            else if (key == "--no-optimize") options.optimize = false;
            else if (key == "--repl") repl = true;
            else if (key == "--foreign") { auto name = value(), symbol = value(), count = value(); options.foreign ~= Foreign(name, symbol, integer(SExpr.atom(count))); }
            else if (key == "--max-steps") options.maxSteps = integer(SExpr.atom(value()));
            else if (key == "--max-call-depth") options.maxCalls = integer(SExpr.atom(value()));
            else if (key == "--max-frames") options.maxFrames = integer(SExpr.atom(value()));
            else if (key == "--max-source-bytes") options.maxSource = integer(SExpr.atom(value()));
            else if (key == "--max-values") options.maxValues = integer(SExpr.atom(value()));
            else throw new MuLLError("unknown worker option: " ~ key);
        }
        auto vm = new MuLLRuntime([]);
        vm.maxSteps = options.maxSteps; vm.maxCallDepth = options.maxCalls;
        vm.maxFrames = options.maxFrames; vm.maxSourceBytes = options.maxSource; vm.mullier.optimize = options.optimize;
        if (rules.length) vm.mullier.loadDirectory(rules);
        foreach (path; options.paths) vm.addLibraryPath(path);
        if (options.stdlib) install(vm);
        MuLLValue[] values; foreach (arg; args) values ~= MuLLValue(arg); vm.define("Args", MuLLValue(values));
        foreach (f; options.foreign)
        {
            if (!identifier(f.name) || !identifier(f.symbol) || f.arity > 8) throw new MuLLError("invalid foreign declaration");
            vm.define(f.name, MuLLValue((MuLLValue[] unused) { throw new MuLLError("foreign calls require the native backend"); return MuLLValue.init; }));
        }
        if (stage == "run")
        {
            foreach (name; options.preload) if (!vm.loadLibrary(name).truth()) throw new MuLLError(vm.lastError());
            if (repl)
            {
                stderr.writeln("MuLL (D); one complete program per line; :quit exits");
                while (true)
                {
                    stderr.write("mull> "); stderr.flush(); auto source = stdin.readln(); if (!source.length) break;
                    source = source.chomp(); if (source == ":quit") break; if (!source.length) continue;
                    try { writeln("=> ", vm.run(source, "<repl>").text()); }
                    catch (Exception e) { stderr.writeln(e.msg); }
                }
            }
            else { auto result = vm.run(vm.readSource(file), options.source); if (options.printResult) writeln(result.text()); }
            return 0;
        }
        auto source = vm.readSource(file);
        if (stage == "ast") { writeln(vm.parse(source, options.source).dump()); return 0; }
        auto ir = vm.compile(source, options.source);
        if (stage == "check") return 0;
        if (stage == "mullier") { writeln(ir.dump()); return 0; }
        auto program = (new Lowering).compile(ir);
        if (options.optimize)
        {
            auto rewrite = new Rewriter; if (mu3acRules.length) rewrite.loadRules(vm.readSource(mu3acRules)); rewrite.optimize(program);
        }
        if (stage == "mu3ac") stdout.rawWrite(program.dump());
        else if (stage == "asm") stdout.rawWrite((new X86_64).emit(program, options));
        else throw new MuLLError("unknown compilation stage: " ~ stage);
        return 0;
    }
    catch (MuLLError e) { if (e.line) stderr.write("mull-d:", e.line, ":", e.column, ": "); stderr.writeln(e.msg); return 1; }
    catch (Exception e) { stderr.writeln("mull-d: ", e.msg); return 1; }
}
