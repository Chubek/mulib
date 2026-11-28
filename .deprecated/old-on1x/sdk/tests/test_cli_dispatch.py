#!/usr/bin/env python3
"""Verify that on1xc explicitly dispatches only requested SDK data commands."""

import json
import os
import subprocess
import sys


def run(on1xc: str, *arguments: str, expected: int = 0):
    environment = {
        "LC_ALL": "C",
        "PATH": os.defpath,
        "PYTHONDONTWRITEBYTECODE": "1",
    }
    result = subprocess.run(
        [on1xc, "sdk", *arguments],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=30,
        check=False,
        env=environment,
    )
    if result.returncode != expected:
        raise RuntimeError(
            f"SDK dispatch returned {result.returncode}, expected {expected}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: test_cli_dispatch.py ON1XC", file=sys.stderr)
        return 2
    on1xc = os.path.abspath(sys.argv[1])
    info = json.loads(run(on1xc, "info").stdout)
    if (info.get("schema") != "on1x.sdk.info/1" or
            info.get("sdk_version") != "2.0.0" or
            info.get("extension_abi") != 2 or
            info.get("project_abi") != 2 or
            not isinstance(info.get("capabilities"), list)):
        raise RuntimeError("on1xc sdk info returned unexpected SDK metadata")
    run(on1xc, "not-a-command", expected=2)
    print("on1xc SDK data-tool dispatch passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(f"SDK dispatch test failed: {error}", file=sys.stderr)
        raise SystemExit(1)
