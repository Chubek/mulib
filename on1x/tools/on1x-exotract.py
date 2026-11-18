#!/usr/bin/env python3
"""MuLIB static archives and typed import manifests through real Exotract.

No generated On1x ABI guesses: Exotract emits typed metadata. C11 clients link
the generated static archive; On1x clients still require an admitted ABI adapter.
"""
import argparse
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HOME = Path(__file__).resolve().parents[1]
CATALOG = HOME/'contracts-exotract/catalog.json'
KERNEL_PROFILE = HOME/'contracts-exotract/kernel-discovery-v1.lua'
CLAMP_EXPORTS = ('mulib_clamp_add_i64', 'mulib_clamp_sub_i64')
# Profile v1's known entry points are sanity checks, not binding allowlists.
KERNEL_MODULES = {
    'clamp': (None, list(CLAMP_EXPORTS)),
    'histprice': (None, ['mulib_histprice_ema', 'mulib_histprice_holt']),
    'statsprice': (None, ['mulib_statsprice_estimate']),
    'priceopt': (None, ['mulib_priceopt_optimize', 'mulib_priceopt_candidates']),
    'money': (None, ['mulib_money_make']),
    'json': ('json.exocontract.json', []),
    'prettysexpr': ('prettysexpr.exocontract.json', ['mulib_pretty_render_expr', 'mulib_pretty_parse_expr']),
    'ini': (None, ['mulib_ini_parse']),
    'cryptohash': (None, ['mulib_blake3', 'mulib_blake2s']),
    'socket': (None, ['mulib_socket_connect', 'mulib_socket_pair', 'mulib_socket_send',
                      'mulib_socket_receive', 'mulib_socket_close']),
    'channel': (None, ['mulib_channel_create', 'mulib_channel_sender', 'mulib_channel_receiver',
                       'mulib_channel_send', 'mulib_channel_receive', 'mulib_channel_close',
                       'mulib_channel_destroy']),
}
spec = importlib.util.spec_from_file_location('contracts', HOME/'tools/on1x-contracts.py')
contracts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contracts)
spec = importlib.util.spec_from_file_location('mulib_housekeeping', HOME/'stdlib/mulib/housekeeping.py')
housekeeping = importlib.util.module_from_spec(spec)
spec.loader.exec_module(housekeeping)


def inventory(root):
    return housekeeping.inventory(root)


def plan(root, libraries, fixtures=(), paths=()):
    if not libraries:raise ValueError('select MuLIB modules for typed extraction')
    recipe=housekeeping.plan(root,libraries,fixtures,paths)
    recipe['schema']='on1x.exotract-plan/v1'
    return recipe


def invoke(argv):
    p = subprocess.run([str(a) for a in argv], capture_output=True, timeout=180)
    if p.returncode:
        raise ValueError(f'{argv[0]} failed ({p.returncode}): '+p.stderr[:65536].decode(errors='replace'))
    if len(p.stdout)>64*1024*1024:
        raise ValueError('tool output limit')
    return p.stdout


def executable(value):
    resolved = shutil.which(str(value))
    if not resolved:
        raise ValueError('executable unavailable: '+str(value))
    return Path(resolved).resolve()

