#!/usr/bin/env python3
"""Source, manifest, and negative tests for the On1x-native SDK profile."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def run(argv: list[str], *, expect: int = 0) -> subprocess.CompletedProcess[str]:
    process = subprocess.run(
        argv,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        timeout=120,
    )
    if process.returncode != expect:
        raise RuntimeError(
            f"command returned {process.returncode}, expected {expect}: {argv!r}\n"
            f"stdout:\n{process.stdout}\nstderr:\n{process.stderr}"
        )
    return process


def require_rejection(
    on1xc: str,
    source: Path,
    expected_text: str,
) -> None:
    process = run([on1xc, "--check", str(source)], expect=1)
    output = process.stdout + process.stderr
    if expected_text not in output:
        raise RuntimeError(
            f"{source.name} did not report {expected_text!r}:\n{output}"
        )


def validate_catalogues(root: Path) -> None:
    manifest_path = root / "sdk/native/reference_extension/manifest.json"
    catalogue_path = root / "sdk/native/reference_extension/catalogue.json"
    contract_path = root / \
        "sdk/native/reference_extension/trusted-adapter-contract.json"
    candidate_path = root / \
        "sdk/native/reference_extension/candidate-schema.json"
    diagnostics_path = root / \
        "sdk/native/reference_extension/diagnostics.json"
    stdlib_path = root / "stdlib/sdk/CATALOG.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    candidate = json.loads(candidate_path.read_text(encoding="utf-8"))
    diagnostics = json.loads(diagnostics_path.read_text(encoding="utf-8"))
    stdlib = json.loads(stdlib_path.read_text(encoding="utf-8"))

    assert manifest["schema"] == "on1x.sdk.native-extension-manifest/1"
    assert manifest["sdk_abi"] == {"minimum": 2, "maximum": 2}
    assert manifest["bridge"]["handle_encoding"] == "generation32-slot32"
    assert manifest["discovery_executes_code"] is False
    assert manifest["capability_authority"] == "host-only"
    assert manifest["trust"]["self_certifying"] is False
    assert manifest["trust"]["sandboxed"] is False
    assert manifest["bridge"]["contract"] == "trusted-adapter-contract.json"
    assert manifest["output"]["schema_document"] == "candidate-schema.json"
    assert "granted_capabilities" not in manifest
    assert "executable_path" not in manifest

    assert catalogue["schema"] == "on1x.sdk.native-extension-catalogue/1"
    assert len(catalogue["entries"]) == 1
    assert catalogue["entries"][0]["activation"] == \
        "explicit-static-host-allowlist"
    assert (catalogue_path.parent /
            catalogue["entries"][0]["manifest"]).is_file()
    assert (catalogue_path.parent /
            catalogue["entries"][0]["source"]).resolve().is_file()
    assert contract["schema"] == "on1x.sdk.native-trusted-scalar/1"
    assert contract["handles"]["encoding"] == "generation32-slot32"
    assert contract["authority"]["capability_grants"] == "host-only"
    assert contract["reviewed_general_ffi"] is False
    assert candidate["schema"] == "on1x.sdk.native-reference-candidate/1"
    assert candidate["pointers"] is False
    assert diagnostics["schema"] == \
        "on1x.sdk.native-diagnostic-catalogue/1"
    assert diagnostics["entries"][0]["message_utf8_bytes"] == 28
    assert stdlib["schema"] == "on1x.stdlib.sdk.catalogue/1"
    assert len(stdlib["modules"]) == 9


def main() -> int:
    if len(sys.argv) != 5:
        print(
            "usage: test_native_sdk.py ON1XC ON1X_ROOT WORKDIR CC",
            file=sys.stderr,
        )
        return 2
    on1xc = os.path.abspath(sys.argv[1])
    root = Path(sys.argv[2]).resolve()
    work = Path(sys.argv[3]).resolve()
    cc = shutil.which(sys.argv[4]) or os.path.abspath(sys.argv[4])
    work.mkdir(parents=True, exist_ok=True)

    extension = root / "sdk/examples/native/reference_analyzer.on1x"
    positive = root / "sdk/tests/native/fixtures/stdlib_positive.on1x"
    generated_extension = work / "reference_analyzer.c"
    generated_extension_repeat = work / "reference_analyzer-repeat.c"
    generated_positive = work / "stdlib_positive.c"
    positive_executable = work / "stdlib_positive"

    run([on1xc, "--check", str(extension)])
    run([on1xc, "--emit-c", str(extension), "-o", str(generated_extension)])
    run([
        on1xc,
        "--emit-c",
        str(extension),
        "-o",
        str(generated_extension_repeat),
    ])
    if generated_extension.read_bytes() != generated_extension_repeat.read_bytes():
        raise RuntimeError("native extension C11 emission is not deterministic")
    generated_text = generated_extension.read_text(encoding="utf-8")
    if "on1x_fn_basis_sdk_reference_extension" not in generated_text:
        raise RuntimeError("canonical C11 output is missing the native entry")

    run([on1xc, "--emit-c", str(positive), "-o", str(generated_positive)])
    run([
        cc,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        f"-I{root / 'runtime'}",
        f"-I{root / 'sdk/native/reference_extension'}",
        str(generated_positive),
        str(root / "sdk/native/reference_extension/trusted_bridge.c"),
        str(root / "runtime/on1x_runtime.c"),
        "-lm",
        "-o",
        str(positive_executable),
    ])
    run([str(positive_executable)])

    fixtures = root / "sdk/tests/native/fixtures"
    require_rejection(
        on1xc,
        fixtures / "unsafe_adapter_string.on1x",
        "expected u8, got string",
    )
    require_rejection(
        on1xc,
        fixtures / "handle_kind_mismatch.on1x",
        "incompatible nominal types",
    )

    malformed = work / "malformed-utf8.on1x"
    malformed.write_bytes(b"fn main() -> i64 { return 0; }\\n\\xc0\\x80")
    process = subprocess.run(
        [on1xc, "--check", str(malformed)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        timeout=120,
    )
    if process.returncode == 0:
        raise RuntimeError("malformed UTF-8 source was accepted")

    validate_catalogues(root)
    print("native SDK source and manifest checks passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"native SDK test failed: {error}", file=sys.stderr)
        raise SystemExit(1)
