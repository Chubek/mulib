#!/usr/bin/env python3
"""Exercise named declaration checks through real CLI/loader pairing."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    test_dir = Path(sys.argv[2]).resolve()
    test_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=test_dir) as temp:
        root = Path(temp)
        implementation = root / "vault.on1xm"
        interface = root / "vault.on1xi"
        implementation.write_text(
            """module Test.Vault;
record Token { secret: i64 }
fn make(value: i64) -> Token { return deliberate_unchecked_body; }
""",
            encoding="utf-8",
        )

        def check(text, expected_error=None):
            interface.write_text(text, encoding="utf-8")
            result = subprocess.run(
                [compiler, "--no-prelude", "--check-interfaces", implementation,
                 "--interface", interface],
                text=True, capture_output=True, timeout=30, check=False,
            )
            if expected_error is None:
                assert result.returncode == 0, result.stderr
                assert result.stdout == "OK: 2 interface members\n", result.stdout
            else:
                assert result.returncode != 0, result.stdout
                assert not result.stdout, result.stdout
                assert expected_error in result.stderr, result.stderr

        check("abstract module Test.Vault; type Token; fn make(input:i64)->Token;")
        check("abstract module Other.Vault; type Token; fn make(input:i64)->Token;",
              "abstract module name must match")
        check("abstract module Test.Vault; type Wrong; fn make(input:i64)->Wrong;",
              "expected one local nominal definition")
        check("abstract module Test.Vault; type Token; fn other(input:i64)->Token;",
              "expected one local On1x definition of 'other'")
        check("abstract module Test.Vault; type Token; fn make(input:bool)->Token;",
              "interface signature mismatch")


if __name__ == "__main__":
    main()