def kernel_discovery(requests, root, paths, exotract, compiler, resource_dir, cflags, pins_before_load):
    """Resolve *compiler* source requests. This produces no interface or link input."""
    def location(request):
        return f"{request['file']}:{request['line']}:{request['column']}: "

    if requests['schema'] != 'on1x.kernel-requests/v1':
        raise ValueError('unsupported compiler KernelBind request schema')
    sources = {os.fsdecode(bytes.fromhex(s['file_hex'])): s['sha256'] for s in requests['sources']}
    imports = [{k: v for k, v in r.items() if k != 'file_hex'} |
               {'file': os.fsdecode(bytes.fromhex(r['file_hex']))} for r in requests['requests']]
    if not imports or len(imports) > 1024:
        raise ValueError('KernelBind inspection requires 1..1024 source requests')
    # Do not automatically enable arbitrary manifest recipes or executable inputs.
    selected = {}
    for request in imports:
        match = re.fullmatch(r'mulib\.([a-z][a-z0-9_]*)', request['module'])
        if not match or match[1] not in KERNEL_MODULES:
            raise ValueError(location(request)+'ON1X-KERNEL-PROFILE: no discovery profile: '+request['module'])
        selected.setdefault(match[1], request)
    observed = {}

    def observe(path, expected=None):
        real = Path(path).resolve()
        digest = contracts.digest(real)
        if (expected is not None and digest != expected) or (real in observed and digest != observed[real]):
            raise ValueError('KernelBind discovery input drift: '+str(real))
        observed[real] = digest
        return real

    for path, digest in sources.items():
        observe(path, digest)
    for path, digest in pins_before_load.items():
        observe(path, digest)
    manifest_input = observe(housekeeping.local(root, 'MANIFEST.yaml'))
    lua_profile = observe(KERNEL_PROFILE)
    exotract, compiler = map(executable, [exotract, compiler])
    for tool in [exotract, compiler, Path(__file__)]:
        observe(tool)
    resource = Path(resource_dir or invoke([compiler, '-print-resource-dir']).decode().strip()).resolve()
    if not (resource/'include/stddef.h').is_file():
        raise ValueError('provide --resource-dir from a compatible clang')
    target = invoke([compiler, '-dumpmachine']).decode().strip()
    defines, undefines, flags = [], [], []
    for flag in cflags:
        if 'MULIB_IMPL_' in flag:
            raise ValueError('implementation macros belong to the generated TU, not KernelBind discovery')
        if flag.startswith('-D') and len(flag) > 2:
            defines.append(flag[2:])
        elif flag.startswith('-U') and len(flag) > 2:
            undefines.append(flag[2:])
        else:
            flags.append(flag)  # The real Exotract SDK allowlists ABI flags.

    discovered = {}
    for name, request in selected.items():
        try:
            recipe = plan(root, [name], paths=paths)
            for path, digest in recipe['inputs'].items():
                observe(path, digest)
            if recipe['dependency_cycles'] or recipe['native_sources'] or recipe['include_headers'] or recipe['generator_checks']:
                raise ValueError('discovery profile has unsupported build requirements')
            modules = {}
            for item in recipe['modules']:
                header = housekeeping.local(root, item['header'])
                if root/item['header'] != header:
                    raise ValueError('symlinked MuLIB kernel header: '+item['header'])
                # Reject additional candidates, even identical copies or broken symlinks.
                # Use the actual path rather than only comparing content hashes.
                for directory in recipe['include_directories']:
                    candidate = Path(directory)/Path(item['header']).name
                    if candidate != root/item['header'] and (candidate.exists() or candidate.is_symlink()):
                        raise ValueError('ambiguous MuLIB kernel header: '+str(candidate))
                if Path(housekeeping.header_path(root, item)) != header:
                    raise ValueError('noncanonical MuLIB header: '+item['header'])
                doc = observe(housekeeping.local(root, item['manual']), item['manual_sha256'])
                modules[item['name']] = (observe(header, item['header_sha256']), doc, item['header_sha256'])
            header, manual, revision = modules[name]
            sidecar = None
            sidecar_name, required = KERNEL_MODULES[name]
            required = sorted(set(required + recipe['required_symbols'][name]))
            pins = [manifest_input, lua_profile, manual, *(h for mod, (h, _, _) in modules.items() if mod != name)]
            if sidecar_name:
                sidecar = observe(housekeeping.local(contracts.CONTRACTS, sidecar_name))
                pins.append(sidecar)
            # Do not pass ExoLib's `target` override: it adds --target to C
            # preprocessing and GCC does not accept that option. In this
            # native-only profile compare its actual CDecl target below.
            globs = dict(KERNEL=name, HEADER=header, MANUAL=manual, REVISION='sha256:'+revision,
                         COMPILER=compiler, RESOURCE=resource)
            if sidecar:
                globs['CONTRACT'] = sidecar
            dependencies = [h for mod, (h, _, _) in modules.items() if mod != name]
            for key, values in dict(INCLUDES=[root/'include'], PINS=pins,
                                    REQUIRED=required, FLAGS=flags, DEFINES=defines,
                                    UNDEFINES=undefines, DEPENDENCIES=dependencies,
                                    IMPL=recipe['implementation_macros']).items():
                globs[key+'_COUNT'] = len(values)
                globs.update({f'{key}_{i}': v for i, v in enumerate(values, 1)})
            argv = [exotract, 'lua', lua_profile]
            for k, v in globs.items():
                if '$GLOBAL{' in str(v):
                    raise ValueError('nested ExoLib global substitution is not allowed')
                argv += ['--global', f'{k}={v}']
            wire = contracts.loads(invoke(argv))
            profile, on1x, contract = (wire[key] for key in ('profile', 'on1x', 'contract'))
            decl = on1x['cdecl']
            if (profile['schema'] != 'moonstone.exotract/2' or profile['name'] != name or
                profile['header'] != str(header) or profile['revision'] != globs['REVISION'] or
                profile['required_symbols'] != required or
                profile['safe_export'] is not False or profile['complete'] is not True or
                profile['syntax_checked'] is not True or
                on1x['schema'] != 'moonstone.exotract.on1x/2' or on1x['library'] != name or
                on1x['unsafe_by_default'] is not True or on1x['requires_review'] is not True or
                decl['schema_version'] != 2 or decl['complete'] is not True or decl['safe_export'] is not False or
                decl['header'] != str(header) or decl['revision'] != globs['REVISION'] or
                decl['target']['triple'] != target):
                raise ValueError('incomplete or mismatched ExoLib/CDecl identity')
            impl = profile['header_only']
            if (impl['implementation_translation_units'] != 1 or
                impl['implementation_defines'] != recipe['implementation_macros'] or
                impl['dependencies'] != [str(h) for h in dependencies]):
                raise ValueError('mismatched ExoLib implementation profile')
            decl_inputs = set()
            for record in decl['inputs']:
                path = observe(record['path'], record['sha256'])
                if path in decl_inputs:
                    raise ValueError('duplicate CDecl input')
                decl_inputs.add(path)
            if not {header, *pins} <= decl_inputs:
                raise ValueError('incomplete CDecl provenance pins')
            preprocessing = decl['preprocessing']
            if (preprocessing['final_snapshot_complete'] is not True or
                any(m['defined'] and m['name'].startswith('MULIB_IMPL_') for m in preprocessing['final_macros'])):
                raise ValueError('not a complete declaration-only CDecl macro snapshot')
            if sidecar:
                # Reject duplicate keys even if an alternate SDK reader were
                # permissive. Only ExoML interprets semantic fields.
                contracts.load(sidecar)
            if (sidecar is None and contract is not None) or (sidecar is not None and (
                not isinstance(contract, dict) or contract['schema_version'] != 2 or contract['library'] != name or
                contract['bundle_provenance']['revision'] != globs['REVISION'])):
                raise ValueError('mismatched ExoML semantic sidecar')
            status = {entry['name']: dict(status='missing_review', callable=False,
                       reason=('no independent KernelBind authorization' if sidecar else
                               'no ExoML semantic sidecar or independent KernelBind authorization'))
                      for entry in decl['declarations'] if entry['kind'] == 'function'}
            type_ids = {entry['id'] for entry in decl['types'] if entry['kind'] == 'function'}
            for entry in on1x['imports']:
                if (entry['safe'] is not False or entry['c_name'] not in status or entry['c_type_id'] not in type_ids):
                    raise ValueError('unexpected Exotract import metadata')
            discovered['mulib.'+name] = dict(exolib=profile, metadata=on1x, contract=contract, callable=False,
                                           status='missing_review', safe_export=False, symbols=status)
        except (ValueError, OSError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
            raise ValueError(location(request)+'ON1X-KERNEL-DISCOVERY: '+str(error)) from error
    compiler_version = invoke([compiler, '--version']).decode()
    for path, digest in list(observed.items()):
        observe(path, digest)
    return dict(schema='on1x.kernel-discovery/v1', safe_export=False, requests=imports,
                modules=discovered, inputs={str(p): h for p, h in observed.items()},
                exotract=str(exotract), compiler=str(compiler), target=target, cflags=cflags,
                compiler_version=compiler_version, resource_directory=str(resource))


def unsafe_scalar_interface(report):
    """Project the exact, deliberately narrow executable CDecl shape.

    Only use an in-process discovery result with pinned inputs; this function
    is not a reader for public discovery reports or admission certificates.
    """
    for request in report['requests']:
        if not request['unsafe'] or request['module'] != 'mulib.clamp':
            code = 'UNSAFE-UNSUPPORTED' if request['unsafe'] else 'NOT-ADMITTED'
            raise ValueError(f"{request['file']}:{request['line']}:{request['column']}: "
                             f"ON1X-KERNEL-{code}: {request['module']}")
    module = report['modules']['mulib.clamp']
    cdecl = module['metadata']['cdecl']
    types = {t['id']: t for t in cdecl['types']}
    if (len(types) != len(cdecl['types']) or not cdecl['fingerprint'] or
        not re.fullmatch(r'x86_64(?:-[a-z0-9_]+)?-linux-gnu', report['target']) or
        cdecl['target']['pointer_bits'] != 64 or
        module['exolib']['header_only']['implementation_defines'] != ['MULIB_IMPL_CLAMP']):
        raise ValueError('ON1X-KERNEL-ABI: unsupported clamp CDecl target or implementation profile')

    def scalar(type_id):
        t = types[type_id]
        c = types[t['canonical_id']]
        return (t['kind'] == 'typedef' and t['spelling'] == 'int64_t' and
                c['kind'] == 'integer' and c['signed'] is True and
                all(x['size'] == 8 and x['alignment'] == 8 and
                    not (x['const'] or x['volatile'] or x['restrict'] or x['attributes'] or
                         x['unsupported_reason']) for x in (t, c)))

    exports = {}
    for symbol in CLAMP_EXPORTS:
        candidates = [d for d in cdecl['declarations'] if d['name'] == symbol]
        if len(candidates) != 1:
            raise ValueError('ON1X-KERNEL-ABI: missing or ambiguous declaration for '+symbol)
        d = candidates[0]
        f = types[d['type']]
        if (d['kind'] != 'function' or d['definition'] or d['inline'] or d['linkage'] != '4' or
            d['source']['file'] != cdecl['header'] or d['unsupported_reason'] or
            f['kind'] != 'function' or f['calling_convention'] != 1 or f['variadic'] or
            not f['prototype'] or f['attributes'] or f['unsupported_reason'] or
            len(f['arguments']) != 2 or [p['type'] for p in d['parameters']] != f['arguments'] or
            not all(scalar(t) for t in [f['result'], *f['arguments']])):
            raise ValueError('ON1X-KERNEL-ABI: incompatible CDecl signature for '+symbol)
        exports[symbol.removeprefix('mulib_clamp_')] = dict(
            c_name=symbol, c_type_id=d['type'], declaration_id=d['id'],
            parameters=['i64', 'i64'], result='i64', status='unsafe', effect='unsafe_foreign')
    return dict(schema='on1x.kernel-interface/v1', profile='unsafe-clamp-i64/v1',
                module='mulib.clamp', safe_export=False, target=report['target'],
                cdecl_fingerprint=cdecl['fingerprint'], header=cdecl['header'],
                header_revision=cdecl['revision'], imports=report['requests'], exports=exports)


def build(args, root):
    recipe = plan(root, args.libraries, args.fixture,args.paths)
    housekeeping.check_generated(root, recipe)
    output = args.output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError('output already exists')
    exotract, compiler, ar, nm = map(executable, [args.exotract, args.compiler, args.ar, args.nm])
    # An implementation macro in command-line flags would corrupt declaration
    # discovery and bypass the exactly-one-TU model.
    if any('MULIB_IMPL_' in f for f in args.cflag):
        raise ValueError('implementation macros belong to the generated TU')
    # Exotract accepts only ABI switches in compiler_arguments. Definitions use
    # its dedicated API; these specific instrumentation options affect only
    # archive code generation and are separately recorded in the lock.
    codegen_only={'-g','-g0','-g1','-g2','-g3','-fsanitize=address,undefined',
                  '-fno-omit-frame-pointer','-fno-sanitize-recover=all'}
    discovery=[]
    for flag in args.cflag:
        if flag in codegen_only:continue
        if flag.startswith('-D') and len(flag)>2:discovery += ['--define',flag[2:]]
        elif flag.startswith('-U') and len(flag)>2:discovery += ['--undefine',flag[2:]]
        else:discovery += ['--compile-arg',flag]
    resource = args.resource_dir or invoke([compiler, '-print-resource-dir']).decode().strip()
    if not Path(resource).is_dir():
        raise ValueError('provide --resource-dir from a compatible clang')
    observed = {}
    def observe(path, expected=None):
        path = str(Path(path).resolve()); digest = contracts.digest(path)
        if expected and digest != expected or path in observed and observed[path] != digest:
            raise ValueError('input drift: '+path)
        observed[path] = digest
    for path in [CATALOG, housekeeping.CATALOG, Path(housekeeping.__file__), root/'MANIFEST.yaml', Path(__file__), HOME/'tools/on1x-contracts.py', exotract, compiler, ar, nm]:
        observe(path)
    for path,digest in recipe['inputs'].items():observe(path,digest)
    for entry in recipe['modules']:
        observe(housekeeping.header_path(root,entry), entry['header_sha256'])
        observe(root/entry['manual'], entry['manual_sha256'])
    for record in recipe['files']:observe(root/record['path'], record['sha256'])
    for name in args.libraries:
        if name in contracts.load(CATALOG)['profiles'] and contracts.load(CATALOG)['profiles'][name]['recipe']=='exoml-core':
            for path in contracts.CONTRACTS.iterdir():
                if path.is_file():observe(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.exotract-', dir=output.parent))
    try:
        housekeeping.materialize(root, recipe, stage)
        imports={}
        headers={m['name']:housekeeping.header_path(root,m) for m in recipe['modules']}
        requests=[(name,headers[name]) for name in args.libraries]
        requests += [('fixture-'+Path(path).parent.name+'-'+Path(path).stem,root/path) for path in recipe['include_headers']]
        for name,header in requests:
            common=[exotract, '--name', name, '--header', header,
                    '--compiler', compiler, '--resource-dir', resource, '--preprocessor', 'compiler',
                    '--revision', 'sha256:'+contracts.digest(header), '--provenance', root/'MANIFEST.yaml',
                    '--provenance', CATALOG]
            for directory in recipe['include_directories']:common += ['--include',directory]
            common += discovery
            profile = contracts.load(CATALOG)['profiles'].get(name, {})
            if profile.get('recipe') == 'exoml-core':
                core = contracts.catalog(root)['profiles'][name]
                for path in core.get('evidence_sha256', {}):
                    common += ['--provenance', root/path]
            for symbol in recipe['required_symbols'].get(name,[]):common += ['--require-symbol', symbol]
            try:
                wire=invoke([common[0], 'on1x', *common[1:]])
            except ValueError as error:
                # The upstream on1x command intentionally emits no incomplete
                # import manifest. Inspect retains the detailed diagnostics.
                diagnosis=subprocess.run([str(v) for v in [common[0], 'inspect', *common[1:]]],capture_output=True,timeout=180)
                raise ValueError(str(error)+'\n'+diagnosis.stdout[:65536].decode(errors='replace')) from error
            manifest=contracts.loads(wire)
            if manifest.get('schema')!='moonstone.exotract.on1x/2' or manifest.get('unsafe_by_default') is not True:
                raise ValueError('invalid Exotract On1x manifest')
            decl=manifest['cdecl']
            if decl.get('schema_version')!=2 or decl.get('complete') is not True or decl.get('header')!=str(header):
                raise ValueError('incomplete/mismatched CDecl')
            for item in decl['inputs']:observe(item['path'], item['sha256'])
            (stage/(name+'.on1x-import.json')).write_bytes(wire)
            (stage/(name+'.cdecl.json')).write_text(contracts.dumps(decl))
            imports[name]=[entry['c_name'] for entry in manifest['imports']]
        # All selected modules share one implementation TU, avoiding duplicate
        # definitions when several requested libraries depend on the same kernel.
        last=recipe['modules'][-1]
        command=[exotract,'emit-source','--name','MuLIB','--header',housekeeping.header_path(root,last)]
        for macro in recipe['implementation_macros']:command += ['--impl-define',macro]
        for entry in recipe['modules'][:-1]:command += ['--dependency',housekeeping.header_path(root,entry)]
        for header in recipe['include_headers']:command += ['--dependency',root/header]
        definitions=''.join('#define '+item.replace('=', ' ', 1)+'\n' for item in recipe['compile_definitions'])
        (stage/'mulib.c').write_bytes(definitions.encode()+invoke(command))
        (stage/'mulib.h').write_text('/* Declaration-only static binding surface. */\n#pragma once\n'+
            ''.join('#include <mulib-'+name+'.h>\n' for name in args.libraries)+
            ''.join('#include '+json.dumps(path)+'\n' for path in recipe['include_headers']))
        flags=[*args.cflag,*[part for p in recipe['include_directories'] for part in ['-I',p]],
               *['-D'+item for item in recipe['compile_definitions']]]
        objects=[]
        for i,source in enumerate([stage/'mulib.c',*[root/path for path in recipe['native_sources']]]):
            obj=stage/('mulib.o' if i==0 else 'fixture_'+str(i)+'.o')
            for path,digest in housekeeping.compile_inputs(compiler,flags,source).items():
                if not Path(path).is_relative_to(stage):observe(path,digest)
            invoke([compiler,'-std=c11',*flags,'-c',source,'-o',obj]);objects.append(obj)
        invoke([ar,'rcsD',stage/'libmulib.a',*objects])
        # Header declarations can include optional APIs whose implementation
        # switch is not in this recipe. Record actual linker exports separately
        # while preserving Exotract's complete upstream metadata unmodified.
        symbols=''.join(invoke([nm,'--extern-only','--defined-only','--format=posix',obj]).decode() for obj in objects)
        exports=set()
        for line in symbols.splitlines():
            fields=line.split()
            if len(fields)<2 or len(fields[1])!=1:
                raise ValueError('unsupported nm output')
            exports.add(fields[0])
        surfaces={name:dict(archive_exports=sorted(set(names)&exports),
                           declaration_only=sorted(set(names)-exports))
                  for name,names in imports.items()}
        for name,required in recipe['required_symbols'].items():
            if not set(required)<=exports:raise ValueError('required archive symbols missing: '+name)
        (stage/'symbols.json').write_text(contracts.dumps(dict(schema='on1x.exotract-symbols/v1',
            libraries=surfaces, defined_symbols=sorted(exports), safe_export=False)))
        (stage/'build-plan.json').write_text(contracts.dumps(recipe))
        for path,digest in observed.items():
            if contracts.digest(path)!=digest:raise ValueError('input changed before publication: '+path)
        artifacts={p.relative_to(stage).as_posix():contracts.digest(p) for p in sorted(stage.rglob('*')) if p.is_file()}
        (stage/'lock.json').write_text(contracts.dumps(dict(schema='on1x.exotract-lock/v1',
            safe_export=False, inputs=observed, artifacts=artifacts, compiler=str(compiler),
            compiler_version=invoke([compiler,'--version']).decode(), cflags=args.cflag,
            compiler_target=invoke([compiler,'-dumpmachine']).decode().strip(),
            declaration_options=discovery,
            resource_directory=resource)))
        contracts.publish_new(stage,output)
    finally:
        if stage.exists():shutil.rmtree(stage)


def main():
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--root',type=Path)
    cli.add_argument('--mulib-path',action='append',default=[],metavar='DIR:DIR')
    sub=cli.add_subparsers(dest='mode',required=True)
    sub.add_parser('list')
    p=sub.add_parser('plan');p.add_argument('libraries',nargs='+');p.add_argument('--fixture',action='append',default=[])
    b=sub.add_parser('build');b.add_argument('libraries',nargs='+');b.add_argument('--output',type=Path,required=True)
    b.add_argument('--exotract',required=True);b.add_argument('--compiler',default='clang');b.add_argument('--ar',default='ar');b.add_argument('--nm',default='nm')
    b.add_argument('--resource-dir');b.add_argument('--cflag',action='append',default=[])
    b.add_argument('--fixture',action='append',default=[])
    args=cli.parse_args()
    try:
        args.paths=housekeeping.search_paths(args.mulib_path)
        root=housekeeping.kernel_root(args.root,HOME.parent,args.paths)
        if args.mode=='list':
            modules=inventory(root)
            known={m['header'] for m in modules.values()}
            auxiliary=sorted(str(p.relative_to(root)) for p in (root/'include').glob('mulib-*.h') if str(p.relative_to(root)) not in known)
            print(contracts.dumps(dict(schema='on1x.exotract-inventory/v1',modules=modules,auxiliary_headers=auxiliary)),end='')
        elif args.mode=='plan':print(contracts.dumps(plan(root,args.libraries,args.fixture,args.paths)),end='')
        else:build(args,root);print(args.output)
    except (ValueError,OSError,KeyError,subprocess.TimeoutExpired) as error:
        print('on1x-exotract: '+str(error),file=sys.stderr);return 1
    return 0


if __name__=='__main__':sys.exit(main())
