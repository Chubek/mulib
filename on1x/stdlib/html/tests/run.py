"""Compile and execute std.html through the actual On1x C11 backend."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
HTML = HERE.parent
ON1X = HERE.parents[2]
ROOT = ON1X.parent


def run(argv, **kwargs):
    return subprocess.run(argv, timeout=30, check=True, **kwargs)


def main(compiler, cc, *flags):
    runtime = ON1X / "runtime"
    flags = [
        "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        "-I", runtime, "-I", ROOT / "kernels", "-I", HTML,
        *(f.removeprefix("--cflag=") for f in flags),
    ]
    with tempfile.TemporaryDirectory(prefix="on1x-html-") as tmp:
        work = Path(tmp)
        env = {**os.environ, "LC_ALL": "C"}

        def link(name, objects):
            executable = work / name
            generated = work / f"{name}.c"
            run([compiler, "--emit-c", HERE / f"{name}.on1x", "-o", generated])
            run([cc, *flags, generated, *objects, "-lm", "-o", executable])
            return executable

        def compile_c(name, source, opts=()):
            obj = work / f"{name}.o"
            run([cc, *flags, *opts, "-c", source, "-o", obj])
            return obj

        r = compile_c("on1x", runtime / "on1x_runtime.c")
        native = [compile_c("runtime", HTML / "runtime.c"),
                  compile_c("kernels", HTML / "kernels.c")]
        run([link("api", [r, *native])], env=env)
        raw = work / "raw"
        run([cc, *flags, HERE / "raw.c", r, *native, "-lm", "-o", raw])
        run([raw], env=env)
        stdin = link("stdin", [r, *native])

        def interpret(data):
            return run([stdin], env=env, input=data, capture_output=True).stdout

        html = interpret((HERE / "catalog.html").read_bytes())
        assert html.startswith(b'<!doctype html>\n<html lang="en">')
        assert b'<li data-id="42"><a href="/event?id=42&amp;lang=en">Caf\xc3\xa9 &amp; craft</a>\n      </li>' in html
        assert b'<li data-id="43"><a href="/event?id=43&amp;lang=en">Open studio</a>\n    </li>' in html
        assert b"<script" not in html and b"<style" not in html and b"Not a listing" not in html
        assert html.endswith(b"</body></html>") and interpret(html) == html
        assert interpret(b"x" * (64 * 1024)).startswith(b"<!doctype html>\n")
        assert interpret(b"x" * (64 * 1024 + 1)) == b"-2\n"
        assert interpret(b"<br>" * 1024) == b"-2\n"
        assert interpret(b"<p>" * 49) == (
            b"<!doctype html>\n<html><body>" + b"<p></p>" * 49 + b"</body></html>"
        )
        assert interpret(b"<div>" * 49).startswith(b"-")
        assert interpret(b"<html><head><body>x</html>") == b"-1\n"
        assert interpret(b"<html><body>first</body><body>second</body></html>") == b"-1\n"
        assert interpret(b"&" * (64 * 1024)) == b"-2\n"
        assert interpret(b"\xff") == b"-1\n"

        # Switch the whole native dependency closure to a counted allocator,
        # while leaving the regular On1x pool unmodified.
        shim = compile_c("alloc-shim", HERE / "alloc.c")
        include = ["-include", HERE / "alloc.h"]
        replacements = [
            *include, "-Dmalloc=on1x_html_test_malloc", "-Dcalloc=on1x_html_test_calloc",
            "-Drealloc=on1x_html_test_realloc", "-Dfree=on1x_html_test_free",
        ]
        hooked = [compile_c("hooked-runtime", HTML / "runtime.c", replacements),
                  compile_c("hooked-kernels", HTML / "kernels.c", replacements), shim]
        run([link("alloc", [r, *hooked])], env=env)
        fail_data_runtime = compile_c(
            "oom-runtime", runtime / "on1x_runtime.c",
            [*include, "-DON1X_DATA_MALLOC=on1x_html_test_fail_data_malloc"],
        )
        oom = subprocess.run([link("fail_data", [fail_data_runtime, *hooked])],
                             env=env, capture_output=True, timeout=30)
        assert oom.returncode == 73 and b"text allocation failure" in oom.stderr, oom
        assert b"LeakSanitizer" not in oom.stderr, oom


if __name__ == "__main__":
    main(*sys.argv[1:])
