#!/usr/bin/env python3
"""MuLIB static archives and typed import manifests through real Exotract.

No generated On1x ABI guesses: Exotract emits typed metadata. C11 clients link
the generated static archive; On1x clients still require an admitted ABI adapter.
"""
import argparse
import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HOME = Path(__file__).resolve().parents[1]
CATALOG = HOME/'contracts-exotract/catalog.json'
spec = importlib.util.spec_from_file_location('contracts', HOME/'tools/on1x-contracts.py')
contracts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contracts)
spec = importlib.util.spec_from_file_location('mulib_housekeeping', HOME/'std/mulib/housekeeping.py')
housekeeping = importlib.util.module_from_spec(spec)
spec.loader.exec_module(housekeeping)


def inventory(root):
    return housekeeping.inventory(root)


def plan(root, libraries, fixtures=()):
    if not libraries:raise ValueError('select MuLIB modules for typed extraction')
    recipe=housekeeping.plan(root,libraries,fixtures)
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


def build(args, root):
    recipe = plan(root, args.libraries, args.fixture)
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
        observe(root/entry['header'], entry['header_sha256'])
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
        requests=[(name,root/'include'/('mulib-'+name+'.h')) for name in args.libraries]
        requests += [('fixture-'+Path(path).parent.name+'-'+Path(path).stem,root/path) for path in recipe['include_headers']]
        for name,header in requests:
            common=[exotract, '--name', name, '--header', header, '--include', root/'include',
                    '--compiler', compiler, '--resource-dir', resource, '--preprocessor', 'compiler',
                    '--revision', 'sha256:'+contracts.digest(header), '--provenance', root/'MANIFEST.yaml',
                    '--provenance', CATALOG]
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
        command=[exotract,'emit-source','--name','MuLIB','--header',root/last['header']]
        for macro in recipe['implementation_macros']:command += ['--impl-define',macro]
        for entry in recipe['modules'][:-1]:command += ['--dependency',root/entry['header']]
        for header in recipe['include_headers']:command += ['--dependency',root/header]
        definitions=''.join('#define '+item.replace('=', ' ', 1)+'\n' for item in recipe['compile_definitions'])
        (stage/'mulib.c').write_bytes(definitions.encode()+invoke(command))
        (stage/'mulib.h').write_text('/* Declaration-only static binding surface. */\n#pragma once\n'+
            ''.join('#include <mulib-'+name+'.h>\n' for name in args.libraries)+
            ''.join('#include '+json.dumps(path)+'\n' for path in recipe['include_headers']))
        flags=[*args.cflag,'-I',str(root/'include'),*['-D'+item for item in recipe['compile_definitions']]]
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
    cli.add_argument('--root',type=Path,default=HOME.parent)
    sub=cli.add_subparsers(dest='mode',required=True)
    sub.add_parser('list')
    p=sub.add_parser('plan');p.add_argument('libraries',nargs='+');p.add_argument('--fixture',action='append',default=[])
    b=sub.add_parser('build');b.add_argument('libraries',nargs='+');b.add_argument('--output',type=Path,required=True)
    b.add_argument('--exotract',required=True);b.add_argument('--compiler',default='clang');b.add_argument('--ar',default='ar');b.add_argument('--nm',default='nm')
    b.add_argument('--resource-dir');b.add_argument('--cflag',action='append',default=[])
    b.add_argument('--fixture',action='append',default=[])
    args=cli.parse_args();root=args.root.resolve()
    try:
        if args.mode=='list':
            modules=inventory(root)
            known={m['header'] for m in modules.values()}
            auxiliary=sorted(str(p.relative_to(root)) for p in (root/'include').glob('mulib-*.h') if str(p.relative_to(root)) not in known)
            print(contracts.dumps(dict(schema='on1x.exotract-inventory/v1',modules=modules,auxiliary_headers=auxiliary)),end='')
        elif args.mode=='plan':print(contracts.dumps(plan(root,args.libraries,args.fixture)),end='')
        else:build(args,root);print(args.output)
    except (ValueError,OSError,KeyError,subprocess.TimeoutExpired) as error:
        print('on1x-exotract: '+str(error),file=sys.stderr);return 1
    return 0


if __name__=='__main__':sys.exit(main())
