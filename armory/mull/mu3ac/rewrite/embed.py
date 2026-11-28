#!/usr/bin/env python3
"""Reproduce the two host snapshots of the shared Mu3AC default rules."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = (root / 'rewrite/default.t').read_text()
    outputs = {
        'DefaultRules.hpp': '#pragma once\nnamespace mull::mu3ac {\n'
                            'inline constexpr auto default_rules = R"MU3AC(' + source + ')MU3AC";\n}\n',
        'DefaultRules.d': 'module mull.mu3ac.DefaultRules;\nimmutable default_rules = `' + source + '`;\n',
    }
    for name, content in outputs.items():
        path = root / name
        if args.check:
            if path.read_text() != content:
                raise SystemExit(f'{path}: embedded Mu3AC rules are stale')
        else:
            path.write_text(content)


if __name__ == '__main__':
    main()
