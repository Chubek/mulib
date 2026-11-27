#!/usr/bin/env python3
"""Execute bounded nested transparent records through the real loader and C11."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


def main():
    compiler = Path(sys.argv[1]).resolve()
    work = Path(sys.argv[2]).resolve()
    work.mkdir(parents=True, exist_ok=True)
    runtime = Path(__file__).resolve().parents[1]/"runtime"
    with tempfile.TemporaryDirectory(dir=work, prefix="nested records ") as temporary:
        root = Path(temporary)
        entry, output, index = root/"entry.on1x", root/"entry.c", root/"modules.json"
        payload, token, interface = root/"payload.on1xm", root/"token.on1xm", root/"view.on1xi"
        payload_source = "module Example.Vault; record Payload { number:i64 }"
        token_source = (
            "module Example.Vault; record Token { payload:Payload }"
            "public fn make(n:i64)->Token {return record<Token>{payload:record<Payload>{number:n}};}"
            "public fn read(t:Token)->i64 {return t.payload.number;}"
        )
        signature = (
            "signature S {type Payload; type Token; fn make(n:i64)->Token; fn read(t:Token)->i64;};"
            "module Example.Vault : S;"
        )
        payload.write_text(payload_source)
        token.write_text(token_source)
        interface.write_text(signature)
        peer, peer_interface = root/"peer.on1xm", root/"peer.on1xi"
        peer.write_text((payload_source + token_source.replace("module Example.Vault;", ""))
                        .replace("Example.Vault", "Example.Peer"))
        peer_interface.write_text(signature.replace("Example.Vault", "Example.Peer"))
        index.write_text(json.dumps({
            "schema": "on1x.project-modules/v1", "project": str(root), "programs": {},
            "members": {
                "Example.Vault": {"package": str(root), "declared_name": "Example.Vault",
                                  "fragments": [str(payload), str(token)], "interfaces": [str(interface)]},
                "Example.Peer": {"package": str(root), "declared_name": "Example.Peer",
                                 "fragments": [str(peer)], "interfaces": [str(peer_interface)]},
            },
        }))
        imports = "import Example.Vault as V; from Example.Vault import Token as Key;"
        client = (
            imports + "fn pass(t:Key)->V.Token {return t;}"
            "fn main(){"
            "println(pass(V.make(42)).payload.number);"
            "println(V.read(record<V.Token>{payload:record<V.Payload>{number:42}}));"
            "let saved=V.make(42); println(V.read(saved));}"
        )
        entry.write_text(client)

        def emit(mode="--emit-c"):
            return subprocess.run([compiler, "--no-prelude", mode, entry,
                                   "--project-module-index", index, "-o", output],
                                  capture_output=True, text=True, timeout=30)

        result = emit()
        assert result.returncode == 0, result.stderr
        good = output.read_bytes()
        binary = root/"program"
        subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-Wall",
                        "-Wextra", "-Wpedantic", "-Werror",
                        *shlex.split(os.environ.get("ON1X_TEST_VIEW_CFLAGS", "")),
                        "-I", runtime, output, runtime/"on1x_runtime.c", "-pthread",
                        "-o", binary], check=True, timeout=30)
        assert subprocess.check_output([binary], timeout=10) == b"42\n42\n42\n"

        def denied(message, mode="--emit-c"):
            result = emit(mode)
            assert result.returncode != 0 and message in result.stderr, result.stderr
            assert output.read_bytes() == good

        # Same-layout peers must not acquire this provider's nominal identity.
        entry.write_text("import Example.Peer as P;" + imports +
                         "fn main(){println(V.read(P.make(42)));}")
        denied("incompatible nominal types")
        entry.write_text(client)
        # A transparent outer record cannot implicitly export hidden layouts.
        interface.write_text(signature.replace("type Payload;", ""))
        denied("unexported representation type")
        interface.write_text(signature)
        payload.write_text(payload_source.replace("number:i64", "number:string"))
        denied("transparent executable record fields")
        payload.write_text(payload_source.replace("number:i64", "number:Token"))
        denied("recursive executable representation")
        payload.write_text(payload_source)
        token.write_text(token_source.replace("payload:Payload", "payload:Payload<i64>"))
        denied("transparent executable record fields")
        token.write_text(token_source)
        interface.write_text(signature.replace("module Example.Vault", "abstract module Example.Vault"))
        # Existing opaque admission can transport these values, but cannot expose
        # their layout even when the nested record is itself in the signature.
        denied("opaque module type representation")
        interface.write_text(signature)
        for mode in ("--emit-ir", "--emit-vm"):
            denied("direct C11", mode)


if __name__ == "__main__":
    main()
