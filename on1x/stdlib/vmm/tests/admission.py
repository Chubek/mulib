"""Verify real extraction and that the unreviewed seed cannot publish bindings."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

bind, compiler, cc, header, library = sys.argv[1:]
contract = Path(__file__).resolve().parents[1] / "version.contract.json"
revision = json.loads(contract.read_text(encoding="utf-8"))["bundle_provenance"]["revision"]


def run(args, expected=0):
    result = subprocess.run([str(x) for x in args], text=True, capture_output=True, timeout=30)
    assert result.returncode == expected, (args, result)
    return result


with tempfile.TemporaryDirectory(prefix="vmm-admission-", dir=Path.cwd()) as tmp:
    work = Path(tmp)
    decl = work / "archvirt.cdecl.json"
    pinned = work / "archvirt.pinned.json"
    argv = [bind, "extract", header, "--revision", revision]
    clang = shutil.which("clang")
    if clang:
        argv += ["--resource-dir", run([clang, "-print-resource-dir"]).stdout.strip()]
    decl.write_text(run(argv).stdout, encoding="utf-8")
    pinned.write_text(run([bind, "pin", decl, contract]).stdout, encoding="utf-8")
    result = json.loads(run([bind, "check", decl, pinned]).stdout)
    assert result["safe_profile"] == "reviewed-immutable-string/v4", result
    assert result["gate_b"] and not result["safe_export"] and not result["review_metadata"], result
    assert [entry["name"] for entry in result["functions"]] == ["archvirt_version"], result
    assert result["functions"][0]["safe_profile_supported"], result
    out = work / "version"
    rejected = run(
        [bind, "emit-safe", decl, pinned, out, "--on1xc", compiler,
         "--cc", cc, "--workdir", work / "gates", "--impl", library],
        expected=3,
    )
    assert "review metadata gate failed" in rejected.stderr, rejected.stderr
    assert not list(work.glob("version*")) and not (work / "gates").exists()
