#!/usr/bin/env python3
"""Integrated source operator regressions on either scanner implementation."""
import argparse
import json
from pathlib import Path
import tempfile
from textwrap import dedent

from support import run


def command(*argv, status=0):
    result = run(argv)
    assert result.returncode == status, (argv, result.returncode, result.stdout, result.stderr)
    return result


def binary(lexeme="<+>", body="return a * 10 + b;"):
    return f'''operator (lexeme: "{lexeme}", arity: "binary", precedence: 5,
        tightness: 1, binds: "left", formals: a: i64, b: i64) -> i64 {{ {body} }}\n'''


TERNARY = '''
    operator (lexeme: "??", lexeme2: "|>", arity: "ternary", precedence: 2,
        binds: "right", formals: condition: bool, yes: i64, no: i64) -> i64 {
      if condition { return yes; }
      return no;
    }
'''

OWNER = '''
    record Item { n: i64 }
    operator (lexeme: "<+>", arity: "binary", precedence: 5,
        formals: item: own<Item>, delta: i64) -> i64 {
      let payload = take(item);
      return payload["n"] + delta;
    }
'''


class Tests:
    def __init__(self, args, work):
        self.compiler = args.compiler.resolve()
        self.vm = args.vm.resolve()
        self.cc = args.cc
        self.flags = args.cflag
        self.root = args.root.resolve()
        self.work = work
        self.syren = args.syren
        self.strict = ("-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                       "-I", self.root / "runtime", *self.flags)
        self.runtime = self.work / "runtime.o"
        command(self.cc, *self.strict, "-c", self.root / "runtime/on1x_runtime.c", "-o", self.runtime)

    def library_flags(self, with_prelude=False):
        stdlib = ("--stdlib", self.root / "stdlib")
        return stdlib if with_prelude else (*stdlib, "--no-prelude")

    def source(self, name, text):
        source = self.work / (name + ".on1x")
        source.write_text(dedent(text).lstrip("\n"), encoding="utf-8")
        return source

    def compile(self, mode, source, dest, *flags, with_prelude=False):
        command(self.compiler, mode, *self.library_flags(with_prelude), *flags, source, "-o", dest)

    def c(self, name, source, mode="--emit-c", *flags, with_prelude=False):
        artifact = self.work / (name + ".c")
        executable = artifact.with_suffix("")
        self.compile(mode, source, artifact, *flags, with_prelude=with_prelude)
        command(self.cc, *self.strict, artifact, self.runtime, "-lm", "-o", executable)
        return executable

    @staticmethod
    def output(expected, *argv, fault=None):
        result = command(*argv, status=70 if fault else 0)
        assert result.stdout == expected, (argv, result.stdout, expected)
        if fault:
            assert fault in result.stderr, (argv, result.stderr)
        else:
            assert not result.stderr, (argv, result.stderr)

    def products(self, name, source, expected, *, fault=None):
        self.output(expected, self.c(name + "-direct", source), fault=fault)
        for profile in ("debug", "release"):
            base = self.work / (name + "-" + profile)
            flags = ("--opt-profile", profile)
            self.output(expected, self.c(base.name, source, "--emit-ir-c", *flags), fault=fault)
            mir = base.with_suffix(".mir")
            self.compile("--emit-ir", source, mir, *flags)
            self.output(expected, self.compiler, "--ir-input", "--run-ir", mir, fault=fault)
            image = base.with_suffix(".o1vm")
            self.compile("--emit-vm", source, image, *flags)
            command(self.vm, "--verify", image)
            self.output(expected, self.vm, image, fault=fault)

    def reject(self, name, text, *, anchor=None, diagnostic=None, snippet=None, modes=("--emit-c",)):
        source = self.source(name, text)
        for mode in modes:
            dest = self.work / (name + "-" + mode[2:] + ".artifact")
            options = ("--diagnostics", "json", "--color", "never") if self.syren else ()
            result = command(self.compiler, mode, *self.library_flags(), *options, source, "-o", dest, status=1)
            assert not dest.exists() and not result.stdout, (name, mode, result)
            if self.syren:
                records = json.loads(result.stderr)["diagnostics"]
                local = [record for record in records
                         if record["location"]["file"] == str(source) and record["severity"] == "error"]
                assert local, (name, mode, records)
                if diagnostic:
                    assert any(record["id"] == diagnostic for record in local), (name, mode, local)
                if snippet:
                    assert any(snippet in record["message"] for record in local), (name, mode, local)
                if anchor:
                    assert any(source.read_bytes()[record["location"]["offset"]:].startswith(anchor.encode())
                               for record in local), (name, mode, anchor, local)
            else:
                assert str(source) in result.stderr, (name, mode, result.stderr)
                if snippet:
                    assert snippet in result.stderr, (name, mode, result.stderr)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    for argument in ("compiler", "vm", "root", "work"):
        cli.add_argument(argument, type=Path)
    cli.add_argument("cc")
    cli.add_argument("--cflag", action="append", default=[])
    cli.add_argument("--syren", action="store_true")
    args = cli.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="run-", dir=args.work.resolve()) as temp:
        exercise(Tests(args, Path(temp)))
    print("On1x operators: fixity, calls, import identity, formatting, eager order, and rejected programs")


