"""Run a trusted ABI-2 analyzer only on canonical, verified compiler input."""

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    env = dict(os.environ)
    env.pop("ON1X_PLUGIN_PATH", None)

    def run(*args, status=(0,), environment=env):
        result = subprocess.run(
            [compiler, *args], capture_output=True, env=environment, timeout=15
        )
        assert result.returncode in status, (args, result)
        return result

    with tempfile.TemporaryDirectory(prefix="on1x-linked-sdk-v2-") as path:
        work = Path(path)
        source = work / "checked.on1x"
        source.write_text("fn main() -> i64 { return 19 * 3; }\n", encoding="utf-8")
        clean = run("--emit-c", source)
        assert b"SDK-V2-FIXTURE" not in clean.stderr
        if "--unavailable" in sys.argv[2:]:
            result = run("--sdk-analyze", "linked-v2:inspect", source, status=(1,))
            assert b"no linked ABI-2 analyzer" in result.stderr
            assert not result.stdout
            return

        # A dynamically discoverable file must never be consulted for this hook.
        (work / "unrelated.so").write_text("not a plugin", encoding="utf-8")
        v2_env = dict(env, ON1X_PLUGIN_PATH=str(work))
        result = run("--sdk-analyze", "linked-v2:inspect", source, environment=v2_env)
        assert not result.stdout and b"<sdk-mir>" in result.stderr
        assert b"[SDK-V2-FIXTURE] verified main has one multiply and constant 19" in result.stderr
        assert b"xxxxxxxxxx" not in result.stderr

        canonical = run("--emit-ir", source).stdout
        mir = work / "checked.mir"
        mir.write_bytes(canonical)
        assert run("--ir-input", "--sdk-analyze", "linked-v2:inspect", mir).stderr == result.stderr
        run("--sdk-analyze", "inspect", source, status=(1,))
        no_fallback = run("--sdk-analyze", "linked-v2:mir-check", source, status=(1,))
        assert b"SDK-FIXTURE-MIR" not in no_fallback.stderr
        run("--sdk-analyze", "linked-v2:inspect", "--plugin", "+missing", source, status=(1, 2))
        destination = work / "keep"
        destination.write_bytes(b"previous artifact")
        run("--sdk-emit", "linked-v2:inspect", source, "-o", destination, status=(1,))
        assert destination.read_bytes() == b"previous artifact"

        different_valid = work / "different.on1x"
        different_valid.write_text("fn main() -> i64 { return 18 * 3; }\n", encoding="utf-8")
        result = run("--sdk-analyze", "linked-v2:inspect", different_valid, status=(1,))
        assert b"VERIFICATION_FAILED" in result.stderr and b"SDK-V2-FIXTURE" not in result.stderr
        rejected = run("--sdk-analyze", "linked-v2:reject", source, status=(1,))
        assert b"error: [SDK-V2-FIXTURE]" in rejected.stderr and not rejected.stdout
        for hook, code, text in (
            ("missing", 1, b"unsupported hook"),
            ("bad-utf8", 1, b"invalid argument"),
            ("bad-span", 1, b"STALE"),
            ("overflow", 75, b"limit exceeded"),
            ("unknown", 1, b"ABI-2 analysis"),
            ("bad-shutdown", 1, b"CALLBACK_FAILED"),
        ):
            result = run("--sdk-analyze", f"linked-v2:{hook}", source, status=(code,))
            assert text in result.stderr, result.stderr
            assert b"SDK-V2-FIXTURE" not in result.stderr and not result.stdout

        invalid = work / "invalid.on1x"
        invalid.write_text("fn main() -> i64 { return false; }\n", encoding="utf-8")
        unverified, n = re.subn(rb"^(\(on1x\.mir \d+ )\d+", rb"\g<1>0", canonical)
        assert n == 1
        mir.write_bytes(unverified)
        for path, flags in ((invalid, []), (mir, ["--ir-input"])):
            result = run(*flags, "--sdk-analyze", "linked-v2:inspect", path, status=(1,))
            assert b"SDK-V2-FIXTURE" not in result.stderr and b"ABI-2 analysis" not in result.stderr
        assert run("--emit-c", source).stdout == clean.stdout


if __name__ == "__main__":
    main()
