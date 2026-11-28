#!/usr/bin/env python3
"""Execute assembly actually admitted through the trusted SDK native profile."""
import pathlib
import subprocess
import sys
import tempfile


def main():
    emitter, cc, scalar_abi, runtime, work_root = sys.argv[1:]
    root = pathlib.Path(work_root)
    root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="sdk-native-", dir=root) as temporary:
        work = pathlib.Path(temporary)
        assembly = work / "main.s"
        executable = work / "main"
        subprocess.run([emitter, str(assembly)], check=True, timeout=60)
        subprocess.run(
            [cc, str(assembly), scalar_abi, runtime, "-lm", "-pthread",
             "-o", str(executable)],
            check=True, timeout=60,
        )
        result = subprocess.run([str(executable)], capture_output=True, timeout=10)
        assert (result.returncode, result.stdout, result.stderr) == (42, b"", b""), result


if __name__ == "__main__":
    main()
