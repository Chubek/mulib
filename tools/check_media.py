#!/usr/bin/env python3
"""Bounded standalone C11 review checks, not production/On1x admission."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-UNDEBUG", "-O2"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", action="append", help="repeatable; default: gcc and clang")
    parser.add_argument("--no-sanitize", action="store_true", help="explicitly skip ASan/UBSan")
    parser.add_argument("--timeout", type=int, default=60, help="seconds per process")
    parser.add_argument("--report", type=Path, help="write command/results JSON to this path")
    parser.add_argument("--fuzz-runs", type=int, default=0,
                        help="optional Clang libFuzzer parser runs (0..1000000)")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if not 0 <= args.fuzz_runs <= 1000000:
        parser.error("--fuzz-runs must be between 0 and 1000000")
    if args.fuzz_runs and not shutil.which("clang"):
        parser.error("--fuzz-runs requires clang with libFuzzer support")
    inventory = json.loads((ROOT / "manual/media-kernels.json").read_text())
    headers = [item["header"] for item in inventory]
    if len(headers) != 194 or len(set(headers)) != 194 or "mulib-audio-core.h" not in headers:
        parser.error("review inventory must contain the 193 kernels plus shared PCM helper")
    compilers = args.compiler or ["gcc", "clang"]
    for compiler in compilers:
        if not shutil.which(compiler):
            parser.error(f"required compiler not found: {compiler}")
    records = []
    source_hashes = {name: hashlib.sha256((ROOT / "kernels" / name).read_bytes()).hexdigest()
                     for name in headers}
    env = dict(os.environ, LC_ALL="C")
    env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

    def run(command, *, expect_link_failure=False):
        start = time.monotonic()
        try:
            result = subprocess.run(
                [str(x) for x in command], cwd=ROOT, env=env,
                capture_output=True, text=True, timeout=args.timeout,
            )
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"timeout: {command}") from error
        passed = result.returncode == 0
        if expect_link_failure:
            passed = (result.returncode != 0 and
                      "mulib_media_timebase_rescale" in result.stderr and
                      ("undefined" in result.stderr or "unresolved" in result.stderr))
        records.append({
            "command": [str(x) for x in command],
            "expected_link_failure": expect_link_failure,
            "returncode": result.returncode,
            "seconds": round(time.monotonic() - start, 3),
            "passed": passed,
            "stdout": result.stdout,
            "stderr": result.stderr,
        })
        if not passed:
            raise RuntimeError(
                f"failed: {' '.join(str(x) for x in command)}\n"
                f"{result.stdout}{result.stderr}"
            )

    temp_root = os.environ.get("TMPDIR") or (
        "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None)
    try:
        with tempfile.TemporaryDirectory(prefix="mulib-media-", dir=temp_root) as directory:
            work = Path(directory)
            aggregate = work / "aggregate.c"
            includes = "".join(f'#include "{name}"\n' for name in headers)
            aggregate.write_text(includes + "int main(void) { return 0; }\n")
            canonical_names = [
                "fraction", "linalg", "cgeom", "floatgraphics", "glx-pixelformat",
                "audio", "raytracing", "colorprofile", "colorconvert", "colormodel",
            ]
            canonical = ["mulib-" + name + ".h" for name in canonical_names]
            canonical += sorted(p.name for p in (ROOT / "kernels").glob("mulib-bytecode-*.h"))
            composition = work / "composition.c"
            composition.write_text(
                "".join(f'#include "{name}"\n' for name in canonical) +
                includes + "int main(void) { return 0; }\n"
            )
            one = work / "one.c"
            # A translation unit containing static inline functions cannot supply
            # a foreign symbol. This negative link is intentionally expected.
            external = work / "external.c"
            external.write_text(
                '#include "mulib-media-core.h"\n'
                'extern bool mulib_media_timebase_rescale(int64_t,'
                'mulib_media_rational,mulib_media_rational,int64_t *);\n'
                'int main(void) { int64_t out; return !mulib_media_timebase_rescale'
                '(1,(mulib_media_rational){1,1},(mulib_media_rational){1,1},&out); }\n'
            )
            provider = work / "inline-only.c"
            provider.write_text('#include "mulib-media-timebase.h"\n')
            tests = sorted((ROOT / "tests").glob("media-*-test.c"))
            if not tests:
                raise RuntimeError("no media regression test sources found")
            run([sys.executable, ROOT / "tests/test-media-tools.py"])
            for compiler in compilers:
                base = [compiler, *FLAGS, "-I", ROOT / "kernels"]
                run([compiler, "--version"])
                for header in headers:
                    one.write_text(f'#include "{header}"\nint main(void) {{ return 0; }}\n')
                    run([*base, "-fsyntax-only", one])
                print(f"{compiler}: {len(headers)} independent headers passed", flush=True)
                run([*base, "-fsyntax-only", composition])
                for extra, label in [([], "aggregate"), (["-U__SIZEOF_INT128__"], "portable")]:
                    executable = work / label
                    run([*base, *extra, aggregate, "-lm", "-o", executable])
                    run([executable])
                run([*base, ROOT / "tests/media-link-a.c",
                     ROOT / "tests/media-link-b.c", "-lm", "-o", work / "link"])
                run([work / "link"])
                run([*base, external, provider, "-lm", "-o", work / "not-exported"],
                    expect_link_failure=True)
                for source in tests + [ROOT / "examples/media-pipeline.c"]:
                    executable = work / source.stem
                    run([*base, source, "-lm", "-o", executable])
                    run([executable])
                    if not args.no_sanitize:
                        run([*base, "-g", "-O1", "-fsanitize=address,undefined",
                             "-fno-omit-frame-pointer", source, "-lm", "-o", executable])
                        run([executable])
                # Exercise exact rational arithmetic without compiler wide integers.
                run([*base, "-U__SIZEOF_INT128__",
                     ROOT / "tests/media-integration-test.c", "-lm", "-o", work / "fallback"])
                run([work / "fallback"])
                print(f"{compiler}: aggregate, canonical composition, portable, separate-TU, negative link, "
                      f"{len(tests)} regressions and example passed"
                      + ("" if args.no_sanitize else " (ASan/UBSan included)"), flush=True)
            if args.fuzz_runs:
                run(["clang", *FLAGS, "-I", ROOT / "kernels", "-g", "-O1",
                     "-fsanitize=fuzzer,address,undefined", "-fno-omit-frame-pointer",
                     ROOT / "tests/fuzz_media_parsers.c", "-lm", "-o", work / "fuzz-parsers"])
                run([work / "fuzz-parsers", "-runs=" + str(args.fuzz_runs),
                     "-max_len=4096", "-seed=42", "-timeout=2", "-rss_limit_mb=256",
                     "-artifact_prefix=" + str(work) + "/"])
                print(f"Clang libFuzzer: {args.fuzz_runs} bounded parser runs passed", flush=True)
            changed = [name for name, digest in source_hashes.items()
                       if hashlib.sha256((ROOT / "kernels" / name).read_bytes()).hexdigest() != digest]
            if changed:
                raise RuntimeError("headers changed during validation; rerun: " + ", ".join(changed))
    except (OSError, RuntimeError) as error:
        print(error, file=sys.stderr)
        return 1
    finally:
        if args.report:
            args.report.write_text(json.dumps({
                "status": "review-only; no safe On1x admission",
                "sanitizers": not args.no_sanitize,
                "headers": headers,
                "source_sha256": source_hashes,
                "results": records,
            }, indent=2) + "\n")
    print("Standalone review checks passed; no native codec/GPU/audio or On1x claim.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
