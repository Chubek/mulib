"""Exercise the real Yellowstone generator and the header from the driver build."""

from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    generator, spec, built_header = map(Path, sys.argv[1:])
    builtins = ("c11", "mir-c11", "mir-eval", "mir-text", "mir-cfg")
    calls = [
        f'qld.backend("{name}", "on1x_builtin_{name.replace("-", "_")}_register")'
        for name in builtins
    ]
    rows = [
        f'ON1X_BUILTIN_REGISTRAR("{name}", on1x_builtin_{name.replace("-", "_")}_register)'
        for name in builtins
    ]

    def compare(header, ordered_rows):
        text = header.read_text(encoding="utf-8").splitlines()
        assert text[1:] == ordered_rows, text

    with tempfile.TemporaryDirectory(prefix="on1x registrar map ") as workdir:
        source = Path(workdir) / "backends.lua"
        output = Path(workdir) / "BuiltinRegistrars.inc"
        cmd = [generator.resolve(), source.name, output.name]
        source.write_bytes(spec.read_bytes())
        subprocess.run(cmd, cwd=workdir, check=True, timeout=5)
        compare(output, rows)
        assert output.read_bytes() == built_header.read_bytes()

        source.write_text("\n".join(reversed(calls)), encoding="utf-8")
        subprocess.run(cmd, cwd=workdir, check=True, timeout=5)
        compare(output, rows[::-1])

        bad_scripts = (
            ("\n".join(calls[:-1]), "missing required"),
            ('qld.backend("custom", "custom_register")', "unknown built-in"),
            (calls[0].replace("builtin_c11", "builtin_other"), "unknown built-in"),
            ("\n".join(calls + calls[:1]), "duplicate backend"),
            (calls[0] + '\nerror("abort manifest")', "abort manifest"),
            ('qld.input("libc11.a")', ""),
            (" " * (16 * 1024 + 1), "exceeds 16 KiB"),
        )
        previous = output.read_bytes()
        for body, expected in bad_scripts:
            source.write_text(body, encoding="utf-8")
            failed = subprocess.run(cmd, cwd=workdir, capture_output=True, text=True, timeout=5)
            assert failed.returncode != 0 and expected in failed.stderr, failed
            assert output.read_bytes() == previous
            assert not output.with_suffix(".inc.tmp").exists()


if __name__ == "__main__":
    main()
