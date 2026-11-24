#!/usr/bin/env python3
"""Produce and test fixtures with the actual selected LibStone build."""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from project_interface import CephyrProvider, ProjectionError, TARGET_ABIS


def load_upstream_tests(source):
    spec = importlib.util.spec_from_file_location("upstream_c11_interface",
                                                   source / "tests/test_c11_interface.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def save(output, name, result):
    (output / f"{name}.interface").write_bytes(result.output)
    (output / f"{name}.json").write_text(json.dumps(result.provenance, indent=2) + "\n")
    (output / f"{name}.stderr").write_bytes(result.diagnostics)


def negative(output, name, command, message):
    try:
        command()
    except ProjectionError as exc:
        assert exc.code == "cephyr_failed", (exc.code, str(exc))
        assert message in exc.diagnostics, exc.diagnostics
        (output / f"{name}.stderr").write_bytes(exc.diagnostics)
        (output / f"{name}.json").write_text(json.dumps(exc.provenance, indent=2) + "\n")
    else:
        raise AssertionError(f"expected Cephyr failure for {name}")


def stage(provider, operation, source):
    # Test-only access: no configurable non-interface stage in the public provider.
    return provider._run((*provider._options, operation, "-x", "c", "-"), source)


def qualify(args, local):
    upstream = load_upstream_tests(args.source)
    sys.path.insert(0, str(args.source / "python"))
    from cephyr import Compiler

    library = args.cephyr.parent / "libcephyr.so.0.1.0"
    tool_inputs = (library, args.source / "triples", args.source.parent / "Archon/stdspec")
    synthetic = local / "headers"
    synthetic.mkdir()
    (synthetic / "counts.h").write_bytes(b"#define COUNT 3\n#define FLAG_WIDTH 3\n")
    # Physical origin is not-owner.h, but Cephyr's syntax line map exposes a
    # remappable display path. An absolute spelling does not make it physical.
    physical = synthetic / "not-owner.h"
    misleading = synthetic / "owner.h"
    misleading.write_text("/* no declarations */\n")
    physical.write_text(f'#line 70 "{misleading}"\nint mulib_misleading(void);\n')
    sdk = Compiler(library)
    source = upstream.SOURCE.encode("ascii")
    output = args.output
    (output / "upstream-fixture.c").write_bytes(source)
    (output / "counts.h").write_bytes((synthetic / "counts.h").read_bytes())
    for target, abi in TARGET_ABIS.items():
        common = dict(libstone_revision=args.revision, scratch_root=local, tool_inputs=tool_inputs)
        synthetic_provider = CephyrProvider(args.cephyr, target, include_dirs=(synthetic,), **common)
        direct = synthetic_provider.project(source)
        prep = stage(synthetic_provider, "-E", source)
        ast = stage(synthetic_provider, "--emit-ast=json", source)
        preprocessed = synthetic_provider.project(prep.output, language="cpp-output")
        syntax = synthetic_provider.project(ast.output, language="c11-ast-json")
        # SDK requests omit the stage flag. Point its config to the very same
        # neutral bytes without changing this test process's working directory.
        sdk_args = list(synthetic_provider._options)
        sdk_args[sdk_args.index("--config") + 1] = str(local / "sdk.toml")
        (local / "sdk.toml").write_bytes(b"# On1x projection: no ambient settings or extensions\n")
        with patch.dict(os.environ, direct.provenance["invocation"]["env"], clear=True):
            embedded = sdk.emit_interface((*sdk_args, "-x", "c", "-"), source)
        assert embedded.ok, embedded.diagnostics
        assert direct.output == preprocessed.output == syntax.output == embedded.output
        doc = upstream.parse(direct.output)
        assert upstream.value(doc, "target") == target and upstream.value(doc, "abi") == abi
        upstream.check_declarations(doc, target)
        for route, result in (("direct", direct), ("preprocessed", preprocessed), ("syntax", syntax)):
            save(output, f"{target}.{route}", result)
        (output / f"{target}.sdk.interface").write_bytes(embedded.output)
        (output / f"{target}.sdk.stderr").write_bytes(embedded.diagnostics)
        (output / f"{target}.sdk.json").write_text(json.dumps(
            {"identity": direct.provenance["identity"], "sdk_arguments": sdk_args,
             "sdk_status": int(embedded.status), "loaded_library": str(library)}, indent=2) + "\n")
        (output / f"{target}.i").write_bytes(prep.output)
        (output / f"{target}.syntax-ast.json").write_bytes(ast.output)
        forged = stage(synthetic_provider, "--emit-ast=json", b'#include "not-owner.h"\n')
        origin_doc = json.loads(forged.output)
        [node] = origin_doc["ast"]["items"]
        assert node["kind"] == "function_decl"
        assert node["children"]["name"]["attributes"]["name"] == "mulib_misleading"
        assert any(entry == {"file": str(misleading), "line": 70, "physical": node["line"]}
                   for entry in origin_doc["source"]["line_map"])
        (output / f"{target}.presumed-origin.json").write_bytes(forged.output)
        negative(output, f"{target}.corrupt-syntax", lambda: synthetic_provider.project(
            ast.output + ast.output, language="c11-ast-json"), b"cephyr.ast")
        negative(output, f"{target}.unsupported-C", lambda: synthetic_provider.project(
            b"int not_c11(;\n"), b"cephyr.parse")
        vla = b"int body(int n) { int body_only[n]; return sizeof body_only; }\n"
        body = synthetic_provider.project(vla)
        assert b'"body"' in body.output and b"body_only" not in body.output
        save(output, f"{target}.body-declaration-only", body)
        negative(output, f"{target}.body-not-lowerable",
                 lambda: stage(synthetic_provider, "-fsyntax-only", vla), b"variable-length arrays")
        synthetic_provider.verify_inputs()

        mulib = CephyrProvider(args.cephyr, target, include_dirs=(args.mulib / "include",),
                               system_include_dirs=(args.source / "stdlib/include",), **common)
        clamp = mulib.project_header("mulib-clamp.h")
        doc = upstream.parse(clamp.output)
        for name in ("mulib_clamp_i64", "mulib_clamp_add_i64", "mulib_clamp_sub_i64"):
            decl = upstream.named(doc, "function", name)
            assert upstream.value(decl, "linkage") == "external"
            assert upstream.value(decl, "definition") == "declaration"
            native = upstream.value(upstream.value(decl, "type"), "result")
            assert native[:2] == ["integer", "long"] and upstream.value(native, "bits") == 64
        save(output, f"clamp.{target}", clamp)
        inline = mulib.project_header("mulib-container-core.h")
        decl = upstream.named(upstream.parse(inline.output), "function", "mulib_ct_counter_add")
        assert upstream.value(decl, "linkage") == "internal"
        assert upstream.value(decl, "definition") == "defined"
        assert "inline" in upstream.one(decl, "specifiers")
        native = upstream.value(upstream.value(decl, "type"), "result")
        assert native[:2] == ["integer", "unsigned-long"] and upstream.value(native, "bits") == 64
        save(output, f"container-core.{target}", inline)
        negative(output, f"media-timebase.{target}", lambda: mulib.project_header(
            "mulib-media-timebase.h"), b"file 'math.h' not found")
        mulib.verify_inputs()
        print(f"{target}: direct/preprocessed/syntax/SDK identical; "
              "clamp and inline container-core passed; media-timebase blocked on math.h")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("cephyr", "source", "mulib", "scratch", "output"):
        parser.add_argument(f"--{arg}", type=lambda p: Path(p).resolve(strict=True), required=True)
    parser.add_argument("--revision", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="qualify-projection-", dir=args.scratch) as local:
        qualify(args, Path(local))


if __name__ == "__main__":
    main()
