"""A legacy C11 project and a build-time recipe do not consult package roots."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd, env):
    result = subprocess.run(command, cwd=cwd, env=env, check=True, capture_output=True,
                            text=True, timeout=60)
    return result.stdout, result.stderr


compiler, on1x, artifacts, cc = map(Path, sys.argv[1:5])
flags = [f"--cflag={flag}" for flag in sys.argv[5:]]
artifacts.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="kernel-load-legacy-", dir=artifacts) as directory:
    work = Path(directory)
    project = work / "energy-budget"
    shutil.copytree(on1x / "examples/energy-budget", project)
    env = {**os.environ, "TMPDIR": str(work), "ON1X_KERNEL_TRACE": "1",
           "ON1X_KERNEL_PACKAGE_ROOTS": str(work / "does-not-exist")}
    env.pop("MULIB_FOREIGNLIB_PATH", None)
    output = work / "out"
    run([sys.executable, "-B", on1x / "tools/on1x-project.py", "build", project, "--on1xc",
         compiler, "--backend", "c11", "--cc", cc, "--output-dir", output], work, env)
    assert run([output / "energy-budget"], work, env) == (
        (project / "expected.txt").read_text(), "")

    source = on1x / "examples/kernel/portable_clamp.on1x"
    executable = work / "build-time-clamp"
    run([sys.executable, "-B", on1x / "tools/on1x-build.py", source, "--on1xc",
         compiler, "--cc", cc, "-o", executable, *flags], work, env)
    assert run([executable], work, env) == ("10\n9223372036854775807\n", "")
