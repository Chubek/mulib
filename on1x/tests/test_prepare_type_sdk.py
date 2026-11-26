"""The Satie projection must apply its patch even inside a parent Git worktree."""

import json
from hashlib import sha256
from pathlib import Path
import subprocess
import sys
import tempfile


git, source_dir, build_dir = sys.argv[1:]
source = Path(source_dir).resolve()
helper = Path(__file__).resolve().parents[1] / "tools/prepare-type-sdk.py"

with tempfile.TemporaryDirectory(prefix="satie provision ", dir=build_dir) as work:
    parent = Path(work)
    subprocess.run([git, "init", "-q", parent], check=True, timeout=30)
    dest = parent / "build/sdk"
    command = [sys.executable, "-I", helper, "--source", source, "--output", dest]
    subprocess.run(command, cwd=parent, check=True, timeout=30)
    original = (source / "CMakeLists.txt").read_bytes()
    patched = (dest / "CMakeLists.txt").read_bytes()
    assert b"option(SATIE_EUF_ONLY" in patched and patched != original
    record = json.loads((dest / "on1x-sdk-provenance.json").read_bytes())
    assert record["original_sha256"]["CMakeLists.txt"] == sha256(original).hexdigest()
    assert record["patched_cmake_sha256"] == sha256(patched).hexdigest()

    another = subprocess.run(command, cwd=parent, capture_output=True, timeout=30)
    assert another.returncode and b"destination already exists" in another.stderr
    assert (dest / "CMakeLists.txt").read_bytes() == patched
