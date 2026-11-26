"""Emit, compile and exercise the standalone Elvi package without network."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ELVI = HERE.parent
ON1X = HERE.parents[2]
ROOT = ON1X.parent


def main(compiler, cc, *flags):
    with tempfile.TemporaryDirectory(prefix="on1x-elvi-") as temp:
        work = Path(temp)
        env = {**os.environ, "TMPDIR": temp}
        env.pop("ELVI_ALLOC_TEST", None)

        def run(*args, **kwargs):
            return subprocess.run(args, check=True, timeout=120, env=env, **kwargs)

        def emit(source):
            generated = work / (source.stem + ".c")
            run(compiler, "--emit-c", "--stdlib", ON1X / "stdlib", source, "-o", generated)
            return generated

        runtime = ON1X / "runtime"
        cflags = ["-D_POSIX_C_SOURCE=200809L", "-std=c11", "-Wall", "-Wextra",
                  "-Werror", "-Wpedantic", "-I", ROOT / "kernels", "-I", runtime,
                  "-I", ELVI, *(f.removeprefix("--cflag=") for f in flags if f != "--legacy")]
        native = [ELVI / "runtime.c", ELVI / "kernels.c"]
        linked = [*native, ELVI / "bridge.c", runtime / "on1x_runtime.c"]
        run(cc, *cflags, HERE / "native.c", *native, "-lm", "-o", work / "native")
        run(work / "native", ROOT / "library/elvis/Google.elvs")
        run(cc, *cflags, emit(HERE / "elvi.on1x"), *linked, "-lm", "-o", work / "elvi")
        run(work / "elvi")
        run(cc, *cflags, emit(HERE / "alloc.on1x"), *linked,
            "-include", HERE / "alloc.h", "-DON1X_ELVI_MALLOC=elvi_test_hit_alloc",
            "-DON1X_ELVI_FREE=elvi_test_hit_free", "-DON1X_DATA_MALLOC=elvi_test_text_alloc",
            HERE / "alloc.c", "-lm", "-o", work / "alloc")
        env["ELVI_ALLOC_TEST"] = "native"
        run(work / "alloc")
        env["ELVI_ALLOC_TEST"] = "text"
        fatal = subprocess.run([work / "alloc"], check=False, timeout=30,
                               capture_output=True, env=env)
        assert fatal.returncode == 73 and b"text allocation failure" in fatal.stderr, fatal

        if "--legacy" in flags:
            legacy = work / "legacy.o"
            run(cc, *cflags, "-Dmain=elvi_legacy_fixture_entry", "-c",
                ROOT / "tests/test-elvi-script.c", "-o", legacy)
            run(cc, *cflags, HERE / "script_native.c", ELVI / "script.c",
                ELVI / "runtime.c", legacy, "-lm", "-ldl", "-pthread",
                "-o", work / "script")
            run(work / "script")


if __name__ == "__main__":
    main(*sys.argv[1:])
