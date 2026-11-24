#!/usr/bin/env python3
"""Genuine LLSynth native graph discovery and explicit unsupported controls."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import ffic

TARGET = 'target "x86_64-linux-gnu" LP64 "SysVAMD64" sysv;'
INT = 'integer(int,3,32,signed,4,4)'
REAL = 'real(double,binary64,64,8,8)'


def module(body, key='graph-test', alias='graph_test'):
    return (f'ffi 3; {TARGET} foreign mulib.{alias} {{'
            f' provider mulib "{key}"; header "mulib-{key}.h"; {body} }}')


def discovery(root, *, gid=9, symbol='mulib_graph_test_probe'):
    return (f'discover probe = c::{symbol} native external declaration graph {gid} '
            f'{root} reason rich_lowering_unavailable;')


def run(parser_path, directory):
    parser = ffic.load_parser(parser_path)
    source = directory / 'graph-test.ffi'

    def parse(text):
        return next(ffic.parse_document(parser, text.encode('utf-8'), source))

    tag = f'tag 72 struct "node" complete (16,8) {{ field "n" at 0 : {INT};' \
          ' field "next" at 8 : pointer(64,8,8,tag-ref(72)); }'
    enum = (f'tag 17 enum "options" complete (unknown,unknown) {{ underlying {INT};'
            ' case "NONE" = -1; case "A" = 3; }')
    root = f'function-type(fixed,(tag-ref(17),pointer(64,8,8,qualified(const,tag-ref(72)))),tag-ref(72))'
    disc = discovery(root)
    original = module(f'graph 9 {{ {enum} {tag} }} {disc}')
    mod = parse(original)
    assert len(mod['native_graphs']) == 1
    entry, = mod['entries']
    norm, tags = ffic.normalize_native_graph(entry['native_type'], {
        t['id']: t for t in mod['native_graphs'][0]['tags']
    })
    assert norm[3] == ['tag-ref', 0] and norm[4][0] == ['tag-ref', 1]
    assert tags[0]['fields'][1]['type'][4] == ['tag-ref', 0]
    assert tags[1]['underlying'] == ['integer', 'int', 3, 32, 'signed', 4, 4]
    assert tags[1]['layout'] is None and tags[1]['enumerators'] == [['NONE', -1], ['A', 3]]
    fingerprint = entry['native_fingerprint']
    assert ffic.DIGEST.fullmatch(fingerprint) and 'id' not in entry
    for variant in (
        original.replace('72', '8').replace('17', '33').replace('graph 9', 'graph 400'),
        module(f'graph 9 {{ {tag} tag 120 struct "unrelated" incomplete; {enum} }} {disc}'),
    ):
        assert parse(variant)['entries'][0]['native_fingerprint'] == fingerprint
    assert parse(original.replace(' = 3', ' = 4'))['entries'][0]['native_fingerprint'] != fingerprint
    assert parse(original.replace('qualified(const,tag-ref(72))', 'tag-ref(72)'))[
        'entries'][0]['native_fingerprint'] != fingerprint

    ipc_root = 'function-type(fixed,(pointer(64,8,8,pointer(64,8,8,tag-ref(0)))),'+INT+')'
    companion = module('companion framed_ipc_v1; graph 9 { tag 0 struct "on1x_unique" incomplete; } '
                       + discovery(ipc_root, symbol='on1x_kernel_ipc_v1_open_pair'),
                       'ipc-framed', 'ipc_framed')
    assert parse(companion)['declaration_source'] == ['companion', 'ipc-framed', 'framed_ipc_v1']
    public = discovery(f'function-type(fixed,(),{INT})', gid=2, symbol='mulib_ipc_framed_init').replace(
        'discover probe', 'discover init')
    mixed = companion.replace('companion framed_ipc_v1; graph 9', 'graph 2 {} graph 9 companion framed_ipc_v1')
    mixed = parse(mixed[:-1] + public + '}')
    assert [e['declaration_source'][0] for e in mixed['entries']] == ['companion', 'public']
    assert mixed['entries'][0]['native_fingerprint'] == parse(companion)['entries'][0]['native_fingerprint']
    audit = parse(module('unavailable reason gc_roots_and_callback_authority'
                         ' audit "Retention requires independent review.";'))
    assert audit['audit'] == 'Retention requires independent review.' and not audit['entries']
    extras = (
        f'tag 11 struct "flex" complete (4,4) {{ field "n" at 0 : {INT}; '
        f'field "data" at 4 : array(unknown,unknown,4,{INT}) flexible; }}'
        f'tag 12 union "bits" complete (4,4) {{ field "n" at 0 : {INT} bit-field(3,0,0); }}'
        f'tag 13 struct "array" complete (8,4) {{ field "arr" at 0 : array(2,8,4,{INT}); }}'
    )
    for mode in ('fixed', 'variadic'):
        parse(module(f'graph 9 {{ {extras} }} ' + discovery(
            f'function-type({mode},(pointer(64,8,8,function-type(unprototyped,(),void)),'
            f'pointer(64,8,8,tag-ref(11)),tag-ref(12),tag-ref(13)),{REAL})')))
    deep = 'pointer(64,8,8,' * 16 + 'void' + ')' * 16
    many = ' '.join('tag %d struct "nodes" incomplete;' % i for i in range(129))
    bad = (
        (original.replace('ffi 3;', 'ffi 2;'), 'requires ffi 3'),
        (original.replace('ffi 3;', 'ffi 4;'), 'unsupported FFI schema'),
        (original.replace('graph 9 {', 'graph 9 {} graph 9 {', 1), 'duplicate graph'),
        (original.replace(enum, enum + enum), 'duplicate tag'),
        (original.replace(disc, discovery(root, gid=15)), 'unknown wire graph'),
        (original.replace('tag-ref(17)', 'tag-ref(10)'), 'dangling tag'),
        (original.replace('pointer(64,8,8,tag-ref(72))', 'tag-ref(72)', 1), 'cyclic by-value'),
        (original.replace('at 8 :', 'at 16 :'), 'exceeds layout'),
        (original.replace('at 8 :', 'at 0 :'), 'overlaps'),
        (original.replace('pointer(64,8,8,tag-ref(72))', 'pointer(64,4,8,tag-ref(72))', 1), 'LP64 pointer'),
        (original.replace('qualified(const,', 'qualified(volatile+const,'), 'noncanonical qualifiers'),
        (original.replace('qualified(const,', 'qualified(restrict,'), 'invalid qualified'),
        (original.replace('at 0 :', 'at unknown :'), 'unknown complete field'),
        (original.replace('complete (unknown,unknown)', 'complete (4,4)'), 'enum layout'),
        (original.replace(' = 3', ' = 2147483648'), 'outside C int'),
        (original.replace(' = -1', ' = -0'), 'negative zero'),
        (original.replace('declaration graph', 'inline-only graph'), 'invalid inline-only'),
        (original.replace('reason rich_', 'specifiers(inline inline) reason rich_'), 'function specifiers'),
        (original.replace('reason rich_', 'specifiers(trusted) reason rich_'), 'function specifiers'),
        (original.replace('tag 72 struct "node" complete (16,8)', 'tag 72 struct "node" complete (18446744073709551616,8)'),
         'graph integer overflow'),
        (companion.replace('open_pair', 'forged'), 'outside declaration source'),
        (companion.replace('framed_ipc_v1', 'some_recipe'), 'closed companion'),
        (companion.replace(TARGET, 'target "aarch64-linux-gnu" LP64 "AAPCS64" aapcs64;'), 'closed companion'),
        (module('graph 9 {} ' + discovery(f'function-type(fixed,({deep}),void)')), 'nesting limit'),
        (module(f'graph 9 {{ {many} }} {disc}'), 'tag limit'),
        (module('graph 9 {} ' + discovery('function-type(fixed,(' + ','.join([INT]*17) + '),void)')),
         'parameter limit'),
        (module('graph 9 {} ' + discovery('function-type(variadic,(),void)')), 'prototype arity'),
    )
    for text, diagnostic in bad:
        try:
            parse(text)
        except ffic.Invalid as exc:
            assert diagnostic in str(exc), (diagnostic, str(exc))
        else:
            raise AssertionError('accepted invalid shape: ' + diagnostic)

    source.write_text(original)
    manifest = directory / 'manifest.json'
    header = directory / 'on1x_ffi_rules.hpp'
    command = [sys.executable, '-B', ffic.__file__, '--parser', str(parser_path),
               '--manifest', str(manifest), '--header', str(header), str(source)]
    subprocess.run(command, check=True, timeout=20)
    report = json.loads(manifest.read_text())
    assert not report['bindings'] and not report['grants_safe_admission']
    assert report['discovery'][0]['native_fingerprint'] == fingerprint
    assert b'array<Descriptor, 0>' in header.read_bytes() and b'mulib_graph_test_probe' not in header.read_bytes()
    before = (manifest.read_bytes(), header.read_bytes())
    source.write_text(original.replace('tag-ref(17)', 'tag-ref(10)'))
    failure = subprocess.run(command, capture_output=True, text=True, timeout=20)
    assert failure.returncode == 1 and 'dangling tag' in failure.stderr and 'Traceback' not in failure.stderr
    assert before == (manifest.read_bytes(), header.read_bytes())
    print(f'PASS: normalized recursive native graph, closed companion, audit; {len(bad)} invalid graphs; no executable entries.')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--parser', type=Path, required=True)
    ap.add_argument('--work', type=Path, required=True)
    args = ap.parse_args()
    with tempfile.TemporaryDirectory(dir=args.work) as tmp:
        run(args.parser.resolve(), Path(tmp).resolve())
