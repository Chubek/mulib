"""Check the generated MIR grammar against the canonical C++ encoder."""
from pathlib import Path
import re
import runpy
import subprocess
import sys
import tempfile


parser, compiler = map(Path, sys.argv[1:])
grammar = runpy.run_path(str(parser))
parse = grammar["parse"]
LanguageError = grammar["LanguageError"]


def run(*args):
    return subprocess.run([str(compiler), *map(str, args)], check=True, capture_output=True, text=True).stdout


def reject(wire):
    try:
        parse(wire)
    except LanguageError as error:
        assert error.code == "PARSE", error
    else:
        raise AssertionError("generated parser accepted malformed MIR")


def records(node, tag):
    todo = [node]
    while todo:
        item = todo.pop()
        if item.symbol == tag:
            yield item
        todo.extend(reversed(item.children))


fixtures = {
    1: "fn main(){var x=0;while x<2{x=x+1;}println(x);}",
    2: "fn swap(x:(f64,bool))->(f64,bool){return x;}fn main(){swap((1.5,true));}",
    3: "fn add(x:f32)->f32{return x+2f32;}fn main(){println(add(1.5f32));}",
}
# Cephyr-backed FFI generation emits rewrite requests only when the input
# provides specific MIR evidence. Seed both directive kinds explicitly so the
# C++ encoder and generated parser are tested independently of that evidence.
fixtures[4] = '''(on1x.mir 4 1
  ((function 2 "main" void (at "fixture" 0 1 1) ()
    ((block 3 4 (at "fixture" 0 1 1) ()
      ((instruction 5 const i64 () 7 0 0 0 (at "fixture" 0 1 1))
       (instruction 6 print void (5) 0 0 4 7 (at "fixture" 0 1 1)))
      (exit 8 return 0 7 () (at "fixture" 0 1 1))))))
  ((directive "mir-archon" "x86_64-unknown-linux-gnu" "hint" "local-memory" 2 true)
   (directive "" "" "rewrite" "fold-identity" 0 false)))
'''
with tempfile.TemporaryDirectory(prefix="mir-wire-") as directory:
    for version, source in fixtures.items():
        path = Path(directory) / (f"v{version}.on1x" if version != 4 else "v4.mir")
        path.write_text(source, encoding="utf-8")
        wire = run("--emit-ir", path) if version != 4 else run("--emit-ir", "--ir-input", path)
        assert wire.startswith(f"(on1x.mir {version} "), wire[:200]
        tree = parse(wire).children[0]
        assert tree.symbol == "MirEnvelope"
        for record in ("Function", "Block", "Instruction", "Terminator"):
            assert next(records(tree, record), None) is not None, (version, record)
        if version == 1:
            assert next(records(tree, "Edge"), None) is not None
            # Canonical S-expression escaping is independent of the ambient
            # filesystem's restrictions on filenames.
            escaped = re.sub(r'\(at "[^"]*"', lambda _: r'(at "a\\b\"c\x7f"', wire, count=1)
            assert parse(escaped).children[0].symbol == "MirEnvelope"
        if version == 3:
            assert '(instruction ' in wire and ' numeric ' in wire
        if version != 4:
            assert next(records(tree, "Directives"), None) is None
            continue
        directives = list(records(tree, "Directive"))
        assert [node.children[4].token.lexeme for node in directives] == ['"hint"', '"rewrite"']
        saved = path
        saved.write_text(wire, encoding="utf-8")
        assert run("--emit-ir", "--ir-input", saved) == wire

        # There is no directive list in v1–v3, and the v4 flag is a Boolean
        # symbol, not a string. Corrupt a real emitted record in place.
        reject(wire.replace("(on1x.mir 4 ", "(on1x.mir 1 ", 1))
        flags = list(re.finditer(r'\(directive [^)]* (true|false)\)', wire))
        assert [flag.group(1) for flag in flags] == ["true", "false"]
        for flag in flags:
            start, end = flag.span(1)
            bad = f'{wire[:start]}"{flag.group(1)}"{wire[end:]}'
            reject(bad)
            saved.write_text(bad, encoding="utf-8")
            decoded = subprocess.run([str(compiler), "--emit-ir", "--ir-input", str(saved)],
                                     capture_output=True, text=True)
            assert decoded.returncode != 0 and "ON1X-MIR-DECODE" in decoded.stderr, decoded
