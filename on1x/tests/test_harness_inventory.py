"""Build and link the one-TU raw archive; no language-level admission."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


exotract, bind, on1xc, cc, with_lua = sys.argv[1:]
root = Path(__file__).resolve().parents[2]
clang = shutil.which("clang")
if not clang:
    sys.exit(77)


def run(*args, cwd=root, check=True):
    result = subprocess.run(args, cwd=cwd, capture_output=True, timeout=60)
    assert not check or result.returncode == 0, (args, result.stderr.decode(errors="replace"))
    return result


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    return json.loads(path.read_bytes())


resource = run(clang, "-print-resource-dir").stdout.decode().strip()
target = run(cc, "-dumpmachine").stdout.decode().strip()
modules = ("workspace-fs", "unidiff", "workspace-edit", "process-stream", "fs")
tool = root / "on1x/tools/on1x-exotract.py"
registry = json.loads(run(sys.executable, tool, "list-kernels").stdout)
assert registry["safe_export"] is False
for name in modules[:-1]:
    profile = registry["modules"][name]
    assert profile["source_status"] == "eligible"
    assert profile["static_compile"] == {"safe": "unsupported", "unsafe": "unsupported"}

with tempfile.TemporaryDirectory(prefix="on1x-harness-abi-") as tmp:
    work = Path(tmp)
    archive = work / "archive"
    result = run(sys.executable, tool, "build", *modules, "--exotract", exotract,
                 "--compiler", cc, "--resource-dir", resource, "--output", archive, check=False)
    assert result.returncode == 0, result.stderr.decode()
    plan = load(archive / "build-plan.json")
    assert plan["safe_export"] is False and plan["implementation_translation_units"] == 1
    assert plan["compile_definitions"] == ["_POSIX_C_SOURCE=200809L", "_GNU_SOURCE=1"]
    switches = plan["implementation_macros"]
    assert len(switches) == len(set(switches)) == 5 and "MULIB_IMPL_PTY" not in switches
    assert run("ar", "t", archive / "libmulib.a").stdout.splitlines() == [b"mulib.o"]
    exports = load(archive / "symbols.json")
    assert exports["safe_export"] is False and set(exports["libraries"]) == set(modules)
    assert not any(name.startswith("mulib_pty_") for name in exports["defined_symbols"])
    lock = load(archive / "lock.json")
    for name, sha in lock["artifacts"].items():
        assert sha == digest(archive / name), name
    for name, sha in lock["inputs"].items():
        assert sha == digest(Path(name)), name

    for name in modules[:-1]:
        header = root / "include" / f"mulib-{name}.h"
        direct = work / f"direct-{name}.json"
        run(bind, "extract", header, "--revision", "sha256:" + digest(header),
            "--include", root / "include", "--target", target,
            "--resource-dir", resource, "-o", direct)
        expected = set(exports["libraries"][name]["archive_exports"])
        for graph in (load(direct), load(archive / f"{name}.cdecl.json")):
            assert graph["complete"] and graph["safe_export"] is False
            assert graph["revision"] == "sha256:" + digest(header)
            assert expected == {decl["name"] for decl in graph["declarations"]
                                if decl["kind"] == "function"}, name

    run(cc, "-std=c11", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        "-I", root / "include", root / "tests/harness-kernels/test_harness.c",
        archive / "libmulib.a", "-o", work / "client")
    run(work / "client", cwd=work)

    source = work / "verify.on1x"
    output = work / "not-admitted"
    build = root / "on1x/tools/on1x-build.py"
    options = ["--no-prelude", "--on1xc", on1xc, "--cc", cc, "--exotract", exotract,
               "--resource-dir", resource, "--mulib-root", root]
    if with_lua == "1":
        aliases = ["mulib." + name.replace("-", "_") for name in modules[:-1]]
        source.write_text("".join(f"import kernel {name} as K{i};\n" for i, name in enumerate(aliases))
                          + "fn main(){}\n")
        inspection = work / "inspection.json"
        run(sys.executable, build, "--inspect-kernels", *options, source, "-o", inspection)
        report = load(inspection)
        assert report["safe_export"] is False and set(report["modules"]) == set(aliases)
        for name in aliases:
            module = report["modules"][name]
            assert module["status"] == "not_representable"
            assert not module["callable"] and not module["safe_export"] and module["sidecar"] is None
    for prefix, code in (("", "NOT-ADMITTED"), ("unsafe ", "UNSAFE-UNSUPPORTED")):
        source.write_text(prefix + "import kernel mulib.workspace_edit as E;\nfn main(){}\n")
        result = run(sys.executable, build, "--kernel-bind", *options, source, "-o", output, check=False)
        diagnostic = ("ON1X-KERNEL-" + code).encode() if with_lua == "1" else b"Lua support not built"
        assert result.returncode and diagnostic in result.stderr, result.stderr
        assert not output.exists()
