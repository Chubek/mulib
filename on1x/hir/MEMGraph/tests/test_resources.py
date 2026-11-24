#!/usr/bin/env python3
"""Checked HIR speculation and actual scalar scratch placement in C11."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

compiler, root, work = (Path(a).resolve() for a in sys.argv[1:4])
cc = sys.argv[4]
cflags = [arg.removeprefix("--cflag=") for arg in sys.argv[5:]]
runtime = root / "runtime"


def run(*args, code=0):
    result = subprocess.run(args, text=True, capture_output=True, timeout=60)
    assert result.returncode == code, result
    return result


def source(dir_, name, text):
    path = dir_ / name
    path.write_text(text, encoding="ascii")
    return path


def native(c, *extra, flags=(), code=0):
    binary = c.with_suffix(".exe")
    run(cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        "-I", runtime, c, runtime / "on1x_runtime.c", *extra,
        *flags, *cflags, "-lm", "-o", binary)
    return run(binary, code=code)


work.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
    d = Path(d)
    prefix = "extern fn observe(s:string);\n"
    good = source(d, "static.on1x", prefix + """
fn main() {
  let root = "native static literal";
  let alias = root;
  { let leaf = alias; observe(leaf); }
  { let root = "shadow"; observe(root); }
}
""")
    p = run(compiler, "--check", "--no-prelude", "--memgraph-resources", "error", good)
    assert not p.stderr, p

    suspicious = source(d, "raw.on1x", prefix + """extern fn obtain() -> string;
fn main() {
  let result = obtain();
  let relay = result;
  observe(relay);
  var mutable = "looks static";
  observe(mutable);
  let borrowed = "also looks static";
  { let change = borrow mut borrowed; change:=relay; }
  observe(borrowed);
  observe("genuinely static");
}
""")
    output = d / "raw.c"
    emit = [compiler, "--emit-c", "--no-prelude", suspicious, "-o", output]
    p = run(*emit, "--memgraph-resources", "error", code=1)
    assert not output.exists() and p.stderr.count("ON1X-MEMGRAPH-RETENTION") == 3, p
    assert "error:" in p.stderr and "ON1X-MEMGRAPH-POSSIBLE-LEAK" in p.stderr, p
    p = run(*emit, "--memgraph-resources", "warn")
    assert "warning:" in p.stderr and "ON1X-MEMGRAPH-POSSIBLE-LEAK" in p.stderr, p
    warned_c = output.read_bytes()
    p = run(*emit, "--memgraph-resources", "off")
    assert not p.stderr and output.read_bytes() == warned_c, p
    assert not run(*emit).stderr and output.read_bytes() == warned_c
    assert not run(*emit, "--memgraph-stack-bytes", "0").stderr
    assert output.read_bytes() == warned_c  # Never copy or move the foreign result.

    limited = source(d, "limit.on1x", prefix + "fn main(){\n" + 'observe("x");\n' * 4097 + "}\n")
    p = run(compiler, "--check", "--no-prelude", "--memgraph-resources", "warn", limited)
    assert "ON1X-MEMGRAPH-RESOURCE-LIMIT" in p.stderr, p
    assert "ON1X-MEMGRAPH-RETENTION" not in p.stderr, p
    p = run(compiler, "--check", "--no-prelude", "--memgraph-resources", "error", limited, code=1)
    assert "ON1X-MEMGRAPH-RESOURCE-LIMIT" in p.stderr, p
    assert "--memgraph-stack-bytes requires" in run(compiler, "--emit-ir", "--memgraph-stack-bytes",
                                                   "0", good, code=2).stderr
    assert "must be 0..65536" in run(compiler, "--memgraph-stack-bytes", "65537", good, code=2).stderr

    mir_source = source(d, "slots.on1x", """
fn change(early:bool)->i64 {
  let x=5;
  let edit=borrow mut x;
  if early { return edit; }
  var i=0;
  while i<3 { edit:=edit+1; i=i+1; }
  return edit;
}
fn main(){ println(change(true)); println(change(false)); }
""")
    mir = d / "slots.mir"
    run(compiler, "--emit-ir", mir_source, "-o", mir)
    reference = run(compiler, "--run-ir", "--ir-input", mir).stdout
    assert reference == "5\n8\n", reference
    for stack_bytes, want_stack, want_heap in [(0, False, True), (8, True, True), (16, True, False)]:
        c = d / f"mir{stack_bytes}.c"
        run(compiler, "--emit-ir-c", "--ir-input", "--memgraph-stack-bytes", str(stack_bytes), mir, "-o", c)
        text = c.read_text()
        assert ("MEMGraph: heap slot" in text) == want_heap, text
        assert ("MEMGraph: stack slot" in text) == want_stack, text
        assert ("on1x_memgraph_return:;" in text) == want_heap, text
        assert native(c).stdout == reference

    # Override malloc in the generated TU only. The checked allocation trap
    # must precede access to the missing cell, with no observable partial result.
    fail = source(d, "fail.c", """
#include <stddef.h>
void *no_scratch_malloc(size_t bytes) { (void)bytes; return NULL; }
""")
    obj, binary = d / "fail-generated.o", d / "fail-program"
    run(cc, "-std=c11", "-I", runtime, *cflags, "-Dmalloc=no_scratch_malloc",
        "-c", d / "mir0.c", "-o", obj)
    run(cc, "-std=c11", "-I", runtime, *cflags, obj, fail, runtime / "on1x_runtime.c",
        "-lm", "-o", binary)
    p = run(binary, code=72)
    assert not p.stdout and "allocation failure" in p.stderr, p

    malformed, count = re.subn(r"( load i64 \(\) 0 )\d+", r"\g<1>800000", mir.read_text(), count=1)
    assert count == 1
    bad_mir = source(d, "invalid.mir", malformed)
    bad_c = d / "invalid.c"
    p = run(compiler, "--emit-ir-c", "--ir-input", "--memgraph-stack-bytes", "8",
            bad_mir, "-o", bad_c, code=1)
    assert "unknown local slot" in p.stderr and not bad_c.exists(), p

    if sys.platform == "linux" and os.uname().machine == "x86_64":
        impl = source(d, "ipc-impl.c", """
#define MULIB_IMPL_SANDBOX_PROFILE
#define MULIB_IMPL_IPC_FRAMED
#include <mulib-sandbox-profile.h>
#include <mulib-ipc-framed.h>
""")
        for example, trace in (("framed_pair", "0\n42\n-1\n"), ("framed_session", "42\n297\n")):
            on1x = root / "examples" / "kernel" / (example + ".on1x")
            for stack_bytes in (0, 8):
                c = d / f"{example}-{stack_bytes}.c"
                run(compiler, "--kernel-c11", "--no-prelude", "--memgraph-stack-bytes",
                    str(stack_bytes), "--emit-c", on1x, "-o", c)
                text = c.read_text()
                assert f"MEMGraph: {'stack' if stack_bytes else 'heap'} (call-scoped i64 scratch)" in text
                assert re.search(r"on1x_kernel_ipc_v1_open_(?:session|pair)\(&on1x_v\d+_ipc_out\)", text)
                assert re.search(r"on1x_unique \*on1x_v\d+_ipc_out = NULL;", text)
                assert ("size_t bytes; }" in text) == (stack_bytes == 0)
                options = ("-D_GNU_SOURCE", "-DON1X_KERNEL_ABI_V1=1", "-I", root.parent / "include")
                p = native(c, impl, runtime / "on1x_kernel_ipc.c", flags=options)
                assert p.stdout == trace and not p.stderr, p
    else:
        print("IPC placement subtest not applicable: requires Linux x86-64")
