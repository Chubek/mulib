"""Native ABI fixtures for test_scalar_abi.py (real verified On1x MIR input)."""
import subprocess

from test_scalar_abi import exercise, literal, run, runtime_source

AT = '(at "scalar-abi.mir" 0 1 1)'


class Module:
    def __init__(self):
        self.next_id = 1
        self.functions = []

    def fresh(self):
        self.next_id += 1
        return self.next_id

    def function(self, count, kind):
        fid, block, effect = self.fresh(), self.fresh(), self.fresh()
        t = "i64" if kind == "sum" else "u64"
        parameters = [self.fresh() for _ in range(count)]
        values = " ".join(f"(value {p} {t} {AT})" for p in parameters)
        result_type = "i64" if kind == "sum" else "(tuple (" + " ".join([t] * count) + "))"
        instructions = []
        if kind == "sum":
            result = parameters[0]
            for p in parameters[1:]:
                dst, next_effect = self.fresh(), self.fresh()
                instructions.append(
                    f"(instruction {dst} add.checked i64 ({result} {p}) 0 0 "
                    f"{effect} {next_effect} {AT})")
                effect, result = next_effect, dst
        else:
            result = self.fresh()
            instructions.append(
                f"(instruction {result} tuple {result_type} "
                f"({' '.join(map(str, parameters))}) 0 0 0 0 {AT})")
        # Block effect is the initially allocated id, not the final token.
        start_effect = block + 1
        self.functions.append(
            f'(function {fid} "{kind}{count}" {result_type} {AT} () '
            f'((block {block} {start_effect} {AT} ({values}) '
            f'({" ".join(instructions)}) '
            f'(exit {self.fresh()} return {result} {effect} () {AT}))))')
        return fid

    def relay(self, count, target):
        fid, block, effect = self.fresh(), self.fresh(), self.fresh()
        parameters = [self.fresh() for _ in range(count)]
        values = " ".join(f"(value {p} u64 {AT})" for p in parameters)
        result_type = "(tuple (" + " ".join(["u64"] * count) + "))"
        result, next_effect = self.fresh(), self.fresh()
        self.functions.append(
            f'(function {fid} "relay{count}" {result_type} {AT} () '
            f'((block {block} {effect} {AT} ({values}) '
            f'((instruction {result} call {result_type} '
            f'({" ".join(map(str, parameters))}) 0 {target} '
            f'{effect} {next_effect} {AT})) '
            f'(exit {self.fresh()} return {result} {next_effect} () {AT}))))')
        return fid

    def nested_identity(self):
        fid, block, effect, parameter = [self.fresh() for _ in range(4)]
        t = "(tuple (u64 (tuple (f32 f64)) bool))"
        self.functions.append(
            f'(function {fid} "nested" {t} {AT} () '
            f'((block {block} {effect} {AT} ((value {parameter} {t} {AT})) () '
            f'(exit {self.fresh()} return {parameter} {effect} () {AT}))))')
        return fid

    def adapter(self, name, args, result_type):
        fid, block, effect = self.fresh(), self.fresh(), self.fresh()
        parameters = [self.fresh() for _ in args]
        values = " ".join(f"(value {p} {t} {AT})" for p, t in zip(parameters, args))
        result, next_effect = self.fresh(), self.fresh()
        op, descriptor = "numeric", f' "{name}"'
        if name.endswith("_f64") and name.split("_")[0] in (
                "add", "sub", "mul", "div", "neg", "eq", "ne", "lt", "le", "gt", "ge"):
            op, descriptor = "f64." + name.split("_")[0], ""
        if name.endswith("_i64") and name.split("_")[0] in (
                "add", "sub", "mul", "div", "mod", "neg"):
            op, descriptor = name.split("_")[0] + ".checked", ""
        self.functions.append(
            f'(function {fid} "{name}" {result_type} {AT} () '
            f'((block {block} {effect} {AT} ({values}) '
            f'((instruction {result} {op} {result_type} '
            f'({" ".join(map(str, parameters))}) 0 0 '
            f'{effect} {next_effect} {AT}{descriptor})) '
            f'(exit {self.fresh()} return {result} {next_effect} () {AT}))))')
        return fid

    def text(self):
        return f'(on1x.mir 3 1 ({" ".join(self.functions)}))\n'


