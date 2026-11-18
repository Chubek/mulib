#!/usr/bin/env python3
"""PrettyPrint CLI, On1x link-plan, archive, ABI, and KernelBind discovery tests."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "on1x/tools"
BRIDGE = ROOT / "on1x/stdlib/prettyprint/runtime.c"
MODULES = ["prettydoc", "prettyemit", "prettysexpr"]
MACROS = ["MULIB_IMPL_" + name.upper() for name in MODULES]


def run(*args, input=None, ok=True):
    proc = subprocess.run(args, input=input, capture_output=True, timeout=120, cwd=ROOT)
    if (proc.returncode == 0) != ok:
        raise AssertionError((args, proc.returncode, proc.stdout, proc.stderr))
    return proc.stdout if ok else proc.stderr


def read(path):
    return json.loads(path.read_text())


def check_plan(plan):
    assert plan["implementation_translation_units"] == 1
    assert plan["implementation_macros"] == MACROS
    assert [entry["name"] for entry in plan["modules"]] == MODULES
    assert plan["native_sources"] == plan["link_libraries"] == []
    assert plan["safe_export"] is False


def cli(binary, sample):
    def render(*args, input=None, ok=True):
        return run(binary, *args, input=input, ok=ok)

    # The CLI does not append a newline to plain text.
    assert render(sample) == sample.with_suffix(".txt").read_bytes().rstrip(b"\n")
    assert b"<mn>42</mn>" in render("--target", "html", sample)
    assert render("--target", "latex", sample) == sample.with_suffix(".tex").read_bytes()
    assert render("--target", "troff", sample) == sample.with_suffix(".roff").read_bytes()
    group = b'(group (cat (text "abc") (cat (soft) (text "def"))))'
    assert render("--width", "10", "--ribbon", "6", input=group) == b"abc\ndef"
    assert render("--width", "10", "--ribbon", "0", input=group) == b"abc def"
    unicode_group = '(group (cat (text "界") (cat (soft) (annot "identifier" (text "é")))))'.encode()
    utf8 = "界 é".encode()
    assert render("--width", "4", input=unicode_group) == utf8
    assert render("--width", "3", input=unicode_group) == "界\né".encode()
    assert '界 <span class="tok-identifier">é</span>'.encode() in render(
        "--width", "4", "--target", "html", input=unicode_group)
    for invalid in ("-1", "+1", " 3", "4097", "999999999999999999999999"):
        render("--ribbon", invalid, input=group, ok=False)
    text = b'(text "x")'
    assert render(input=text.ljust(2 * 1024 * 1024)) == b"x"
    render(input=text.ljust(2 * 1024 * 1024 + 1), ok=False)
    render(input=text + b"\0", ok=False)
    render(input=text + b" (soft)", ok=False)
    print("PrettyPrint CLI targets and input limits passed")


def on1x(work, on1xc, cc, archive=None):
    binary = work / "program"
    link = work / "link.json"
    flags = [
        "--on1xc", on1xc, "--cc", cc, "--mulib-plan", link, "-o", binary,
    ]
    if archive is not None:
        flags += ["--mulib-archive", archive]
    run(sys.executable, TOOLS / "on1x-build.py", ROOT / "examples/prettyprint.on1x", *flags)
    output = run(binary)
    assert output.startswith(b"calculate(\n  alpha,\n  beta\n)\n<!doctype html>\n")
    assert b'<span class="tok-identifier">alpha</span>' in output
    report = read(link)
    check_plan(report["plan"])
    assert report["inputs"][str(BRIDGE)] == hashlib.sha256(BRIDGE.read_bytes()).hexdigest()
    assert ("mulib.c" in report["generated_inputs"]) == (archive is None)
    if archive is not None:
        assert str(archive / "libmulib.a") in report["inputs"]
    run(sys.executable, TOOLS / "on1x-build.py", ROOT / "tests/prettyprint.on1x", *flags)
    assert b"<mi>x&lt;&amp;</mi>" in run(binary)
    if archive is None:
        # Explicit legacy flags are still accepted and cannot duplicate the
        # automatically selected companion or implementation.
        run(sys.executable, TOOLS / "on1x-build.py", ROOT / "tests/prettyprint.on1x",
            *flags, "--mulib", "prettysexpr", "--extra-c", BRIDGE)
        check_plan(read(link)["plan"])
        assert b"<mi>x&lt;&amp;</mi>" in run(binary)
    print(f"On1x PrettyPrint example and boundary tests passed ({'archive' if archive else 'generated TU'})")


def exotract(work, on1xc, clang, exotract):
    archive = work / "archive"
    run(sys.executable, TOOLS / "on1x-exotract.py", "build", *MODULES,
        "--exotract", exotract, "--compiler", clang, "--output", archive)
    check_plan(read(archive / "build-plan.json"))
    assert run("ar", "t", archive / "libmulib.a").splitlines() == [b"mulib.o"]
    symbols = read(archive / "symbols.json")["libraries"]
    for name, export in zip(MODULES, ("mulib_pretty_layout", "mulib_pretty_emit", "mulib_pretty_render_expr")):
        assert export in symbols[name]["archive_exports"]
    on1x(work, on1xc, clang, archive)


def contract(work, bind, clang):
    header = BRIDGE.with_suffix(".h")
    seed = ROOT / "on1x/contracts-exoml/prettyprint.exocontract.json"
    decl = work / "prettyprint.cdecl.json"
    pinned = work / "prettyprint.pinned.json"
    resource = run(clang, "-print-resource-dir").decode().strip()
    revision = "sha256:" + hashlib.sha256(header.read_bytes()).hexdigest()
    assert read(seed)["bundle_provenance"]["revision"] == revision
    run(bind, "extract", header, "--revision", revision, "--resource-dir", resource, "-o", decl,
        "--provenance", seed, "--provenance", BRIDGE, "--provenance", ROOT / "tests/test_prettybridge.c")
    assert {d["name"] for d in read(decl)["declarations"] if d["kind"] == "function"} == {
        "on1x_pretty_render", "on1x_pretty_status",
    }
    pinned.write_bytes(run(bind, "pin", decl, seed))
    report = json.loads(run(bind, "check", decl, pinned))
    assert report["gate_b"] is True and report["safe_export"] is False
    assert report["review_metadata"] is False
    assert report["fingerprint"] == read(decl)["fingerprint"]
    assert not next(f for f in report["functions"] if f["name"] == "on1x_pretty_render")["raw_scalar_abi_supported"]
    print("PrettyPrint bridge CDecl + ExoML Gate B passed (unreviewed, no safe export)")


def discovery(work, on1xc, clang, exotract):
    source = ROOT / "tests/prettyprint-kernel-discovery.on1x"
    report = work / "discovery.json"
    driver = [sys.executable, TOOLS / "on1x-build.py"]
    resource = run(clang, "-print-resource-dir").decode().strip()
    run(*driver, source, "--inspect-kernels", "--no-prelude", "--on1xc", on1xc,
        "--cc", clang, "--exotract", exotract, "--resource-dir", resource, "-o", report)
    data = read(report)
    assert data["schema"] == "on1x.kernel-discovery/v1" and data["safe_export"] is False
    request, = data["requests"]
    assert request["file"] == str(source) and request["alias"] == "PrettyKernel" and not request["unsafe"]
    kernel = data["modules"]["mulib.prettysexpr"]
    impl = kernel["exolib"]["header_only"]
    assert impl["implementation_defines"] == MACROS and impl["implementation_translation_units"] == 1
    assert impl["dependencies"] == [str(ROOT / "include" / f"mulib-{name}.h") for name in MODULES[:-1]]
    sidecar = ROOT / "on1x/contracts-exoml/prettysexpr.exocontract.json"
    seed = kernel["contract"]
    assert seed["bundle_provenance"]["review_status"] == "unreviewed"
    semantics = {entry["c_name"]: entry for entry in seed["functions"]}
    render_semantics = semantics["mulib_pretty_render_expr"]
    assert render_semantics["return"]["kind"] == "owned" and render_semantics["return"]["destroy_with"] == "free"
    assert render_semantics["errors"]["kind"] == "null_on_failure"
    parse_semantics = semantics["mulib_pretty_parse_expr"]
    assert parse_semantics["parameters"][1]["ownership"] == "out"
    assert parse_semantics["errors"]["out_valid_on_failure"] is False
    assert data["inputs"][str(sidecar)] == hashlib.sha256(sidecar.read_bytes()).hexdigest()
    assert kernel["status"] == "missing_review" and kernel["callable"] is False
    graph = kernel["metadata"]["cdecl"]
    types = {entry["id"]: entry for entry in graph["types"]}
    decls = {entry["name"]: entry for entry in graph["declarations"] if entry["kind"] == "function"}
    parse = decls["mulib_pretty_parse_expr"]
    render = decls["mulib_pretty_render_expr"]
    for entry in (parse, render):
        assert entry["definition"] is False and len(entry["parameters"]) == 3
        assert kernel["symbols"][entry["name"]]["callable"] is False
    assert types[parse["parameters"][1]["type"]]["canonical_spelling"] == "struct mulib_prettydoc **"
    signature = types[render["type"]]
    assert types[signature["result"]]["canonical_spelling"] == "char *"
    assert types[signature["arguments"][1]]["canonical_spelling"] == "const struct mulib_pretty_options *"
    assert types[signature["arguments"][2]]["canonical_spelling"] == "enum mulib_pretty_target"
    assert b"ON1X-KERNEL-NOT-ADMITTED" in run(on1xc, "--check", source, ok=False)
    unsafe = work / "unsafe.on1x"
    unsafe.write_text(source.read_text().replace("import kernel", "unsafe import kernel"))
    assert b"ON1X-KERNEL-UNSAFE-UNSUPPORTED" in run(on1xc, "--check", unsafe, ok=False)
    print("PrettyPrint KernelBind discovery and unadmitted import rejection passed")


def main():
    mode, target, *args = sys.argv[1:]
    target = Path(target).resolve()
    if mode == "cli":
        cli(target, Path(args[0]).resolve())
        return
    with tempfile.TemporaryDirectory(prefix=mode + "-", dir=target) as work:
        action = {"on1x": on1x, "archive": exotract, "contract": contract, "discovery": discovery}[mode]
        action(Path(work), *args)


if __name__ == "__main__":
    main()
