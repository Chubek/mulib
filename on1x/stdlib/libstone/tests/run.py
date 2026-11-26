"""Compile and exercise the std.libstone package through the direct C11 backend."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


STDLIB = Path(__file__).resolve().parents[2]
RUNTIME = STDLIB.parent / "runtime"
IMPORT = "import std.libstone as Stone;\n"
WIDE = """
fn wide_children(count: i64) -> sexpr
{
  var children = sx_cons(sx_string("last"), sx_nil());
  for _ in 0 .. count - 2
  {
    children = sx_cons(sx_nil(), children);
  }
  return sx_cons(sx_string("first"), children);
}
"""
SUCCESS = """
fn main()
{
  {
    let leaf = Stone.libstone_node("Leaf", sx_nil());
    let branch = Stone.libstone_node("Branch", sx_cons(leaf, sx_cons(sx_nil(), sx_nil())));
    assert(Stone.libstone_tag(branch) == "Branch");
    assert(Stone.libstone_tag(Stone.libstone_child(branch, 0)) == "Leaf");
    assert(sx_kind(Stone.libstone_child(branch, 1)) == 0);

    let wide = Stone.libstone_node("Wide", wide_children(262144));
    assert(Stone.libstone_tag(wide) == "Wide");
    assert(sx_text(Stone.libstone_child(wide, 0)) == "first");
    assert(sx_text(Stone.libstone_child(wide, 262143)) == "last");
  }
  assert(data_live_nodes() == 0);
}
"""


def run(*args, **kwargs):
    return subprocess.run(args, capture_output=True, timeout=60, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("on1xc", type=Path)
    parser.add_argument("cc", nargs="?", default="cc")
    parser.add_argument("--cflag", action="append", default=[])
    options = parser.parse_args()
    fatal_env = {**os.environ, "LSAN_OPTIONS": "detect_leaks=0"}

    with tempfile.TemporaryDirectory(prefix="on1x-libstone-") as tmp:
        work = Path(tmp)

        def build(name, program, *flags):
            source, c_file, exe = (work / (name + ext) for ext in (".on1x", ".c", ""))
            source.write_text(IMPORT + program, encoding="utf-8")
            emitted = run(options.on1xc.resolve(), "--emit-c", "--stdlib", STDLIB,
                          source, "-o", c_file)
            assert emitted.returncode == 0, (name, emitted.stderr)
            linked = run(options.cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                         "-I", RUNTIME, *options.cflag, *flags,
                         c_file, RUNTIME / "on1x_runtime.c", "-o", exe)
            assert linked.returncode == 0, (name, linked.stderr)
            return exe

        positive = run(build("round_trip", WIDE + SUCCESS))
        assert positive.returncode == 0 and not positive.stderr, positive

        cases = [
            ("atom_children", 'Stone.libstone_node("Tag", sx_symbol("atom"));',
             b"expected a proper list"),
            ("invalid_tag", 'Stone.libstone_node("bad tag", sx_nil());',
             b"invalid S-expression symbol"),
            ("empty_node", "Stone.libstone_tag(sx_nil());",
             b"expected symbol node tag"),
            ("atom_node", 'Stone.libstone_child(sx_symbol("Tag"), 0);',
             b"expected symbol node tag"),
            ("string_tag", 'Stone.libstone_tag(sx_cons(sx_string("Tag"), sx_nil()));',
             b"expected symbol node tag"),
            ("leaf_child", 'Stone.libstone_child(Stone.libstone_node("Leaf", sx_nil()), 0);',
             b"child index outside node"),
            ("min_index", 'Stone.libstone_child(Stone.libstone_node("Leaf", sx_nil()), '
             "-9223372036854775807 - 1);", b"child index outside node"),
            ("max_index", 'Stone.libstone_child(Stone.libstone_node("Tag", '
             "sx_cons(sx_nil(), sx_nil())), 9223372036854775807);", b"child index outside node"),
            ("wide_index", 'Stone.libstone_child(Stone.libstone_node("Wide", '
             "wide_children(262144)), 262144);", b"child index outside node"),
            ("oversize_children", 'Stone.libstone_node("Tag", wide_children(262145));',
             b"list exceeds work bound"),
            ("oversize_tag", 'Stone.libstone_tag(sx_cons(sx_symbol("Tag"), wide_children(262145)));',
             b"list exceeds work bound"),
            ("oversize_child", 'Stone.libstone_child(sx_cons(sx_symbol("Tag"), wide_children(262145)), 0);',
             b"list exceeds work bound"),
        ]
        for name, statement, diagnostic in cases:
            exe = build(name, WIDE + "fn main() { " + statement + " }\n")
            failed = run(exe, env=fatal_env)
            assert failed.returncode == 73 and not failed.stdout and diagnostic in failed.stderr, failed

        from_stdin = build("stdin_tag", """
fn main() { println(Stone.libstone_tag(Stone.libstone_node(io_stdin(), sx_nil()))); }
""")
        plain = run(from_stdin, input=b"Tag")
        assert plain.returncode == 0 and plain.stdout == b"Tag\n" and not plain.stderr, plain
        nul = run(from_stdin, input=b"Tag\x00suffix", env=fatal_env)
        assert nul.returncode == 73 and not nul.stdout and b"NUL-free text ABI" in nul.stderr, nul

        oom = run(build("alloc_failure", 'fn main() { Stone.libstone_node("Tag", sx_nil()); }\n',
                        "-DON1X_DATA_MALLOC(n)=((void)(n),(void*)0)"), env=fatal_env)
        assert oom.returncode == 73 and not oom.stdout and b"text allocation failure" in oom.stderr, oom

    print("LibStone node interchange: PASS")


if __name__ == "__main__":
    main()
