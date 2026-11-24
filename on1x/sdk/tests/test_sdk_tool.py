"""Black-box deterministic and negative tests for the on1x-sdk utility."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


PYTHON = Path(sys.argv.pop(1)).resolve(strict=True)
TOOL = Path(sys.argv.pop(1)).resolve(strict=True)
SOURCE = Path(sys.argv.pop(1)).resolve(strict=True)
SCRATCH = Path(sys.argv.pop(1)).resolve(strict=True)


class Tool(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="on1x-sdk-tool-",
                                                dir=SCRATCH)
        self.addCleanup(temporary.cleanup)
        self.work = Path(temporary.name)

    def run_tool(self, *arguments, expected=0):
        result = subprocess.run(
            (str(PYTHON), "-IB", str(TOOL), *map(str, arguments)),
            cwd=self.work,
            env={
                "PATH": os.defpath,
                "LC_ALL": "C",
                "PYTHONDONTWRITEBYTECODE": "1",
            },
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=30,
            check=False,
        )
        self.assertEqual(result.returncode, expected, result.stderr)
        self.assertEqual(result.stderr, b"")
        packet = json.loads(result.stdout)
        self.assertEqual(
            result.stdout,
            (json.dumps(packet, ensure_ascii=True, allow_nan=False,
                        sort_keys=True, separators=(",", ":")) + "\n").encode(
                            "ascii"),
        )
        return packet, result.stdout

    def write_json(self, name, value):
        path = self.work / name
        path.write_text(
            json.dumps(value, ensure_ascii=True, sort_keys=True),
            encoding="ascii",
        )
        return path

    def test_info_and_inspection_are_deterministic(self):
        first, wire = self.run_tool("info")
        second, second_wire = self.run_tool("info")
        self.assertEqual(wire, second_wire)
        self.assertEqual(first["schema"], "on1x.sdk.info/1")
        self.assertEqual(first["sdk_version"], "2.0.0")
        self.assertFalse(first["network_access"])
        self.assertFalse(first["loads_extension_code"])
        statuses = {item["id"]: item["status"]
                    for item in first["capabilities"]}
        self.assertEqual(statuses["project.build-graph"], "EXECUTABLE")
        self.assertEqual(statuses["tooling.semantic-index"], "UNSUPPORTED")

        document = self.write_json(
            "inspect.json", {"schema": "example/v1", "z": 1, "a": ["x"]})
        report, _ = self.run_tool("inspect", document)
        self.assertEqual(report["schema"], "on1x.sdk.inspect/1")
        self.assertEqual(report["input_schema"], "example/v1")
        self.assertEqual(report["keys"], ["a", "schema", "z"])
        canonical = b'{"a":["x"],"schema":"example/v1","z":1}\n'
        self.assertEqual(report["canonical_sha256"],
                         hashlib.sha256(canonical).hexdigest())

    def test_supported_validation_families(self):
        extension = self.write_json("extension.json", {
            "schema": "on1x.sdk.extension-manifest/1",
            "name": "example.tools",
            "version": "1.0.0",
            "sdk_abi": {"minimum": 2, "maximum": 2},
            "dependencies": [{"name": "example.base", "version": "^1"}],
            "facets": ["formatter"],
            "requested_capabilities": ["process"],
            "execution_profile": "native-worker",
            "artifacts": [{"name": "libexample.so", "sha256": "a" * 64}],
        })
        report, _ = self.run_tool(
            "validate", "extension-manifest", extension)
        self.assertEqual(
            report["input_schema"], "on1x.sdk.extension-manifest/1")
        self.assertFalse(report["code_loaded"])
        self.assertFalse(report["capabilities_granted"])
        self.assertEqual(report["authority"], "inspection-only")

        catalogue = SOURCE / "sdk/CAPABILITY_MATRIX.json"
        report, _ = self.run_tool("validate", "catalogue", catalogue)
        self.assertEqual(
            report["input_schema"], "on1x.sdk.capability-matrix/1")
        diagnostic_catalogue = SOURCE / "stdlib/diagnostics/catalog.json"
        report, _ = self.run_tool(
            "validate", "catalogue", diagnostic_catalogue)
        self.assertEqual(
            report["input_schema"], "on1x.diagnostic-catalog/v2")

        contract = SOURCE / "contracts-exoml/clamp.exocontract.json"
        report, _ = self.run_tool("validate", "contract", contract)
        self.assertEqual(report["input_schema"], "exoml.contract/2")

        manifest = self.work / "On1xfile"
        manifest.write_text(
            "on1xfile 1\nkind executable\nname \"tool\"\n"
            "target \"tool\" \"main.on1x\"\n"
            "dependency \"dep\" git \"cache/dep.git\" branch \"main\"\n",
            encoding="utf-8",
        )
        report, _ = self.run_tool("validate", "on1xfile", manifest)
        self.assertEqual(report["input_schema"], "on1xfile/1")
        lock = self.write_json("On1xfile.lock", {
            "schema": "on1x.lock/v1",
            "dependencies": {
                "dep": {
                    "kind": "git",
                    "source": "cache/dep.git",
                    "selector": {"kind": "branch", "value": "main"},
                    "commit": "1" * 40,
                }
            },
        })
        report, _ = self.run_tool("validate", "lock", lock)
        self.assertEqual(report["input_schema"], "on1x.lock/v1")
        lock = self.write_json("On1xfile.lock", {
            "schema": "on1x.lock/v2", "dependencies": {},
            "optimization": {"scoped": {"release": {"local-slots": False}}},
        })
        report, _ = self.run_tool("validate", "lock", lock)
        self.assertEqual(report["input_schema"], "on1x.lock/v2")
        for vm in ("always", "opt-in"):
            lock = self.write_json("On1xfile.lock", {
                "schema": "on1x.lock/v3", "dependencies": {}, "vm": vm,
            })
            report, _ = self.run_tool("validate", "lock", lock)
            self.assertEqual(report["input_schema"], "on1x.lock/v3")
        lock = self.write_json("On1xfile.lock", {
            "schema": "on1x.lock/v3", "dependencies": {}, "vm": "fallback",
        })
        report, _ = self.run_tool("validate", "lock", lock, expected=2)
        self.assertIn("invalid locked vm policy", report["message"])

    def test_malformed_oversized_duplicate_and_symlink_inputs(self):
        duplicate = self.work / "duplicate.json"
        duplicate.write_text('{"schema":"a","schema":"b"}', encoding="ascii")
        report, _ = self.run_tool("inspect", duplicate, expected=2)
        self.assertEqual(report["status"], "INVALID_ARGUMENT")
        self.assertIn("duplicate", report["message"])

        deep = self.work / "deep.json"
        deep.write_text("[" * 70 + "0" + "]" * 70, encoding="ascii")
        report, _ = self.run_tool("inspect", deep, expected=2)
        self.assertEqual(report["status"], "INVALID_ARGUMENT")

        oversized = self.work / "oversized.json"
        oversized.write_bytes(b" " * (1024 * 1024 + 1))
        report, _ = self.run_tool("inspect", oversized, expected=2)
        self.assertIn("regular file", report["message"])

        valid = self.write_json("valid.json", {"schema": "example/v1"})
        link = self.work / "link.json"
        link.symlink_to(valid)
        report, _ = self.run_tool("inspect", link, expected=2)
        self.assertEqual(report["status"], "INVALID_ARGUMENT")

        unsupported = self.write_json(
            "future.json", {"schema": "on1x.sdk.extension-manifest/999",
                            "name": "future"})
        report, _ = self.run_tool(
            "validate", "extension-manifest", unsupported, expected=3)
        self.assertEqual(report["status"], "UNSUPPORTED")

        traversal = self.work / "On1xfile"
        traversal.write_text(
            "on1xfile 1\nkind executable\nname \"bad\"\n"
            "target \"bad\" \"../outside.on1x\"\n",
            encoding="utf-8",
        )
        report, _ = self.run_tool(
            "validate", "on1xfile", traversal, expected=2)
        self.assertEqual(report["status"], "INVALID_ARGUMENT")

        malformed_catalogue = json.loads(
            (SOURCE / "stdlib/diagnostics/catalog.json").read_text(
                encoding="utf-8"))
        malformed_catalogue["levels"][0]["kind"] = 2
        malformed = self.write_json("malformed-catalogue.json",
                                    malformed_catalogue)
        report, _ = self.run_tool(
            "validate", "catalogue", malformed, expected=2)
        self.assertEqual(report["status"], "INVALID_ARGUMENT")

        contract = json.loads(
            (SOURCE / "contracts-exoml/clamp.exocontract.json").read_text(
                encoding="utf-8"))
        contract["functions"][0]["parameters"][0] = 0
        malformed = self.write_json("malformed-contract.json", contract)
        report, _ = self.run_tool(
            "validate", "contract", malformed, expected=2)
        self.assertEqual(report["status"], "INVALID_ARGUMENT")


if __name__ == "__main__":
    unittest.main()
