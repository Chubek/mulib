"""Optional public ExoML SDK transport, real discovery and raw ABI integration."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from support import run

cli = argparse.ArgumentParser()
for name in ['compiler', 'root', 'work']:
    cli.add_argument(name, type=Path)
cli.add_argument('cc')
cli.add_argument('bind', type=Path)
cli.add_argument('--extract', action='store_true')
cli.add_argument('--cflag', action='append', default=[])
t = cli.parse_args()
t.root = t.root.resolve(); t.work.mkdir(parents=True, exist_ok=True)
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(path, value):
    path.write_text(json.dumps(value, ensure_ascii=True, sort_keys=True) + '\n')
    return path
def call(*args):
    return run([t.bind, *args])
def good(*args):
    p = call(*args)
    assert p.returncode == 0, (args, p.returncode, p.stderr)
    return p.stdout
def bad(*args, diagnostic=None):
    p = call(*args)
    assert p.returncode in [2, 3] and not p.stdout, (args, p.returncode, p.stdout, p.stderr)
    if diagnostic:
        assert diagnostic in p.stderr, (diagnostic, p.stderr)
    return p

with tempfile.TemporaryDirectory(prefix='sdk-', dir=t.work) as tmp:
    work = Path(tmp).resolve()
    header = work/'contract_abi.h'; shutil.copyfile(t.root/'tests/fixtures/contract_abi.h', header)
    span = {'file': str(header), 'line':1, 'column':1, 'end_line':1, 'end_column':1}
    # A compact, explicitly SYNTHETIC model exercises the public wire structs
    # even when libclang is not installed. It is never a real discovery proof.
    types = {}
    def typ(id, kind, spelling, **fields):
        types[id] = dict(id=id, canonical_id=id, spelling=spelling, canonical_spelling=spelling,
                         declaration_id='', kind=kind, const=False, volatile=False, restrict=False,
                         size=None, alignment=None, array_count=None, signed=None, element='', result='',
                         arguments=[], variadic=False, prototype=False, union=False, calling_convention=0,
                         attributes=[], unsupported_reason='', fields=[], enumerators=[], **{})
        types[id].update(fields)
        return id
    vi = typ('v', 'void', 'void')
    it = typ('i', 'integer', 'int', size=4, alignment=4, signed=True)
    short = typ('s', 'integer', 'short', size=2, alignment=2, signed=True)
    narrow = typ('n', 'typedef', 'fixture_short', size=2, alignment=2, element=short, canonical_id=short, canonical_spelling='short')
    ll = typ('l', 'integer', 'long long', size=8, alignment=8, signed=True)
    ui = typ('u', 'integer', 'unsigned int', size=4, alignment=4, signed=False)
    ul = typ('ul', 'integer', 'unsigned long', size=8, alignment=8, signed=False)
    ip = typ('p', 'pointer', 'int *', size=8, alignment=8, element=it)
    pair = typ('pair', 'record', 'struct contract_pair', size=8, alignment=4,
               fields=[{'name': n, 'type':it, 'source':span, 'offset_bits':off, 'bit_width':None} for n, off in [('first',0), ('second',32)]])
    typ('enum', 'enum', 'enum fixture_mode', size=4, alignment=4, element=it,
        enumerators=[{'name':'CONTRACT_FIXTURE_ZERO', 'value':'0'}])
    cb = typ('cb', 'function', 'int (int)', result=it, arguments=[it], prototype=True)
    cbp = typ('cbp', 'pointer', 'int (*)(int)', size=8, alignment=8, element=cb)
    shapes = [
        ('contract_add', it, [('a',it), ('b',it)]), ('contract_narrow', narrow, [('x',narrow)]),
        ('contract_note', vi, [('x',it)]), ('contract_count', it, []), ('contract_inline', ll, [('x',ll)]),
        ('contract_io', it, [('x',it)]), ('contract_unsigned', ui, [('x',ui)]),
        ('contract_pointer', ip, [('p',ip), ('n',ul)]), ('contract_pair', pair, [('pair',pair)]),
        ('contract_out', it, [('out',ip)]), ('contract_variadic', it, [('n',it)]),
        ('contract_callback', it, [('callback',cbp), ('x',it)]), ('contract_create', ip, []), ('contract_drop', vi, [('p',ip)])]
    declarations = []
    for name, result, args in shapes:
        f = typ('fn_'+name, 'function', name+' prototype', result=result, arguments=[ty for _,ty in args],
                prototype=True, variadic=name=='contract_variadic')
        declarations.append(dict(id='decl_'+name, name=name, kind='function', type=f, source=span,
                                 definition=name=='contract_inline', inline=name=='contract_inline', function_like_macro=False,
                                 linkage='internal' if name=='contract_inline' else 'external', tokens='', unsupported_reason='',
                                 parameters=[dict(name=n, type=ty, source=span, tokens='') for n,ty in args]))
    revision = 'sha256:'+sha(header)
    unit = dict(schema_version=2, extractor_version='exoml-c11-3', state='discovered', safe_export=False,
                unknown_effects=True, complete=True, header=str(header), revision=revision,
                fingerprint=hashlib.sha256(b'explicit synthetic metadata fixture').hexdigest(),
                target=dict(compiler='SYNTHETIC METADATA FIXTURE, NOT EXTRACTION', triple='x86_64-fixture', pointer_bits=64),
                compilation=dict(database=str(work/'compile_commands.json'), directory=str(work), file=str(header), driver='clang',
                                 original_arguments=['clang','-std=c11'], ignored_arguments=['-O2']),
                preprocessing=dict(coverage='SYNTHETIC TRANSPORT FIXTURE', final_snapshot_complete=True,
                                   definitions=[dict(id='m1', name='FIXTURE', tokens=['1'], source=span, function_like=False, builtin=False)],
                                   expansions=[dict(name='FIXTURE', definition_id='m1', unknown_reason='', source=span)],
                                   final_macros=[dict(name='FIXTURE', definition_id='m1', unknown_reason='', defined=True)],
                                   includes=[dict(included='fixture-only', source=span)], skipped_ranges=[span]),
                arguments=['-x','c','-std=c11'], inputs=[dict(path=str(header), sha256=sha(header))],
                types=[types[k] for k in sorted(types)], declarations=declarations,
                diagnostics=[dict(id='FIXTURE-NOTE', severity='note', message='not a discovery proof', source=span, related=[span])])
    decl = dump(work/'fixture.cdecl.json', unit)
    normalized = good('normalize-decl', decl)
    assert json.loads(normalized) == unit

    resource = ''
    if t.extract:
        clang = shutil.which('clang')
        if clang:
            p = run([clang, '-print-resource-dir']); assert p.returncode == 0, p.stderr
            resource = p.stdout.strip()
        extract_args = ['extract', header, '--revision', revision]
        if resource: extract_args += ['--resource-dir', resource]
        discovered = good(*extract_args)
        unit = json.loads(discovered); decl.write_text(discovered)
        assert json.loads(good('normalize-decl', decl)) == unit
        assert unit['complete'] and unit['target']['pointer_bits'] and unit['inputs']
        assert len([d for d in unit['declarations'] if d['kind']=='function']) == len(shapes)
        alt = json.loads(good(*extract_args, '--define', 'CONTRACT_ALT=1'))
        assert alt['fingerprint'] != unit['fingerprint']
        assert any(d['name']=='contract_conditional' for d in alt['declarations'])
        cross = json.loads(good(*extract_args, '--target', 'i386-unknown-linux-gnu'))
        assert cross['target']['pointer_bits'] == 32 and cross['fingerprint'] != unit['fingerprint']
        bad('extract', header, diagnostic='explicit source revision')
        bad(*extract_args, '--clang-arg', '-fplugin=/on1x/never-load')
        command_file = work/'command.c'; command_file.write_text('int value;\n')
        marker = work/'must-not-execute'
        database = dump(work/'compile_commands.json', [dict(directory=str(work), file=str(command_file),
                        command=f'clang -std=c11 -c {command_file}; touch {marker}')])
        bad(*extract_args, '--compile-commands', database, '--compile-file', command_file)
        assert not marker.exists()
        destination = work/'existing.json'; destination.write_text('user data')
        bad(*extract_args, '-o', destination, diagnostic='never replaced')
        assert destination.read_text() == 'user data'
    else:
        bad('extract', header, '--revision', revision, diagnostic='extract unavailable')

    provenance = dict(header=header.name, revision=revision, author='SYNTHETIC TEST FIXTURE — not an accountable review',
                      review_status='unreviewed', evidence_method='Metadata/ABI test fixture only; no safe-export proof')
    functions = []
    for name, result, args in shapes:
        fn = dict(c_name=name, return_=dict(kind='value', nullable=False),
                  parameters=[dict(index=i, name=n, ownership='scalar', nullable=False) for i,(n,_) in enumerate(args)],
                  errors=dict(kind='none'), effects=dict(effects=[], unknown=False),
                  concurrency=dict(thread_safety='single_thread', blocking_behavior='synchronous fixture', suspension_safe=False),
                  provenance=copy.deepcopy(provenance))
        fn['return'] = fn.pop('return_')
        if name == 'contract_io': fn['effects']['effects'] = ['io', 'foreign_call']
        if name == 'contract_pointer':
            fn['return'] = dict(kind='borrow', nullable=True, nullable_means_success=True, lifetime=dict(origin='parameter', parameter_index=0))
            fn['parameters'][0].update(ownership='borrowed_immutable', length_parameter=1)
            fn['invalidations'] = [dict(kind='owner_drop', description='Borrowed test pointer; not owned')]
        if name == 'contract_out':
            fn['parameters'][0]['ownership'] = 'out'
            fn['errors'] = dict(kind='boolean_return', out_valid_on_failure=False, cleanup_required_on_failure=False)
        if name == 'contract_callback':
            fn['parameters'][0]['ownership'] = 'opaque_unsafe'; fn['effects']['unknown'] = True
            fn['callbacks'] = [dict(signature='int(int)', mode='synchronous', reentrant=False, may_throw=False,
                                   lifetime=dict(origin='explicit_region', region_name='call'))]
            fn['concurrency'].update(callback_origin='same thread', registration_thread_affine=True, signal_safe=False)
        if name == 'contract_create':
            fn['return'] = dict(kind='owned', nullable=True, destroy_with='contract_drop', deallocator_family='fixture')
            fn['errors']['kind'] = 'null_on_failure'
            fn['destructors'] = [dict(function_name='contract_drop', allocator_family='fixture', null_safe=True, idempotent=False, thread_affine=True, may_fail=False)]
        if name == 'contract_drop':
            fn['parameters'][0]['ownership'] = 'consumed'
            fn['transfers'] = [dict(branch='unconditional', from_owner='p', to_owner='destroyed', predicate='every call')]
        functions.append(fn)
    seed = dict(schema_version=2, library='metadata_fixture', headers=[header.name], state='annotated',
                bundle_provenance=copy.deepcopy(provenance), functions=functions)
    annotation = dump(work/'fixture.seed.json', seed)
    pinned = work/'fixture.pinned.json'; pinned.write_text(good('pin', decl, annotation))
    bundle = json.loads(pinned.read_text())
    assert bundle['bundle_provenance']['toolchain_fingerprint'] == unit['fingerprint']
    assert all(f['provenance']['review_status']=='unreviewed' for f in bundle['functions'])
    report = json.loads(good('check', decl, pinned))
    assert report == json.loads(good('explain', decl, pinned))
    assert report['gate_b'] and not report['review_metadata'] and not report['safe_export']
    eligible = {f['name'] for f in report['functions'] if f['raw_scalar_abi_supported']}
    assert eligible == {'contract_add', 'contract_narrow', 'contract_note', 'contract_count', 'contract_inline', 'contract_io'}, report
    # Long valid-looking C identifiers must not drive a recursive std::regex
    # stack. They are bounded unsupported ABI metadata, not emitted C tokens.
    long_unit = copy.deepcopy(unit)
    add = next(d for d in long_unit['declarations'] if d['name']=='contract_add')
    argument_type = add['parameters'][0]['type']
    next(ty for ty in long_unit['types'] if ty['id']==argument_type)['spelling'] = 'A'*100000
    long_report = json.loads(good('check', dump(work/'long_type.json', long_unit), pinned))
    assert not next(f for f in long_report['functions'] if f['name']=='contract_add')['raw_scalar_abi_supported']
    long_unit = copy.deepcopy(unit); long_bundle = copy.deepcopy(bundle)
    next(d for d in long_unit['declarations'] if d['name']=='contract_add')['name'] = 'B'*100000
    long_bundle['functions'][0]['c_name'] = 'B'*100000
    long_report = json.loads(good('check', dump(work/'long_name.json', long_unit), dump(work/'long_contract.json', long_bundle)))
    assert not long_report['functions'][0]['raw_scalar_abi_supported']
    bad('verify', decl, pinned, diagnostic='review metadata')
    bad('emit-raw', decl, pinned, work/'denied')
    bad('emit-safe', decl, pinned, diagnostic='safe export unavailable')
    assert not list(work.glob('denied.*'))
    # Explicitly fake review flags exercise the metadata gate, not semantic truth.
    reviewed = copy.deepcopy(bundle); reviewed['state'] = 'reviewed'
    for fn in reviewed['functions']: fn['provenance']['review_status'] = 'reviewed'
    reviewed['bundle_provenance']['review_status'] = 'reviewed'
    reviewed_file = dump(work/'fixture.review-flags-only.json', reviewed)
    assert 'not a C semantic safety proof' in good('verify', decl, reviewed_file)
    assert json.loads(good('check', decl, reviewed_file))['safe_export'] is False
    bad('emit-safe', decl, reviewed_file, diagnostic='safe export unavailable')
    prefix = work/'raw'; good('emit-raw', decl, reviewed_file, prefix)
    manifest = json.loads((work/'raw.manifest.json').read_text())
    assert manifest['safe_export'] is False and manifest['identity']['fingerprint'] == unit['fingerprint']
    io = next(f for f in manifest['contracts']['functions'] if f['c_name']=='contract_io')
    assert set(io['effects']['effects']) == {'io', 'foreign_call'}
    assert json.loads(good('pin', decl, reviewed_file))['state'] == 'annotated'
    second = work/'raw_second'; good('emit', decl, reviewed_file, second)
    for suffix in ['.on1x', '.c', '.manifest.json']:
        assert Path(str(prefix)+suffix).read_bytes() == Path(str(second)+suffix).read_bytes()
    bad('emit-raw', decl, reviewed_file, prefix, diagnostic='already exists')
    blocked = work/'blocked'; (work/'blocked.c').symlink_to(header)
    saved = header.read_bytes(); bad('emit-raw', decl, reviewed_file, blocked)
    assert header.read_bytes() == saved and not (work/'blocked.on1x').exists()

    def native(name, body):
        source = work/(name+'.on1x'); out = work/(name+'.c'); exe = work/name
        source.write_text('import '+json.dumps(str(work/'raw.on1x'))+';fn main(){'+body+'}')
        p = run([t.compiler, '--emit-c', source, '-o', out]); assert p.returncode == 0, p.stderr
        p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', t.root/'runtime', '-I', work,
                 out, work/'raw.c', t.root/'tests/fixtures/contract_abi.c', t.root/'runtime/on1x_runtime.c', *t.cflag, '-o', exe])
        assert p.returncode == 0, p.stderr
        return exe
    exe = native('abi', 'println(on1x_ffi_contract_add(40,2));println(on1x_ffi_contract_narrow(-7));on1x_ffi_contract_note(7);println(on1x_ffi_contract_count());println(on1x_ffi_contract_inline(42));println(on1x_ffi_contract_io(42));')
    p = run([exe]); assert p.returncode == 0 and p.stdout == '42\n-7\n7\n42\n42\n', (p.returncode, p.stdout, p.stderr)
    exe = native('range', 'on1x_ffi_contract_narrow(32768);')
    p = run([exe]); assert p.returncode == 72 and 'out of range' in p.stderr and not p.stdout, p.stderr
    for name, before, after in [('signature', 'int contract_add(int a, int b);', 'unsigned int contract_add(int a, int b);'),
                                 ('size', 'typedef short fixture_short;', 'typedef long fixture_short;'),
                                 ('signedness', 'typedef short fixture_short;', 'typedef unsigned short fixture_short;')]:
        wrong = work/name; wrong.mkdir(); (wrong/header.name).write_text(header.read_text().replace(before, after))
        p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', wrong, '-c', work/'raw.c', '-o', wrong/'bad.o'])
        assert p.returncode != 0 and 'drift' in p.stderr, (name, p.stderr)

    def contract_reject(name, mutate, expected=None):
        value = copy.deepcopy(bundle); mutate(value)
        return bad('check', decl, dump(work/('bad_'+name+'.json'), value), diagnostic=expected)
    cases = [
        ('version', lambda b: b.update(schema_version=1)), ('unknown_root', lambda b: b.update(guessed_safe=True)),
        ('bad_state', lambda b: b.update(state='safe')),
        ('revision', lambda b: b['bundle_provenance'].update(revision='stale')),
        ('fingerprint', lambda b: b['functions'][0]['provenance'].update(toolchain_fingerprint='0'*64)),
        ('header', lambda b: b['bundle_provenance'].update(header='other.h')),
        ('count', lambda b: b['functions'][0]['parameters'].pop()),
        ('index', lambda b: b['functions'][0]['parameters'][1].update(index=0)),
        ('large_index', lambda b: b['functions'][0]['parameters'][0].update(index=1<<32)),
        ('param_name', lambda b: b['functions'][0]['parameters'][0].update(name='changed')),
        ('nullable_type', lambda b: b['functions'][0]['return'].update(nullable='false')),
        ('ownership', lambda b: b['functions'][0]['parameters'][0].update(ownership='safe')),
        ('effect', lambda b: b['functions'][0]['effects'].update(effects=['purity_guess'])),
        ('unknown_effect', lambda b: b['functions'][0]['effects'].pop('unknown')),
        ('duplicate_fn', lambda b: b['functions'].append(b['functions'][0])),
        ('declaration', lambda b: b['functions'][0].update(c_name='not_discovered')),
        ('borrow_source', lambda b: b['functions'][7]['return']['lifetime'].update(parameter_index=99)),
        ('missing_lifetime', lambda b: b['functions'][7]['return'].pop('lifetime')),
        ('implicit_lifetime_index', lambda b: b['functions'][7]['return']['lifetime'].pop('parameter_index')),
        ('length', lambda b: b['functions'][7]['parameters'][0].update(length_parameter=99)),
        ('count_pair', lambda b: b['functions'][7]['parameters'][0].update(count_parameter=99)),
        ('callback', lambda b: b['functions'][11]['callbacks'][0].update(mode='retained', unregister_function='')),
        ('callback_user', lambda b: b['functions'][11]['callbacks'][0].update(user_data_parameter=99)),
        ('region', lambda b: b['functions'][11]['callbacks'][0]['lifetime'].update(region_name='')),
        ('allocator', lambda b: b['functions'][12]['destructors'][0].update(allocator_family='mismatch')),
        ('destroy', lambda b: b['functions'][12]['return'].update(destroy_with='')),
        ('author', lambda b: b['functions'][0]['provenance'].update(author='')),
    ]
    for name, mutate in cases: contract_reject(name, mutate)
    nested_paths = [('functions',0), ('functions',0,'return'), ('functions',0,'parameters',0), ('functions',0,'errors'),
                    ('functions',0,'effects'), ('functions',0,'concurrency'), ('functions',0,'provenance'),
                    ('bundle_provenance',), ('functions',7,'return','lifetime'), ('functions',7,'invalidations',0),
                    ('functions',11,'callbacks',0), ('functions',12,'destructors',0), ('functions',13,'transfers',0)]
    for i, path in enumerate(nested_paths):
        def change(b, path=path):
            for key in path: b = b[key]
            b['unrecognized'] = True
        contract_reject('unknown_nested_'+str(i), change, 'unknown contract key')
    for i, wire in enumerate([b'{"schema_version":2,"schema_version":2}', b'{"x":NaN}', b'{"x":01}', b'{"x":"\xc0\x80"}',
                              b'{"x":"\xed\xa0\x80"}', b'{"x":"\\ud800"}', b'{"x":"\\udc00"}', b'['*65+b']'*65]):
        path = work/('malformed_'+str(i)+'.json'); path.write_bytes(wire)
        bad('check', decl, path)
    # Unicode transport is accepted, not merely rejected by an ASCII-only reader.
    unicode_bundle = copy.deepcopy(bundle); unicode_bundle['bundle_provenance']['author'] = 'author \U0001f9ea'
    assert json.loads(good('check', decl, dump(work/'unicode.json', unicode_bundle)))['gate_b']
    unicode_file = work/'unicode_raw.json'; unicode_file.write_text(json.dumps(unicode_bundle, ensure_ascii=False))
    assert json.loads(good('check', decl, unicode_file))['gate_b']

    for name, mutate in [
        ('incomplete', lambda u: u.update(complete=False)), ('safe_claim', lambda u: u.update(safe_export=True)),
        ('version', lambda u: u.update(schema_version=1)), ('target', lambda u: u['target'].update(pointer_bits=0)),
        ('inputs', lambda u: u.update(inputs=[])), ('hash', lambda u: u['inputs'][0].update(sha256='not-a-hash')),
        ('duplicate_input', lambda u: u['inputs'].append(u['inputs'][0])), ('types', lambda u: u.update(types=[])),
        ('dangling', lambda u: u['declarations'][0].update(type='absent')),
        ('type_kind', lambda u: u['types'][0].update(kind='fabricated')),
        ('unknown', lambda u: u['types'][0].update(ownership_guess='safe')),
    ]:
        value = copy.deepcopy(unit); mutate(value)
        bad('check', dump(work/('bad_decl_'+name+'.json'), value), pinned)

    if t.extract and (t.root.parent/'include/mulib-json.h').is_file() and sys.platform.startswith('linux'):
        tool = t.root/'tools/on1x-contracts.py'
        def workflow(*args): return run([sys.executable, tool, *args])
        options = ['--bind', t.bind]
        if resource: options += ['--resource-dir', resource]
        snapshot = work/'snapshot'
        p = workflow('snapshot', snapshot, *options); assert p.returncode == 0, p.stderr
        p = workflow('check', snapshot, '--bind', t.bind)
        assert p.returncode == 0 and json.loads(p.stdout)['safe_export'] is False, p.stderr
        second = work/'snapshot_two'; p = workflow('snapshot', second, *options); assert p.returncode == 0, p.stderr
        for file in snapshot.iterdir(): assert file.read_bytes() == (second/file.name).read_bytes(), file.name
        for library in ['json', 'vec', 'parsec', 'elvi-scrape', 'mune']:
            cdecl_file = snapshot/(library+'.cdecl.json'); contracts = snapshot/(library+'.exocontract.json')
            assert json.loads(good('normalize-decl', cdecl_file)) == json.loads(cdecl_file.read_text())
            bad('emit-safe', cdecl_file, contracts, diagnostic='safe export unavailable')
            bad('emit-raw', cdecl_file, contracts, work/('denied_'+library))
        p = workflow('snapshot', snapshot, *options)
        assert p.returncode == 3 and 'never overwritten' in p.stderr, p.stderr
        stale = work/'stale'; shutil.copytree(snapshot, stale)
        with (stale/'vec.exocontract.json').open('a') as stream: stream.write('\n')
        p = workflow('check', stale, '--bind', t.bind)
        assert p.returncode == 3 and 'artifact drift' in p.stderr, p.stderr
        fake = work/'canonical-copy'; (fake/'include').mkdir(parents=True); (fake/'manual').mkdir()
        for library in ['json','vec','parsec','elvi-scrape','mune']:
            for directory, suffix in [('include','.h'),('manual','.md')]:
                shutil.copyfile(t.root.parent/directory/('mulib-'+library+suffix), fake/directory/('mulib-'+library+suffix))
        catalog = json.loads((t.root/'contracts-exoml/catalog.json').read_text())
        for profile in catalog['profiles'].values():
            for path in profile.get('evidence_sha256',{}):
                (fake/path).parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(t.root.parent/path,fake/path)
        shutil.copyfile(t.root.parent/'MANIFEST.yaml', fake/'MANIFEST.yaml')
        locked = work/'copy-snapshot'
        p = workflow('--root', fake, 'snapshot', locked, *options, '--library', 'vec'); assert p.returncode == 0, p.stderr
        with (fake/'MANIFEST.yaml').open('a') as stream: stream.write('\n# provenance drift\n')
        p = workflow('--root', fake, 'check', locked, '--bind', t.bind)
        assert p.returncode == 3 and 'input drift' in p.stderr, p.stderr
    elif t.extract:
        print('Real canonical contract snapshot: SKIP (headers or Linux no-replace publication unavailable)')

print('ExoML transport, strict contracts, raw scalar C11 ABI and drift gates: PASS'+(' (real libclang discovery)' if t.extract else ' (synthetic transport; discovery disabled)'))
