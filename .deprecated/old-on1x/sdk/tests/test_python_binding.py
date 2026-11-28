"""Execute the installed-shape ctypes binding against the built shared ABI."""

import copy
import gc
import importlib
import os
from pathlib import Path
import sys
import unittest


LIBRARY = Path(sys.argv.pop(1)).resolve(strict=True)
BINDING = Path(sys.argv.pop(1)).resolve(strict=True)
os.environ["ON1X_SDK_PROJECT_LIBRARY"] = str(LIBRARY)
sys.path.insert(0, str(BINDING))
sdk = importlib.import_module("on1x_sdk")


class Binding(unittest.TestCase):
    def test_manifest_lifecycle_and_views(self):
        source = (
            "on1xfile 1\n"
            "kind executable\n"
            "name \"python\"\n"
            "directory \"src\"\n"
            "target \"python\" \"main.on1x\"\n"
            "dependency \"dep\" local \"vendor/dep\"\n"
        )
        with sdk.Manifest(source) as manifest:
            self.assertEqual(manifest.name, "python")
            self.assertEqual(manifest.kind, 1)
            self.assertEqual(manifest.directory, "src")
            self.assertEqual(manifest.targets, (("python", "main.on1x"),))
            self.assertEqual(manifest.dependencies, ({
                "name": "dep",
                "kind": 1,
                "source": "vendor/dep",
                "selector_kind": 0,
                "selector": "",
            },))
            self.assertRegex(manifest.source_revision, r"^[0-9a-f]{64}$")
        with self.assertRaisesRegex(RuntimeError, "closed"):
            _ = manifest.name
        manifest.close()  # Idempotent.

    def test_error_and_normalization(self):
        with self.assertRaises(sdk.SDKError) as caught:
            sdk.Manifest(
                "on1xfile 1\nkind executable\nname \"bad\"\n"
                "target \"bad\" \"../escape\"\n"
            )
        self.assertEqual(caught.exception.status, 4)
        self.assertIn("ON1X-SDK-PATH", caught.exception.diagnostic)
        self.assertEqual(sdk.normalize_path("src/main.on1x"), "src/main.on1x")
        self.assertEqual(sdk.normalize_path(".", allow_root_dot=True), ".")
        with self.assertRaises(sdk.SDKError):
            sdk.normalize_path("../../outside")

    def test_repeated_native_ownership(self):
        source = "on1xfile 1\nkind library\nname \"short\"\n"
        for _ in range(100):
            manifest = sdk.Manifest(source)
            self.assertEqual(manifest.name, "short")
            manifest.close()
        gc.collect()

    def test_native_owner_rejects_copying(self):
        source = "on1xfile 1\nkind library\nname \"owned\"\n"
        manifest = sdk.Manifest(source)
        with self.assertRaises(TypeError):
            copy.copy(manifest)
        with self.assertRaises(TypeError):
            copy.deepcopy(manifest)
        self.assertEqual(manifest.name, "owned")
        manifest.close()


if __name__ == "__main__":
    unittest.main()
