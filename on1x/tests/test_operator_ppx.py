#!/usr/bin/env python3
"""Exercise trusted operator readmission and rejection through compiler PPX."""
import argparse
from pathlib import Path
from textwrap import dedent
import tempfile

from support import run


def exercise(compiler, root, cc, flags, work):
    processor = work / "operator-ppx.py"
    processor.write_text("""#!/usr/bin/env python3
import os
import sys
from copy import deepcopy
sys.path.insert(0, """ + repr(str(root / "stdppx")) + """)
from _support.ast import (binop, block, fixity_directive, ident, integer, loc,
    mk, operator_binary, operator_declaration, operator_header, param, rewrite, tag, typ)
from _support.sexpr import Atom, dumps, loads

mode = os.environ["ON1X_PPX_LABEL"]
wire = sys.stdin.buffer.read()
assert b"@operator_" not in wire, wire
tree = loads(wire)

def find(node, target):
    if tag(node) == target:
        return node
    if isinstance(node, list):
        for child in node:
            found = find(child, target)
            if found is not None:
                return found

original_application = deepcopy(find(tree, "OperatorBinary"))

def replace(node):
    if mode == "Change" and tag(node) == "DecimalLiteral":
        if node[1] == "10":
            return integer(100, node[2])
        if node[1] == "99":
            return operator_binary("<+>", integer(1, node[2]), integer(2, node[2]), loc())
    if mode == "Forge" and tag(node) == "DecimalLiteral" and node[1] == "99":
        foreign_location = next(d[1][-1] for d in tree[2] if tag(d) == "OperatorDeclaration")
        return operator_binary("<+>", integer(1, node[2]), integer(2, node[2]), foreign_location)
    if mode == "Relocate" and tag(node) == "DecimalLiteral" and node[1] == "99":
        return deepcopy(original_application)
    if mode == "Interface" and tag(node) == "OperatorHeader":
        node[1] = "%%"
    if mode in ("Fixity", "HeaderOnly", "Opaque") and tag(node) == "OperatorHeader":
        node[6], node[7] = Atom("9"), "right"
    if mode in ("Fixity", "DirectiveOnly") and tag(node) == "FixityDirective":
        node[2], node[3] = Atom("9"), "right"
    if mode == "Rank" and tag(node) == "OperatorHeader":
        node[5] = Atom("7")
    if mode == "UnaryRank" and tag(node) == "OperatorHeader" and node[4] == "postfix":
        node[5] = Atom("7")
    if mode == "Opaque" and tag(node) == "OperatorBinary" and tag(node[2]) == "OperatorBinary":
        # Keep the left-grouped shape but explicitly replace part of the source chain.
        node[2] = operator_binary(node[2][1], node[2][2], node[2][3], loc())
    if mode == "OpaqueReorder" and tag(node) == "OperatorBinary" and tag(node[2]) == "OperatorBinary":
        left = node[2]
        return operator_binary(left[1], left[2],
                               operator_binary(node[1], left[3], node[3], node[4]), left[4])
    if mode in ("Signature", "BadSignature") and tag(node) == "OperatorDeclaration":
        node[4] = typ("Bool")
        if mode == "Signature":
            node[5] = block([mk("Return", mk("BooleanLiteral", Atom("false"), node[5][-1]),
                                node[5][-1])], node[5][-1])
    if mode == "RenameFormal":
        if tag(node) == "OperatorDeclaration":
            node[3][0][1] = "first"
        elif tag(node) == "Identifier" and node[1] == "a":
            node[1] = "first"
    if mode in ("Install", "BadRank", "BadBody") and tag(node) == "DecimalLiteral" and node[1] == "99":
        return operator_binary("%%", integer(40, node[2]), integer(2, node[2]), loc())
    if mode == "SynthesizePervasive" and tag(node) == "DecimalLiteral" and node[1] == "99":
        return operator_binary("|>", integer(41, node[2]), ident("next", node[2]), loc())
    return node

tree = rewrite(tree, replace)
if mode == "AppendDirective":
    authored = next(d for d in tree[2] if tag(d) == "FixityDirective")
    tree[2].append(fixity_directive(authored[1], 9, "right", authored[-1]))
if mode in ("Install", "BadRank", "BadBody"):
    at, i64 = loc(), typ("I64")
    header = operator_header("%%", "", "binary", "infix", 5, 10 if mode == "BadRank" else 0, "left", at)
    operands = [param("a", i64, at), param("b", i64, at)]
    value = (mk("BooleanLiteral", Atom("false"), at) if mode == "BadBody"
             else binop("+", ident("a", at), ident("b", at), at))
    body = block([mk("Return", value, at)], at)
    tree[2].extend([operator_declaration(header, [], operands, i64, body),
                    fixity_directive("%%", 1, "left", at)])
sys.stdout.buffer.write(dumps(tree))
""", encoding="utf-8")

    def source(name, text):
        p = work / (name + ".on1x")
        p.write_text(dedent(text), encoding="utf-8")
        return p

    def command(*argv, succeeds=True):
        result = run(argv)
        assert (result.returncode == 0) == succeeds, (argv, result.stdout, result.stderr)
        return result

    runtime = work / "runtime.o"
    cflags = ("-std=c11", "-I", root / "runtime", *flags)
    command(cc, *cflags, "-c", root / "runtime/on1x_runtime.c", "-o", runtime)

    def transform(src, mode, expected=None, *, prelude=False, failure=None, repeat=False):
        output = src.with_suffix(".c")
        source_flags = () if prelude else ("--no-prelude",)
        result = command(compiler, "--emit-c", *source_flags, "--ppx", mode, processor,
                         src, "-o", output, succeeds=failure is None)
        if failure:
            assert failure in result.stderr and not output.exists(), result.stderr
            return
        if repeat:
            canonical = output.read_bytes()
            command(compiler, "--emit-c", *source_flags, "--ppx", mode, processor, src, "-o", output)
            assert output.read_bytes() == canonical, "PPX regrouping was not deterministic"
        artifact = src.with_suffix(".exe")
        obj = src.with_suffix(".o")
        command(cc, *cflags, "-c", output, "-o", obj)
        command(cc, *flags, obj, runtime, "-lm", "-o", artifact)
        result = command(artifact)
        assert result.stdout == expected, result.stdout

    source("library", """
        operator(lexeme: "<+>", arity: "binary", precedence: 5,
                 formals: a: i64, b: i64) -> i64 { return a * 10 + b; }
        macro from_library { rule() => expression { operator("<+>")(1, 2) }; }
    """)
    transform(source("hygiene", """
        from .library import from_library;
        operator(lexeme: "<+>", arity: "binary", precedence: 5,
                 formals: a: i64, b: i64) -> i64 { return a - b; }
        fn main() {
          println(from_library!());
          println(1 <+> 2);
          println(99);
        }
    """), "Change", "102\n-1\n-1\n")
    transform(source("forgery", """
        from .library import from_library;
        fn main() { println(99); }
    """), "Forge", failure="not visible in its owning declaration")
    transform(source("interface", """
        from .library import from_library;
        fn main() {}
    """), "Interface", failure="cannot change the interface")
    transform(source("fixity", """
        operator(lexeme: "<+>", arity: "binary", precedence: 5,
                 formals: a: i64, b: i64) -> i64 { return a * 10 + b; }
         infix 1 <+>;
         fn main() { println(1 <+> 2 <+> 3); }
     """), "Fixity", "33\n")
    digits = """
        operator(lexeme: "<+>", arity: "binary", precedence: 5,
                 formals: a: i64, b: i64) -> i64 { return a * 10 + b; }
    """
    transform(source("grouping", digits + """
        fn main() {
          println(1 <+> 2 * 3);
          println(1 <+> (2 * 3));
          println((1 <+> 2) * 3);
          println((1 <+> 2 * 3));
        }
    """), "Rank", "36\n16\n36\n36\n")
    transform(source("opaque", digits + """
        fn main() { println(1 <+> 2 <+> 3); }
    """), "Opaque", "123\n")
    transform(source("regroup_order", digits + """
        fn mark(n: i64) -> i64 { println(n); return n; }
        fn main() { println(mark(1) <+> mark(2) <+> mark(3)); }
    """), "HeaderOnly", "1\n2\n3\n33\n")
    transform(source("lexical_fixity", digits + """
        fn main() { println(1 <+> 2 <+> 3); after(); }
        infix 1 <+>;
        fn after() {
          println(1 <+> 2 <+> 3);
          {
            println(1 <+> 2 <+> 3);
            infix 0 <+>;
            println(1 <+> 2 <+> 3);
            println((1 <+> 2) <+> 3);
            operator(lexeme: "<+>", arity: "binary", precedence: 5,
                     formals: a: i64, b: i64) -> i64 { return a * 100 + b; }
            println(1 <+> 2 <+> 3);
            infix 0 <+>;
            println(1 <+> 2 <+> 3);
          }
          println(1 <+> 2 <+> 3);
        }
    """), "DirectiveOnly", "123\n33\n33\n33\n123\n10203\n303\n33\n")
    transform(source("active_override", digits + """
        fn main() { println(1 <+> 2 <+> 3); after(); }
        infix 0 <+>;
        fn after() { println(1 <+> 2 <+> 3); }
    """), "HeaderOnly", "33\n123\n")
    transform(source("appended_forged_directive", digits + """
        infix 0 <+>;
        fn main() { println(1 <+> 2 <+> 3); }
    """), "AppendDirective", "123\n")
    long_chain = " <+> ".join(["1"] * 70)
    transform(source("long_fixity", digits + f"fn main() {{ println({long_chain}); }}"),
              "HeaderOnly", "691\n", repeat=True)
    transform(source("unary_fixity", """
        operator(lexeme: "<~>", arity: "unary", precedence: 6,
                 formals: n: i64) -> i64 { return n * 10; }
        operator(lexeme: "~>", arity: "unary", position: "postfix", precedence: 5,
                 formals: n: i64) -> i64 { return n + 1; }
        fn main() {
          println(<~> 2 ~>);
          println((<~> 2) ~>);
          println(<~> (2 ~>));
        }
    """), "UnaryRank", "30\n21\n30\n")
    transform(source("core_suffix", """
        operator<T>(lexeme: "<~>", arity: "unary", precedence: 6,
                 formals: value: T) -> T { println(1); return value; }
        operator<T>(lexeme: "~>", arity: "unary", position: "postfix", precedence: 5,
                 formals: value: T) -> T { println(2); return value; }
        fn identity(value: i64) -> i64 { return value; }
        fn main() {
          println(<~> identity ~> (3));
          println((<~> identity) ~> (3));
        }
    """), "UnaryRank", "2\n1\n3\n1\n2\n3\n")
    transform(source("ternary_fixity", """
        operator(lexeme: "??", lexeme2: "|>", arity: "ternary", precedence: 2,
                 formals: a: i64, b: i64, c: i64) -> i64 { return a * 100 + b * 10 + c; }
        fn main() {
          println(1 ?? 2 |> 3 ?? 4 |> 5);
          println((1 ?? 2 |> 3) ?? 4 |> 5);
          println(1 ?? 2 ?? 3 |> 4 |> 5);
        }
    """), "HeaderOnly", "465\n12345\n2445\n")
    transform(source("signature", digits + "fn main() { println(1 <+> 2); }"),
              "Signature", "false\n")
    transform(source("formal", digits + "fn main() { println(1 <+> 2); }"),
              "RenameFormal", "12\n")
    transform(source("bad_signature", digits + "fn main() { println(1 <+> 2); }"),
              "BadSignature", failure="expected bool")
    transform(source("foreign_signature", """
        from .library import from_library;
        fn main() { println(from_library!()); }
    """), "Signature", failure="cannot change the interface")
    transform(source("install", "fn main() { println(99); }"), "Install", "42\n")
    source("plain_dependency", "fn value() -> i64 { return 1; }")
    transform(source("entry_installation", """
        from .plain_dependency import value;
        fn main() { println(99); }
    """), "Install", "42\n")
    source("no_source_operators", "fn value() -> i64 { return 99; }")
    transform(source("foreign_installation", """
        from .no_source_operators import value;
        fn main() { println(value()); }
    """), "Install", failure="not visible in its owning declaration")
    transform(source("bad_rank", "fn main() { println(99); }"),
              "BadRank", failure="operator rank must be in 0..9")
    transform(source("bad_body", "fn main() { println(99); }"),
              "BadBody", failure="expected i64")
    local_source = """
        fn main() {
          if true {
            operator(lexeme: "<+>", arity: "binary", precedence: 5,
                     formals: a: i64, b: i64) -> i64 { return a + b; }
            println(40 <+> 2);
          }
          println(99);
        }
    """
    transform(source("local_identity", local_source), "Identity", "42\n99\n")
    transform(source("local_unauthorized", local_source), "Forge",
              failure="not visible in its owning declaration")
    transform(source("copied_local_outside_block", local_source), "Relocate",
              failure="not visible in its owning declaration")
    transform(source("opaque_local", """
        fn main() {
          operator(lexeme: "<+>", arity: "binary", precedence: 5,
                   formals: a: i64, b: i64) -> i64 { return a * 10 + b; }
          println(1 <+> 2 <+> 3);
        }
    """), "OpaqueReorder", "33\n")
    source("definition_fixity", digits + """
        infix 0 <+>;
        macro from_definition {
          rule($x: Expression) => expression { 1 <+> $x <+> 4 };
        }
    """)
    transform(source("macro_fixity", """
        from .definition_fixity import from_definition;
        operator(lexeme: "<+>", arity: "binary", precedence: 5,
                 formals: a: i64, b: i64) -> i64 { return a * 100 + b; }
        fn main() {
          println(from_definition!(2 <+> 3 <+> 5));
          println(from_definition!(2 <+> 3));
        }
    """), "DirectiveOnly", "203064\n2044\n")
    transform(source("macro_local_instances", """
        macro make {
          rule($v: Expression) => block {
            operator(lexeme: "%%", arity: "binary", precedence: 5,
                     formals: a: i64, b: i64) -> i64 { return a * 10 + b; }
            infix 0 %%;
            println(1 %% $v %% 3);
          };
        }
        fn main() { make!(2); make!(4); }
    """), "DirectiveOnly", "33\n53\n")
    transform(source("copied_private_outside_expression", """
        from .definition_fixity import from_definition;
        operator(lexeme: "<+>", arity: "binary", precedence: 5,
                 formals: a: i64, b: i64) -> i64 { return a + b; }
        fn main() { println(from_definition!(30)); println(99); }
    """), "Relocate", failure="cannot relocate a lexical operator occurrence")
    transform(source("pervasive", """
        fn next(n: i64) -> i64 { return n + 1; }
        fn main() { println(41 |> next); }
    """), "Identity", "42\n", prelude=True)
    transform(source("synthesize_pervasive", """
        fn next(n: i64) -> i64 { return n + 1; }
        fn main() { println(99); }
    """), "SynthesizePervasive", "42\n", prelude=True)


if __name__ == "__main__":
    cli = argparse.ArgumentParser(description=__doc__)
    for argument in ("compiler", "root", "work"):
        cli.add_argument(argument, type=Path)
    cli.add_argument("cc")
    cli.add_argument("--cflag", action="append", default=[])
    args = cli.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="operator-ppx-", dir=args.work.resolve()) as directory:
        exercise(args.compiler.resolve(), args.root.resolve(), args.cc, args.cflag, Path(directory))
    print("On1x operator PPX readmission: PASS")
