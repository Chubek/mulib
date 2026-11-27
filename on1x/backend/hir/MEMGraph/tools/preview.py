#!/usr/bin/env python3
"""Preview MEMGraph rewrite matches on *untrusted abstract JSON*, never apply.

This is a candidate viewer, NOT a type/effect/borrow/FFI checker. The output
is intentionally marked UNVERIFIED_DO_NOT_PUBLISH. It must not be consumed
as approved HIR, compiler output, or FFI authorization evidence.
"""
import argparse
import copy
import json
from pathlib import Path
import sys

from mmgc import SpecError, compile_package


class Limit(Exception):
    pass


def match(pattern, value, env):
    if 'capture' in pattern:
        key = pattern['capture']
        if key in env and env[key] != value:
            return False
        env[key] = value
        return True
    if 'literal' in pattern:
        return isinstance(value, str) and value == pattern['literal']
    return (isinstance(value, dict) and value.get('op') == pattern['op']
            and isinstance(value.get('args'), list)
            and len(value['args']) == len(pattern['args'])
            and all(match(p, v, env) for p, v in zip(pattern['args'], value['args'])))


def instantiate(template, env):
    if 'capture' in template:
        return copy.deepcopy(env[template['capture']])
    if 'literal' in template:
        return template['literal']
    return {'op': template['op'], 'args': [instantiate(x, env) for x in template['args']]}


def inspect(graph, plan, max_matches=128, max_nodes=4096):
    visited = 0
    emitted = []
    for_passes = [p for p in plan['passes'] if p['mode'] == 'rewrite']
    work = [((), graph, 0)]
    while work:
        path, node, depth = work.pop()
        visited += 1
        if visited > max_nodes or depth > 64:
            raise Limit('graph visitation limit exceeded')
        if not isinstance(node, dict):
            continue
        for p in for_passes:
            for rule in p['rules']:
                if rule['kind'] != 'rewrite':
                    continue
                captures = {}
                if match(rule['match'], node, captures):
                    emitted.append({
                        'status': 'UNVERIFIED_DO_NOT_PUBLISH',
                        'pass': p['name'], 'rule': rule['name'],
                        'path': list(path),
                        'requires_unproven': rule['conditions'],
                        'preservation_unproven': rule['preserves'],
                        'before': node, 'candidate_after': instantiate(rule['replace'], captures),
                    })
                    if len(emitted) >= max_matches:
                        return emitted
        for index, item in reversed(list(enumerate(node.get('args', [])))):
            work.append((path + (index,), item, depth + 1))
    return emitted


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input', type=Path, help='read-only JSON demo graph')
    ap.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = ap.parse_args()
    try:
        if args.input.stat().st_size > 1024 * 1024:
            raise Limit('demo graph exceeds 1 MiB')
        data = json.loads(args.input.read_text('utf-8'))
        result = {'kind': 'MEMGraph_candidate_preview_v1',
                  'warning': 'NOT A COMPILER ADMISSION OR SAFETY PROOF',
                  'candidates': inspect(data, compile_package(args.root))}
        print(json.dumps(result, indent=2, sort_keys=True))
    except (OSError, ValueError, SpecError, Limit) as e:
        print(f'MEMGraph preview error: {e}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