def exercise(t):
    example = t.root / "examples/operators.on1x"
    t.output("123\n33\n42\n33\n42\n", t.c("example", example))

    # Module discovery must see declarations below their first use. All three
    # operands of the ternary are evaluated once, even in the unchosen branch.
    order = t.source("order", '''
        fn mark(value: i64) -> i64 { println(value); return value; }
        fn condition() -> bool { println(1); return false; }
        fn main() {
          println(condition() ?? mark(2) |> mark(3));
          println(mark(4) <+> mark(5));
        }
    ''' + dedent(TERNARY) + binary())
    t.products("order", order, "1\n2\n3\n3\n4\n5\n45\n")
    first_fault = t.source("first_fault", '''
        fn mark(value: i64) -> i64 { println(value); return value; }
        fn condition() -> bool { println(1); return false; }
        fn main() {
          println(condition() ?? mark(2) / mark(0) |> mark(3));
          println(99);
        }
    ''' + dedent(TERNARY))
    t.products("first-fault", first_fault, "1\n2\n0\n", fault="invalid division")

    unary = t.source("unary_generic", '''
        operator (lexeme: "<~>", arity: "unary", formals: n: i64) -> i64 {
          return n + 2;
        }
        operator (lexeme: "~>", arity: "unary", position: "postfix",
            formals: n: i64) -> i64 {
          return n * 10;
        }
        operator<T>(lexeme: "%%", arity: "binary", precedence: 4, formals: a: T, b: T) -> T {
          if true { return b; }
          return a;
        }
        fn add_one(n: i64) -> i64 { return n + 1; }
        fn main() {
          println(<~> add_one(1));
          println((<~> 4) ~>);
          println(0 %% 42);
          println(false %% true);
        }
    ''')
    t.output("4\n60\n42\ntrue\n", t.c("unary", unary))

    t.source("digits", binary() + "fn ordinary() -> i64 { return 1; }\n")
    t.source("arithmetic", binary(body="return a + b;"))
    t.source("api", 'from .digits import (operator("<+>"), ordinary);\n')
    imported = t.source("imported", '''
        from .api import (operator("<+>"), ordinary);
        import arithmetic as Alternate;
        fn main() {
          let op = operator("<+>");
          println(op(ordinary(), 2));
          println(4 <+> 2);
        }
    ''')
    t.output("12\n42\n", t.c("import-run", imported))

    shadow = t.source("shadow", binary() + '''
        fn main() {
          println(1 <+> 2 <+> 3);
          {
            infixr 0 <+>;
            println(1 <+> 2 <+> 3);
            operator (lexeme: "<+>", arity: "binary", precedence: 5, tightness: 1,
                formals: a: i64, b: i64) -> i64 { return a * 100 + b; }
            println(1 <+> 2 <+> 3);
            println(1 + 2 <+> 3);
          }
          println(1 <+> 2 <+> 3);
        }
    ''')
    t.output("123\n33\n10203\n204\n123\n", t.c("shadow-run", shadow))
    t.source("local_library", '''
        fn calculate() -> i64 {
          operator (lexeme: "%%", arity: "binary",
              formals: a: i64, b: i64) -> i64 { return a + b; }
          return 10 %% 20;
        }
    ''')

    owner = t.source("owned", dedent(OWNER) + '''
        fn main() {
          let value = box(record<Item>{n: 40});
          println(move(value) <+> 2);
        }
    ''')
    t.output("42\n", t.c("owned-run", owner))
    borrower = t.source("borrowed", '''
        record Item { n: i64 }
        operator (lexeme: "%%", arity: "binary", precedence: 5,
            formals: item: borrow Item, delta: i64) -> i64 {
          return item["n"] + delta;
        }
        fn main() {
          let value = box(record<Item>{n: 40});
          println(value %% 2);
          { let view = borrow value; println(view["n"]); }
          drop(value);
        }
    ''')
    t.output("42\n40\n", t.c("borrowed-run", borrower))
    t.reject("owner_requires_move", dedent(OWNER) + '''
        fn main() {
          let value = box(record<Item>{n: 40});
          println(value <+> 2);
        }
    ''', snippet="move", modes=("--emit-c", "--emit-ir", "--emit-vm"))
    t.reject("owner_use_after_move", dedent(OWNER) + '''
        fn main() {
          let value = box(record<Item>{n: 40});
          println(move(value) <+> 2);
          drop(value);
        }
    ''', snippet="use after move", modes=("--emit-c", "--emit-ir", "--emit-vm"))

    messy = t.source("messy", 'operator(lexeme:"<+>",arity:"binary",precedence:5,'
                     'formals:a:i64,b:i64)->i64{return a*10+b;}'
                     'fn main(){# scoped\ninfixr 3 <+>;println(1<+>2<+>3);}\n')
    formatted = command(t.compiler, "fmt", messy).stdout
    assert "infixr 3 <+>;\n" in formatted and "1 <+> 2 <+> 3" in formatted, formatted
    clean = t.source("formatted", formatted)
    command(t.compiler, "fmt", "--check", clean)
    t.output("33\n", t.c("formatted-run", clean))

    staged = t.source("staged_operator", binary("%%") + '''
        macro reorder {
          rule (ast_binary($customop, $a: Expression, $b: Expression)) =>
            expression { ast_binary($customop, $b, $a) };
        }
        macro rows {
          rule ([ast_binary($op: CustomOperator, $a: Expression, $b: Expression) ...] ...) =>
            block { println(ast_binary($op, $b, $a))... ...; };
        }
        fn main() {
          println(run(quote { reorder!(2 %% 4) }));
          rows!([2 %% 4, 5 %% 6], [], [7 %% 8]);
        }
    ''')
    t.output("42\n42\n65\n87\n", t.c("staged-run", staged))

    # Validate stable diagnostic identities and their anchors, not just status.
    t.reject("reserved", binary("+") + "fn main() {}", anchor='"+"',
             diagnostic="ON1X-OPERATOR-SPELLING")
    t.reject("overlong_lexeme", binary("%%%%%%%") + "fn main() {}", anchor='"%%%%%%%"',
             diagnostic="ON1X-OPERATOR-SPELLING")
    t.reject("unknown_infix", 'fn main() { println("🙂"); println(4 <+> 2); }', anchor="<+>",
             diagnostic="ON1X-OPERATOR-UNBOUND")
    t.reject("missing_ternary_separator", TERNARY + "fn main() { println(true ?? 1); }",
             anchor=");", diagnostic="ON1X-OPERATOR-TERNARY")
    t.reject("missing_export", 'from .digits import (operator("%%")); fn main() {}',
             diagnostic="ON1X-OPERATOR-IMPORT")
    t.reject("invalid_arity", binary().replace('"binary"', '"nilary"') + "fn main() {}",
             anchor='"nilary"', diagnostic="ON1X-OPERATOR-ARITY")
    t.reject("invalid_tightness", binary().replace("tightness: 1", "tightness: 10") + "fn main() {}",
             anchor="10,", diagnostic="ON1X-OPERATOR-FIXITY")
    overloaded_position = binary() + '''
        operator (lexeme: "<+>", arity: "unary", formals: n: i64) -> i64 {
          return n + 1;
        }
    '''
    positioned = t.source("positioned", overloaded_position + '''
        fn main() { println((<+> 40) <+> 1); }
    ''')
    t.output("411\n", t.c("positioned-run", positioned))
    t.reject("ambiguous_reference", overloaded_position + '''
        fn main() { let action = operator("<+>"); }
    ''', anchor='"<+>"', diagnostic="ON1X-OPERATOR-AMBIGUOUS")
    for name, text in (
        ("wrong_formals", binary().replace(", b: i64", "") + "fn main() {}"),
        ("invalid_precedence", binary().replace("precedence: 5", "precedence: 10") + "fn main() {}"),
        ("duplicate_attribute", binary().replace('arity:', 'lexeme: "%%", arity:') + "fn main() {}"),
        ("duplicate_operator", binary() * 2 + "fn main() {}"),
        ("malformed_reference", 'fn main() { let f = operator(7); }'),
        ("unknown_reference", 'fn main() { let f = operator("<+>"); }'),
        ("missing_rhs", binary() + "fn main() { println(1 <+>); }"),
        ("missing_ternary_operand", TERNARY + "fn main() { println(true ?? 1 |>); }"),
        ("fixity_of_unknown", "infix 4 <+>; fn main() {}"),
        ("ambiguous_import", 'from .digits import operator("<+>");\n'
                             'from .arithmetic import operator("<+>"); fn main() {}'),
        ("local_not_exported", 'from .local_library import operator("%%"); fn main() {}'),
        ("local_noncapturing", '''
            fn main() {
              let delta = 7;
              operator (lexeme: "%%", arity: "binary",
                  formals: a: i64, b: i64) -> i64 { return a + b + delta; }
              println(1 %% 2);
            }
        '''),
        ("operator_chain_depth", binary() + "fn main() { println(" + "1 <+> " * 350 + "1); }"),
    ):
        t.reject(name, text)

    for name, fixture, with_prelude in (
        ("stdlib-pervasive", "operators/tests/pervasive.on1x", True),
        ("stdlib-selective", "operators/tests/selective.on1x", False),
        ("stdlib-utils", "operator/tests/utils.on1x", False),
    ):
        source = t.root / "stdlib" / fixture
        t.output("", t.c(name, source, with_prelude=with_prelude))
    for name, diagnostic, anchor, snippet in (
        ("selective_hidden", "ON1X-OPERATOR-UNBOUND", "|>", "no visible trailing operator"),
        ("nominal_rejected", "ON1X-TYPE", "not_a_result", "incompatible nominal types"),
    ):
        fixture = t.root / "stdlib/operators/tests" / (name + ".on1x")
        t.reject(name, fixture.read_text(encoding="utf-8"), diagnostic=diagnostic, anchor=anchor, snippet=snippet)


if __name__ == "__main__":
    main()
