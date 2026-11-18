#!/usr/bin/env python3
"""Core compiler regex profile, independent of PPX and LibStone at run time."""
import itertools
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER, CC, *FLAGS = sys.argv[1:]
FLAGS = [flag.removeprefix("--cflag=") for flag in FLAGS]


def run(*args):
    return subprocess.run([str(a) for a in args], capture_output=True, text=True,
                          timeout=40)


with tempfile.TemporaryDirectory(prefix="on1x-regex-test-") as directory:
    work = Path(directory)

    def source(name, contents):
        path = work / (name + ".on1x")
        path.write_bytes(contents if isinstance(contents, bytes) else contents.encode())
        return path

    def build(path, *extra):
        c = work / (path.stem + ".c")
        exe = work / path.stem
        p = run(COMPILER, "--emit-c", path, "-o", c)
        assert p.returncode == 0, p.stderr
        c_source = c.read_text()
        assert "#include <on1x_runtime.h>" in c_source and "on1x_regex_" in c_source
        assert "liblr" not in c_source and "firestone" not in c_source
        p = run(CC, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                "-I", ROOT / "runtime", c, ROOT / "runtime/on1x_runtime.c",
                *extra, *FLAGS, "-o", exe)
        assert p.returncode == 0, p.stderr
        return run(exe)

    p = build(ROOT / "tests/fixtures/test_core_regex.on1x")
    assert (p.returncode, p.stdout) == (0, "1\n2\nllk\nk\nk\n😀\n雪\n"), (p.stdout, p.stderr)

    # The type annotation is contextual: existing nominal names win.
    p = build(source("nominal_name", '''
record regex { value:i64 }
record captures { value:i64 }
fn main(){
    let x:regex=record<regex>{value:40};
    let c:captures=record<captures>{value:2};
    assert(match /42/ ~ "42");
    println(x["value"]+c["value"]);
}
'''))
    assert (p.returncode, p.stdout) == (0, "42\n"), p.stderr

    # A seeded differential on the intersecting Unicode-scalar profile.
    rng = random.Random(810)
    atoms = ["a", "[ab]", "é", "😀", ".", "(é|a)", "(ab|😀)", "(|雪)"]
    cases = []
    for _ in range(17):
        pattern = "".join(rng.choice(atoms) + rng.choice(["", "", "?", "*", "+"])
                          for _ in range(rng.randrange(1, 4)))
        for size in range(3):
            for chars in itertools.product("abé😀雪", repeat=size):
                cases.append((pattern, "".join(chars)))
    oracle = "".join(("true" if re.fullmatch(pat, text) else "false") + "\n"
                     for pat, text in cases)
    p = build(source("differential", "fn main(){\n" + "".join(
        f'println(match /{pat}/ ~ "{text}");\n' for pat, text in cases) + "}\n"))
    assert (p.returncode, p.stdout) == (0, oracle), p.stderr

    fixture = work / "bad_input.c"
    fixture.write_text(r'''
const char *malformed_input(void) {
    static const char malformed[] = "\355\240\200"; /* UTF-8 surrogate. */
    return malformed;
}
''')
    p = build(source("invalid_utf8_text", '''
extern fn malformed_input()->string;
fn main(){println(match /.../ ~ malformed_input());}
'''), fixture)
    assert (p.returncode, p.stdout) == (0, "false\n"), p.stderr

    for name, text, why in [
        ("index", 'println((capture /(a)/ ~ "a")[2]);', "capture index out of range"),
        ("negative_index", 'println((capture /(a)/ ~ "a")[-1]);', "capture index out of range"),
        ("absent_index", 'println((capture /(a)/ ~ "b")[0]);', "no match"),
        ("dynamic_profile", 'let p=/(a*)/; let c=capture p ~ "a";', "exact captures"),
    ]:
        p = build(source(name, f"fn main(){{{text}}}\n"))
        assert p.returncode == 73 and why in p.stderr and ":1:" in p.stderr, p.stderr

    for name, text, why in [
        ("unclosed", "let r = /abc;", "regex literal"),
        ("unclosed_application", 'let b = match /abc ~ "abc";', "regex literal"),
        ("group", "let r = /(abc/;", "expected ')'"),
        ("class", "let r = /[]/;", "empty character class"),
        ("range", "let r = /[z-a]/;", "reversed character range"),
        ("wide_class", "let r = /[雪]/;", "ASCII-only"),
        ("negated_class", "let r = /[^a]/;", "negated regex classes"),
        ("anchor", "let r = /^x/;", "metacharacter"),
        ("raw_byte", r"let r = /\xFF/;", "hex escapes require"),
        ("backreference", r"let r = /(a)\1/;", "unsupported regex escape"),
        ("repeat", "let r = /a*?/;", "metacharacter"),
        ("invalid_utf8", b"let r = /\xc0\xaf/;", "invalid UTF-8"),
        ("budget", "let r = /" + "a" * 257 + "/;", "exceeds 256 bytes"),
        ("explosive", "let r = /[ab]*a" + "[ab]" * 12 + "/;", "DFA exceeds"),
        ("captures_repeat", 'let c = capture /(a)+/ ~ "a";', "capture requires"),
        ("captures_nested", 'let c = capture /((a))/ ~ "a";', "capture requires"),
        ("captures_after_var", 'let c = capture /a*(b)/ ~ "ab";', "capture requires"),
        ("captures_disjunction", 'let c = capture /(a)|a/ ~ "a";', "capture requires"),
        ("captures_variable", 'let c = capture /(x|aa)/ ~ "x";', "capture requires"),
        ("wrong_pattern", 'let c = match "a" ~ "a";', "regex"),
        ("wrong_input", 'let c = match /a/ ~ true;', "string"),
        ("wrong_index", 'println((capture /(a)/ ~ "a")[true]);', "i64"),
        ("wrong_shorthand", 'let n = 2; println(n:1);', "captures"),
        ("wrong_annotation", 'let p: regex = "a";', "regex"),
        ("regex_equality", 'let same = /a/ == /a/;', "scalar"),
    ]:
        header, footer = "fn main(){", "}\n"
        if isinstance(text, bytes):
            contents = header.encode() + text + footer.encode()
        else:
            contents = header + text + footer
        c = work / "do_not_replace.c"
        c.write_text("already published\n")
        p = run(COMPILER, "--emit-c", source(name, contents), "-o", c)
        assert p.returncode == 1 and why in p.stderr and ":1:" in p.stderr, (name, p.stderr)
        assert not p.stdout and c.read_text() == "already published\n", name

    p = run(COMPILER, "--emit-ir", source("mir", 'fn main(){assert(match /a/ ~ "a");}'))
    assert p.returncode == 1 and "MIR" in p.stderr, p.stderr
