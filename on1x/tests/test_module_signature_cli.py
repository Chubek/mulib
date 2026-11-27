#!/usr/bin/env python3
"""Exercise named declaration checks through real CLI/loader pairing."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    test_dir = Path(sys.argv[2]).resolve()
    test_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=test_dir) as temp:
        root = Path(temp).resolve()
        implementation = root / "vault.on1xm"
        interface = root / "vault.on1xi"
        implementation.write_text(
            """module Test.Vault;
record Token { secret: i64 }
public fn make(value: i64) -> Token { return deliberate_unchecked_body; }
""",
            encoding="utf-8",
        )

        def run(*args):
            return subprocess.run(
                [compiler, "--no-prelude", *args],
                text=True, capture_output=True, timeout=30, check=False,
            )

        def check(text, expected_error=None, count=2):
            interface.write_text(text, encoding="utf-8")
            result = run("--check-interfaces", implementation, "--interface", interface)
            if expected_error is None:
                assert result.returncode == 0, result.stderr
                assert result.stdout == f"OK: {count} interface members\n", result.stdout
            else:
                assert result.returncode != 0, result.stdout
                assert not result.stdout, result.stdout
                assert expected_error in result.stderr, result.stderr

        check("abstract module Test.Vault; type Token; fn make(input:i64)->Token;")
        check("abstract module Other.Vault; type Token; fn make(input:i64)->Token;",
              "signature does not declare module 'Test.Vault'")
        check("abstract module Test.Vault; type Wrong; fn make(input:i64)->Wrong;",
              "expected one local nominal definition")
        check("abstract module Test.Vault; type Token; fn other(input:i64)->Token;",
              "expected one local On1x definition of 'other'")
        check("abstract module Test.Vault; type Token; fn make(input:bool)->Token;",
              "interface signature mismatch")

        # The project driver passes this transport in both check and emission
        # invocations. Validate the emission path, not just the signature parser.
        index = root / "modules.json"
        program = root / "app.on1x"
        output = root / "app.c"
        index.write_text(json.dumps({
            "schema": "on1x.project-modules/v1",
            "project": str(root),
            "programs": {},
            "members": {
                "Test.Vault": {
                    "package": str(root),
                    "declared_name": "Test.Vault",
                    "fragments": [str(implementation)],
                    "interfaces": [str(interface)],
                },
            },
        }), encoding="utf-8")
        program.write_text(
            "import Test.Vault as Vault; fn main()->i64 { return Vault.make(42); }\n",
            encoding="utf-8",
        )
        interface.write_text(
            "abstract module Test.Vault; type Token; fn make(input:i64)->Token;",
            encoding="utf-8",
        )
        result = run("--check-interfaces", program, "--project-module-index", index)
        assert result.returncode == 0, result.stderr
        assert result.stdout == "OK: 2 interface members\n", result.stdout
        result = run("--emit-c", program, "--project-module-index", index, "-o", output)
        assert result.returncode != 0 and not output.exists(), result.stdout
        assert "opaque module types cannot enter executable HIR" in result.stderr, result.stderr

        implementation.write_text(
            "module Test.Vault; public fn make(value:i64)->i64 { return value; }\n"
            "public proc reset(value:i64) {}\n",
            encoding="utf-8",
        )
        check("abstract module Test.Vault; fn make(input:i64)->i64; proc reset(input:i64);")
        check("abstract module Test.Vault; fn make(input:i64)->i64; fn reset(input:i64)->void;",
              "expected fn(i64) -> void, found proc(i64) -> void")
        interface.write_text(
            "abstract module Test.Vault; fn make(input:i64)->i64; proc reset(input:i64);",
            encoding="utf-8",
        )
        program.write_text(
            "import Test.Vault as Vault;\n"
            "fn main()->i64 { Vault.reset(0); return Vault.make(42); }\n",
            encoding="utf-8",
        )
        result = run("--emit-c", program, "--project-module-index", index, "-o", output)
        assert result.returncode == 0 and output.is_file(), result.stderr

        # A stale signature must also be rejected on the actual emit path.
        output.unlink()
        interface.write_text("abstract module Test.Vault; fn make(input:bool)->i64;", encoding="utf-8")
        result = run("--emit-c", program, "--project-module-index", index, "-o", output)
        assert result.returncode != 0 and not output.exists(), result.stdout
        assert "interface signature mismatch" in result.stderr, result.stderr

        check("""
signature Caller<T> { fn make(value:T)->T; };
signature VaultAPI { include Caller<i64>; proc reset(value:i64); };
module Test.Vault : VaultAPI;
""")
        result = run("--emit-c", program, "--project-module-index", index, "-o", output)
        assert result.returncode == 0 and output.is_file(), result.stderr
        output.unlink()

        implementation.write_text(
            """module Test.Vault;
record Token { label:i64 }
type View = Token;
type Payload = (i64,bool);
public fn make(value:Payload)->View { return deliberately_unchecked; }
""",
            encoding="utf-8",
        )
        catalog = """
signature Base<A> {
  type Token;
  type View;
  type Payload = (A,bool);
  fn make(value:Payload)->View;
};
signature VaultAPI<A> { include Base<A>; sharing type View = Token; };
"""
        check(catalog + "module Test.Vault : VaultAPI<i64>;", count=4)
        check(catalog + "abstract module Test.Vault : VaultAPI<i64>;", count=4)
        program.write_text(
            "import Test.Vault as Vault;\n"
            "fn main()->i64 { return Vault.make((42,true)); }\n",
            encoding="utf-8",
        )
        result = run("--check-interfaces", program, "--project-module-index", index)
        assert result.returncode == 0 and result.stdout == "OK: 4 interface members\n", result.stderr
        check(catalog + "module Test.Vault : VaultAPI<u64>;", "interface type equation mismatch")
        check(catalog + "module Test.Vault : VaultAPI<i64>;", count=4)
        result = run("--emit-c", program, "--project-module-index", index, "-o", output)
        assert result.returncode != 0 and not output.exists(), result.stdout
        assert "declaration-only" in result.stderr, result.stderr

        # A private copy with no module-interface association cannot smuggle
        # declaration-only type equations past ordinary admission.
        result = run("--emit-c", implementation, "-o", output)
        assert result.returncode != 0 and not output.exists(), result.stdout
        assert "declaration-only" in result.stderr, result.stderr


if __name__ == "__main__":
    main()
