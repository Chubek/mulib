"""Smoke-test the shared Sublime grammar with bat's real syntax engine."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    bat = shutil.which("bat") or shutil.which("batcat")
    if not bat:
        print("SKIP: neither bat nor batcat is installed")
        return 77
    syntax_root = Path(__file__).resolve().parents[1] / "sublime"
    with tempfile.TemporaryDirectory(prefix="on1x-bat-") as temp:
        work = Path(temp)
        config = work / "config"
        syntax_dir = config / "syntaxes"
        syntax_dir.mkdir(parents=True)
        for name in ("On1x.sublime-syntax", "On1xfile.sublime-syntax"):
            shutil.copyfile(syntax_root / name, syntax_dir / name)
        # Avoid inheriting user options or touching the user's cache.
        env = {key: value for key, value in os.environ.items() if not key.startswith("BAT_")}
        env.update(BAT_CONFIG_DIR=str(config), BAT_CACHE_PATH=str(work / "cache"))

        def run(*args):
            return subprocess.run(
                [bat, *args], text=True, stdout=subprocess.PIPE, check=True, timeout=30, env=env
            ).stdout

        run("cache", "--build")
        assert re.search(r"^On1x:\s*on1x,\s*on1xi\s*$", run("--list-languages"), re.M), "On1x not registered"
        options = ("--no-config", "--color=always", "--theme=Monokai Extended", "--plain", "--paging=never")
        sgr = re.compile(r"\x1b\[[0-9;]*m")

        def color(line, token):
            """Return the active SGR code at the start of a token."""
            column = sgr.sub("", line).index(token)
            previous = 0
            current = ""
            for escape in sgr.finditer(line):
                column -= escape.start() - previous
                if column < 0:
                    break
                current = escape.group()
                previous = escape.end()
            return current

        # A body line in a triple-backtick literal must look like string data,
        # even when it would be a comment (or contain quotes) in source.
        source = 'fn main() {\n  let p = launch ```/bin/echo\n# "data"\n```;\n  return;\n}\n'
        source_file = work / "fixture.on1x"
        source_file.write_text(source, encoding="utf-8")
        iface_file = work / "fixture.on1xi"
        iface_file.write_text("on1xi 1;\nfn main() -> void;\n", encoding="utf-8")
        for fixture in (source_file, iface_file):
            colored = run(*options, str(fixture))
            assert colored == run(*options, "-l", "On1x", str(fixture))
            assert colored != run(*options, "-l", "txt", str(fixture))
            assert sgr.sub("", colored) == fixture.read_text(encoding="utf-8")
        colored = run(*options, str(source_file)).splitlines()
        assert sgr.findall(colored[2])[0] != sgr.findall(colored[4])[0]
        assert len(sgr.findall(colored[2])) == 2, "backtick contents were tokenized as source"

        source_file.write_text(
            "import foreign stdffi.clamp as Clamp;\n"
            "import foreign as Module;\n"
            "unsafe import foreign stdffi.clamp;\n"
            "proc main() {\n"
            "  let n = 0o17u32 + 0B10;\n"
            '  let c: char = #\\pound; # comment\n'
            "  let block = '''text\n"
            "    # body ${match true with {true => \"inside\", false => \"\"}} text''';\n"
            '  let raw = r!"\\0 $name";\n'
            '  let bytes = b!"\\0\\xff $name";\n'
            '  {- outer {@name(x) nested @} still outer -} let tail = 0;\n'
            '  {! metadata {- opaque } let end = 1;\n'
            '  let p = proc() {@Name()};\n'
            '  infixr 3 <$>;\n'
            '  let answer = a <$> b >>. c ^^ d;\n'
            '  let operator_value = operator("<$>");\n'
            '}\n',
            encoding="utf-8",
        )
        latest = run(*options, str(source_file)).splitlines()
        assert color(latest[0], "foreign") == color(latest[0], "import")
        assert color(latest[1], "foreign") != color(latest[0], "foreign")
        assert color(latest[2], "foreign") != color(latest[0], "foreign")
        assert color(latest[4], "0o17u32") == color(latest[4], "0B10")
        assert color(latest[4], "0o17u32") != color(latest[4], " + ")
        assert color(latest[5], "#\\pound") != color(latest[5], "# comment")
        assert color(latest[7], "# body") == color(latest[7], " text")
        assert color(latest[7], "true") != color(latest[7], "inside")
        assert color(latest[8], "\\0") == color(latest[8], "$name")
        assert color(latest[9], "\\0") != color(latest[9], "$name")
        assert color(latest[10], "outer") == color(latest[10], "still outer")
        assert color(latest[10], "let tail") == color(latest[4], "let")
        assert color(latest[11], "let end") == color(latest[4], "let")
        assert color(latest[12], "@Name") != color(latest[5], "# comment")
        assert color(latest[13], "infixr") != color(latest[13], "3")
        assert color(latest[14], "<$>") == color(latest[14], "$>")
        assert color(latest[14], ">>.") == color(latest[14], ". c")
        assert color(latest[15], "operator(") != color(latest[15], "operator_value")

        for name, directives in (
            ("On1xfile", [
                'backend "debug" mir-vm',
                'optimization-profile default',
                'optimization "integer_add_zero_right" on',
                'optimization "debug" "all" off',
                'kernel-path "ffi/primary"',
                'proxy-http "http://proxy.example:8080"',
                'proxy-https "http://proxy.example:8080"',
                'proxy-any "http://proxy.example:8080"',
                'dotenv ".env" preserve',
                'vm opt-in',
                'profile-arg "size" level integer -2',
                'compiler cephyr',
                'assembler sandas',
                'linker yellowstone',
                'assembler-arg "-g"',
                'linker-arg "--gc-sections"',
            ]),
            ("On1xfile.m", ['library "messages.on1x"']),
            ("On1xfile.i", ['interface "messages.on1xi"']),
        ):
            fixture = work / name
            fixture.write_text("on1xfile 1\n" + "".join(f"{line} # comment\n" for line in directives), encoding="utf-8")
            colored_manifest = run(*options, str(fixture))
            assert colored_manifest == run(*options, "-l", "On1xfile", str(fixture)), name
            assert colored_manifest != run(*options, "-l", "txt", str(fixture)), name
            assert sgr.sub("", colored_manifest) == fixture.read_text(encoding="utf-8")
            lines = colored_manifest.splitlines()
            directive_color = color(lines[0], "on1xfile")
            for line, directive in zip(lines[1:], directives):
                head = directive.split()[0]
                assert color(line, head) == directive_color, head
            if name == "On1xfile":
                fixture.write_text("on1xfile 1\nproxy-none# comment\nproxy-none-extra\n", encoding="utf-8")
                _, none, unknown = run(*options, str(fixture)).splitlines()
                assert color(none, "proxy-none") == directive_color
                assert color(unknown, "proxy-none-extra") != directive_color

        source_file.write_text('  let bad = "unfinished\n  let cmd = `unfinished\n  return;\n', encoding="utf-8")
        recovered = run(*options, str(source_file)).splitlines()
        assert recovered[2] == colored[4], "single-line literals must not swallow subsequent code"
    print("On1x bat cache and highlighting checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