def native_boundaries(tool, build, cc, flags, objects, sanitize, generator):
    module = Module()
    declarations = []
    checks = []
    # Explicit regression for the historical 64-carrier boundary, plus the
    # current pinned ARCHON_MIR_MAX_PARAMS=128 boundary. Tuple results consume
    # one argument for the explicit caller-owned buffer pointer.
    for count in (64, 128):
        fid = module.function(count, "sum")
        declarations.append(f"extern uint64_t on1x_mir_f{fid}("
                            + ",".join(["uint64_t"] * count) + ");")
        args = [(-i if i % 2 else i) for i in range(count)]
        checks.append(f"check(on1x_mir_f{fid}("
                      + ",".join(literal(x) for x in args)
                      + f"),{literal(sum(args))});")
        n = count - 1
        pack = module.function(n, "pack")
        relay = module.relay(n, pack)
        args = [((1 << 63) | i * 0x1020304050607) for i in range(n)]
        for fid in (pack, relay):
            declarations.append(f"extern void on1x_mir_f{fid}(uint64_t *,"
                                + ",".join(["uint64_t"] * n) + ");")
            if n < 127:
                checks.append(f"out.words[{n}]=29;")
            checks.append(f"on1x_mir_f{fid}(out.words,"
                          + ",".join(literal(x) for x in args) + ");")
            for i, value in enumerate(args):
                checks.append(f"check(out.words[{i}],{literal(value)});")
            checks.append("check(out.before,17); check(out.after,23);")
            if n < 127:
                checks.append(f"check(out.words[{n}],29);")
    nested = module.nested_identity()
    declarations.append(f"extern void on1x_mir_f{nested}(uint64_t *,const uint64_t *);")
    checks += [
        "const uint64_t nested[4]={UINT64_MAX,UINT64_C(0x80000000),"
        "UINT64_C(0x7ff8000000000000),1};",
        "out.words[4]=31;",
        f"on1x_mir_f{nested}(out.words,nested);",
        "for(unsigned i=0;i<4;++i) check(out.words[i],nested[i]);",
        "check(out.before,17); check(out.after,23); check(out.words[4],31);",
    ]
    # Run the same typed-runtime oracle/edge/trap corpus across actual native
    # functions as well. These are MIR functions, not C facsimiles of native code.
    oracle, traps, value_count = runtime_source(generator)
    for name, (args, result) in sorted(generator.rows.items()):
        fid = module.adapter(name, args, result)
        oracle = oracle.replace(f"on1x_archon_{name}(", f"on1x_mir_f{fid}(")
    extra_declarations, extra_checks = [], []
    for width in (8, 16, 32, 64):
        maximum = (1 << width) - 1
        for op in ("eq", "ne", "lt", "le", "gt", "ge"):
            fid = module.adapter(f"{op}_u{width}", [f"u{width}"] * 2, "bool")
            extra_declarations.append(
                f"extern uint64_t on1x_mir_f{fid}(uint64_t,uint64_t);")
            for a, b in ((0, maximum), (maximum, 0), (maximum, maximum),
                         (0, 0), (maximum >> 1, 1 << (width - 1))):
                expected = dict(eq=a == b, ne=a != b, lt=a < b, le=a <= b,
                                gt=a > b, ge=a >= b)[op]
                extra_checks.append(
                    f'check(on1x_mir_f{fid}({literal(a)},{literal(b)}),'
                    f'{int(expected)},"unsigned {op} u{width}");')
    for op, values, expected in (
            ("add", [-9, 7], -2), ("sub", [-9, 7], -16),
            ("mul", [-9, 7], -63), ("div", [-7, 3], -2),
            ("mod", [-7, 3], -1), ("neg", [-(1 << 63) + 1], (1 << 63) - 1)):
        fid = module.adapter(f"{op}_i64", ["i64"] * len(values), "i64")
        extra_declarations.append(f"extern uint64_t on1x_mir_f{fid}("
                                  + ",".join(["uint64_t"] * len(values)) + ");")
        extra_checks.append(f'check(on1x_mir_f{fid}('
                            + ",".join(literal(x) for x in values)
                            + f'),{literal(expected)},"signed {op}");')
        oracle = oracle.replace(f"on1x_{op}_i64(", f"on1x_mir_f{fid}(")
    oracle = oracle.replace("int main(", "\n".join(extra_declarations) + "\nint main(")
    oracle = oracle.replace("_mm_setcsr(0x1f80u);\n",
                            "_mm_setcsr(0x1f80u);\n" + "\n".join(extra_checks) + "\n", 1)
    value_count += len(extra_checks)
    source = build / "native.mir"
    assembly = build / "native.s"
    # Each adapter imports one symbol too; use separately compiled modules to
    # stay below the real backend's module-symbol budget. The boundary pack/
    # relay functions remain together in the first module.
    native_objects = []
    functions = module.functions
    for start in range(0, len(functions), 64):
        module.functions = functions[start:start + 64]
        source.write_text(module.text())
        run([tool, "--no-entry", "--ir-input", source, "-o", assembly])
        first = assembly.read_bytes()
        run([tool, "--no-entry", "--ir-input", source, "-o", assembly])
        assert first == assembly.read_bytes(), "nondeterministic native ABI assembly"
        obj = build / f"native-{start}.o"
        run([cc, "-c", assembly, "-o", obj])
        native_objects.append(obj)
    caller = build / "native-caller.c"
    caller.write_text(
        '#include <stdint.h>\n#include <stdlib.h>\n#include <stdio.h>\n'
        'static void check(uint64_t a,uint64_t b) {if(a!=b) {'
        'fprintf(stderr,"native ABI: %llx != %llx\\n",'
        '(unsigned long long)a,(unsigned long long)b);exit(1);}}\n'
        + "\n".join(declarations)
        + "\nint main(void) {\n"
        "struct { uint64_t before,words[127],after; } out={17,{0},23};\n"
        + "\n".join(checks) + "\nreturn 0;\n}\n")
    caller_object = build / "native-caller.o"
    run([cc, *flags, "-c", caller, "-o", caller_object])
    exe = build / "native-test"
    run([cc, *flags, *(["-no-pie"] if sanitize else []),
         caller_object, *native_objects, *objects, "-lm", "-o", exe])
    run([exe])
    caller.write_text(oracle)
    run([cc, *flags, "-c", caller, "-o", caller_object])
    run([cc, *flags, *(["-no-pie"] if sanitize else []),
         caller_object, *native_objects, *objects, "-lm", "-o", exe])
    exercise(exe, traps)
    # The first excessive tuple signature is 128 explicit words + result ptr.
    # The verifier must reject the real MIR and preserve an existing output.
    for count, kind in ((129, "sum"), (128, "pack")):
        excessive = Module()
        excessive.function(count, kind)
        source.write_text(excessive.text())
        assembly.write_text("existing output\n")
        result = subprocess.run(
            [str(tool), "--no-entry", "--ir-input", str(source), "-o", str(assembly)],
            text=True, capture_output=True, timeout=30)
        assert result.returncode != 0, (count, kind, result.stdout, result.stderr)
        assert "argument" in result.stderr or "parameter" in result.stderr, result.stderr
        assert assembly.read_text() == "existing output\n"
    print("native: separate C/assembly TUs, 64/128 scalar args, "
          "63/127 tuple args + hidden result, nested tuples, "
          "native/native calls, excess-signature rejection")
    print(f"native numeric imports: {len(generator.rows)} functions, "
          f"{value_count} value comparisons, {len(traps)} exact traps")
