#!/usr/bin/env python3
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def query(argv, **kwargs):
    result = subprocess.run(argv, capture_output=True, check=True, timeout=10, **kwargs)
    assert not result.stderr, result.stderr
    assert result.stdout.endswith(b"\n"), result.stdout
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("probe", type=Path)
    parser.add_argument("archon_linked", choices=("0", "1"))
    parser.add_argument("archon_target", nargs="?", default="")
    parser.add_argument("--archon-prerequisite", choices=("satisfied", "unsupported"),
                        default="satisfied")
    parser.add_argument("--compiler", type=Path)
    args = parser.parse_args()
    probe = [args.probe.resolve()]
    output = query(probe)
    with tempfile.TemporaryDirectory() as scratch:
        env = {**os.environ, "PATH": "", "ON1X_PLUGIN_PATH": scratch}
        assert query(probe, cwd=scratch, env=env) == output
        assert not list(Path(scratch).iterdir())
    if args.compiler:
        command = [args.compiler.resolve(), "--backend-capabilities"]
        assert query(command) == output
        invalid = subprocess.run(command + ["unexpected-input.on1x"], capture_output=True, timeout=10)
        assert invalid.returncode and not invalid.stdout, invalid
        assert b"--backend-capabilities takes no arguments" in invalid.stderr, invalid.stderr

    report = json.loads(output)
    assert report["schema"] == "on1x.backend-capabilities/v1"
    assert report["scope"] == "compiler-linked-sdk-builtins"
    providers = report["providers"]
    assert len({row["name"] for row in providers}) == len(providers)
    expected = {
        "c11": ("checked-hir", "on1x-hir", "c11-source", None),
        "mir-c11": ("verified-mir", "on1x-mir", "c11-source", {"min": 0, "max": 4}),
        "mir-eval": ("verified-mir", "on1x-mir", "program-output", {"min": 0, "max": 4}),
        "mir-text": ("verified-mir", "on1x-mir", "mir-text", {"min": 0, "max": 4}),
        "mir-cfg": ("verified-mir", "on1x-mir", "cfg-dot", {"min": 0, "max": 4}),
    }
    if args.archon_linked == "1":
        assert args.archon_target, "need the configured native target"
        expected["mir-archon"] = (
            "verified-mir", "on1x-mir", "assembly", {"min": 1, "max": 4}
        )
    assert {row["name"] for row in providers} == set(expected), providers
    for row in providers:
        name = row["name"]
        native = name == "mir-archon"
        assert row["discovery"] == "linked-builtin"
        assert row["registration_prerequisite"] == (
            args.archon_prerequisite if native else "satisfied"
        )
        assert row["build_link_readiness"] == "compiler-linked"
        assert row["external_build_link"] == "not-probed"
        assert row["execution_evidence"] == "not-collected"
        assert len(row["capabilities"]) == 1
        cap = row["capabilities"][0]
        assert (cap["phase"], cap["representation"], cap["artifact"],
                cap["input_schema"]) == expected[name]
        assert cap["target"] == {
            "kind": "exact" if native else "independent",
            "triples": [args.archon_target] if native else [],
            "required_features": [],
            "forbidden_features": [],
        }
        assert cap["declared_deterministic"] is True
        assert cap["admission"] == {
            "input": "not-checked",
            "policy": "explicit-trusted-native-assembly" if native else "compiler-linked",
        }


if __name__ == "__main__":
    main()
