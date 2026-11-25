"""Check one independently generated scoped MIR bank and its bootstrap."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile


python, *args, parser, bootstrap = sys.argv[1:]
source = Path(args[args.index("--input") + 1])
parser, bootstrap = Path(parser), Path(bootstrap)
bank = source.read_text(encoding="utf-8")
with tempfile.TemporaryDirectory(prefix="mir-profile-", dir=parser.parent) as work:
    header = Path(work) / "table.hpp"
    candidate = Path(work) / source.name
    args[args.index("--input") + 1] = str(candidate)

    def compile_bank(text, mode, expected=""):
        candidate.write_text(text, encoding="utf-8")
        run = subprocess.run([python, *args, *mode, "--output", header],
                             capture_output=True, text=True, check=False)
        if expected:
            assert run.returncode != 0 and expected in run.stderr, run.stderr
        else:
            assert run.returncode == 0, run.stderr
            return header.read_bytes()

    generated = ["--parser", str(parser)]
    fallback = ["--bootstrap", str(bootstrap)]
    assert compile_bank(bank, generated) == compile_bank(bank, fallback) == bootstrap.read_bytes()
    compile_bank(bank + "\n# changed\n", fallback, "stale MIR policy")
    bad, changes = re.subn(r"(?m)^(pattern .*? replace )\S+", r"\g<1>-37", bank, count=1)
    assert changes == 1
    compile_bank(bad, generated, "replacement is not justified")
