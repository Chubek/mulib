#!/usr/bin/env python3
"""Generate the On1xMETA parser with relocatable LLSynth provenance."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from generate import publish


SPEC = re.compile(r'^SPEC = json\.loads\(base64\.b64decode\("([A-Za-z0-9+/=]+)"\)\)$',
                  re.MULTILINE)


def generate_parser(perl, llsynth, grammar, output):
    grammar = grammar.resolve()
    if output.resolve() in {grammar, llsynth.resolve()}:
        raise ValueError("parser output would overwrite an input")
    grammar_sha256 = hashlib.sha256(grammar.read_bytes()).hexdigest()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="llsynth-", dir=output.parent) as work:
        raw = Path(work).resolve() / "On1xMETA.py"
        subprocess.run([perl, str(llsynth), "--mode=interpret", "-T", "python3",
                        "-o", str(raw), str(grammar)], check=True, timeout=120)
        source = raw.read_text(encoding="utf-8")

    # Pinned LLSynth embeds argv and a canonical dependency path in its Python
    # SPEC. Even relative argv leaves the latter absolute. On1xMETA is a
    # self-contained grammar: accept only that one dependency and preserve its
    # content hash. Do not rewrite generated code, automata, or unknown paths.
    matches = list(SPEC.finditer(source))
    if len(matches) != 1:
        raise ValueError("unexpected LLSynth Python SPEC envelope")
    match = matches[0]
    spec = json.loads(base64.b64decode(match[1], validate=True))
    # Perl may serialize the native path's undecoded bytes as characters.
    path = spec.get("file") if isinstance(spec, dict) else None
    if (path not in (str(grammar), os.fsencode(grammar).decode("latin-1"))
            or spec.get("dependencies") != [{"path": path, "sha256": grammar_sha256}]):
        raise ValueError("unexpected LLSynth grammar provenance; expected one self-contained grammar")
    spec["file"] = spec["dependencies"][0]["path"] = grammar.name
    normalized = json.dumps(spec, sort_keys=True, ensure_ascii=False, separators=(",", ":"))
    encoded = base64.b64encode(normalized.encode("utf-8")).decode("ascii")
    publish(output, source[:match.start(1)] + encoded + source[match.end(1):])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--perl", required=True)
    ap.add_argument("--llsynth", type=Path, required=True)
    ap.add_argument("--grammar", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    try:
        generate_parser(args.perl, args.llsynth, args.grammar, args.output)
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        print("On1xMETA parser: " + str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
