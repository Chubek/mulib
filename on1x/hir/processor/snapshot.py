#!/usr/bin/env python3
"""Regenerate/check the genuine LLSynth bootstrap parser and compiled tables."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from generate import compile_inputs, load_parser, publish


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def provenance(libstone, parser):
    scripts = libstone / "cornerstone/LLSynth/scripts"
    return {
        "generator": "LibStone cornerstone/LLSynth",
        "grammar_sha256": digest(HERE / "On1xMETA.llsynth"),
        "parser_sha256": digest(parser),
        "generator_sources": {p.name: digest(p) for p in sorted(scripts.iterdir())
                              if p.suffix in {".pl", ".pm"}},
    }


def check_parser(libstone):
    manifest = json.loads((HERE / "generated/parser_manifest.json").read_text())
    if manifest["grammar_sha256"] != digest(HERE / "On1xMETA.llsynth"):
        raise ValueError("stale bootstrap parser: grammar changed; run on1x_meta_regenerate")
    if manifest["parser_sha256"] != digest(HERE / "generated/On1xMETA.py"):
        raise ValueError("bootstrap parser digest mismatch")
    scripts = libstone / "cornerstone/LLSynth/scripts"
    if scripts.is_dir():
        for name, expected in manifest["generator_sources"].items():
            if not (scripts / name).is_file() or digest(scripts / name) != expected:
                raise ValueError("bootstrap parser differs from configured LLSynth: " + name)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--libstone", type=Path, default=HERE.parents[1] / "third_party/libstone")
    ap.add_argument("--work", type=Path, required=True)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--parser-only", action="store_true")
    args = ap.parse_args()
    try:
        if args.parser_only:
            check_parser(args.libstone)
            return 0
        args.work.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="snapshot-", dir=args.work) as temporary:
            work = Path(temporary)
            parser = work / "On1xMETA.py"
            subprocess.run([
                shutil.which("perl") or "perl",
                str(args.libstone / "cornerstone/LLSynth/scripts/llsynth.pl"),
                "--mode=interpret", "-T", "python3", "-o", str(parser),
                str(HERE / "On1xMETA.llsynth")], check=True, timeout=120)
            header, manifest = compile_inputs(load_parser(parser), [
                HERE / "canonicalize.o1m", HERE / "desugar.o1m", HERE / "semantic.o1m"])
            artifacts = {
                "On1xMETA.py": parser.read_text(),
                "parser_manifest.json": json.dumps(provenance(args.libstone, parser),
                                                   sort_keys=True, indent=2) + "\n",
                "on1x_meta_rules.hpp": header,
                "rules_manifest.json": manifest,
            }
            for name, text in artifacts.items():
                destination = HERE / "generated" / name
                if args.check:
                    if not destination.is_file() or destination.read_text() != text:
                        raise ValueError("stale generated artifact: " + str(destination))
                else:
                    publish(destination, text)
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        print("On1xMETA snapshot: " + str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
