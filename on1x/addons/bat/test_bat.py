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
    syntax_file = Path(__file__).resolve().parents[1] / "sublime/On1x.sublime-syntax"
    with tempfile.TemporaryDirectory(prefix="on1x-bat-") as temp:
        work = Path(temp)
        config = work / "config"
        syntax_dir = config / "syntaxes"
        syntax_dir.mkdir(parents=True)
        shutil.copyfile(syntax_file, syntax_dir / syntax_file.name)
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

        source_file.write_text('  let bad = "unfinished\n  let cmd = `unfinished\n  return;\n', encoding="utf-8")
        recovered = run(*options, str(source_file)).splitlines()
        assert recovered[2] == colored[4], "single-line literals must not swallow subsequent code"
    print("On1x bat cache and highlighting checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
