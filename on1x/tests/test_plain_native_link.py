#!/usr/bin/env python3
"""Ordinary host C11 linkage needs no source-kernel Python installation."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("build_driver", ROOT/"tools/on1x-build.py")
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class PlainNativeLink(unittest.TestCase):
    def test_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            old = os.environ.get("ON1X_TEST_PROCESSOR_PATH")
            try:
                os.environ["ON1X_TEST_PROCESSOR_PATH"] = f":{path}:"
                self.assertEqual(driver.processor_search_paths(
                    [f":{path}::{path}:"], "ON1X_TEST_PROCESSOR_PATH"), [path.resolve()])
                with self.assertRaisesRegex(ValueError, "byte limit"):
                    driver.processor_search_paths(["a"*65537], "ON1X_TEST_PROCESSOR_PATH")
            finally:
                if old is None:
                    os.environ.pop("ON1X_TEST_PROCESSOR_PATH", None)
                else:
                    os.environ["ON1X_TEST_PROCESSOR_PATH"] = old

    def test_link_and_failure_atomicity(self):
        # A mock only supplies frontend output; a real strict-C11 compiler,
        # runtime, linker and produced executable run below.
        with tempfile.TemporaryDirectory(prefix="on1x plain link ") as temporary:
            root = Path(temporary)
            compiler = root/"frontend"
            library = root/"source library"
            (library/"mulib").mkdir(parents=True)
            sentinel = root/"helper executed"
            (library/"mulib/housekeeping.py").write_text(
                f"from pathlib import Path\nPath({str(sentinel)!r}).touch()\nraise RuntimeError('untrusted helper')\n")
            source = root/"program.on1x"
            source.write_text("not parsed by mock frontend\n")
            compiler.write_text(f"""#!{sys.executable}
from pathlib import Path
import sys
args = sys.argv[1:]
if '--print-stdlib' in args:
    print(args[args.index('--stdlib')+1])
else:
    Path(args[args.index('-o')+1]).write_text(
        '#include <stdio.h>\\nint main(void) {{ puts("42"); return 0; }}\\n')
""")
            compiler.chmod(0o700)
            output = root/"published program"
            command = [sys.executable, ROOT/"tools/on1x-build.py", source,
                       "--on1xc", compiler, "--stdlib", library, "--cc", "cc",
                       "--mulib-root", root, "-o", output]
            result = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(subprocess.check_output([output], timeout=10), b"42\n")
            self.assertFalse(sentinel.exists())
            previous = output.read_bytes()
            result = subprocess.run([*command, "--cflag=-fnot-a-real-c-flag"],
                                    capture_output=True, text=True, timeout=60)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_bytes(), previous)
            if not (ROOT/"stdlib/mulib/housekeeping.py").is_file():
                result = subprocess.run([*command, "--mulib", "json"],
                                        capture_output=True, text=True, timeout=60)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("compiler-owned", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(output.read_bytes(), previous)
                self.assertFalse(sentinel.exists())


if __name__ == "__main__":
    unittest.main()
