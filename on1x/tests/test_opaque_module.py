"""Execute the direct C11 artifact from the checked opaque ABI fixture."""
from pathlib import Path
import subprocess
import sys


def main():
    fixture, cc, runtime, output = sys.argv[1:]
    runtime = Path(runtime)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    c_source = output / "opaque.c"
    exe = output / "opaque"
    c_source.write_bytes(subprocess.check_output([fixture], timeout=30))
    subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                    "-I", runtime, c_source, runtime / "on1x_runtime.c", "-pthread",
                    "-o", exe], timeout=30, check=True)
    assert subprocess.check_output([exe], timeout=10) == b"42\n"


if __name__ == "__main__":
    main()
