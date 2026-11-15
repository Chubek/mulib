#!/usr/bin/env python3
"""MuLIB static archives and typed import manifests through real Exotract.

No generated On1x ABI guesses: Exotract emits typed metadata. C11 clients link
the generated static archive; On1x clients still require an admitted ABI adapter.
"""
import argparse
import importlib.util
import json
import re
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


def inventory(root):
    text = contracts.read(root/'MANIFEST.yaml').decode()
    names = re.findall(r'^- name: ([a-z0-9][a-z0-9_+-]*)$', text, re.M)
    if not names or len(names) != len(set(names)):
        raise ValueError('missing/duplicate MANIFEST module names')
    result = {}
    for name in names:
        item = contracts.module_info(root, name)
        header = Path(item['header'])
        if header != Path('include')/('mulib-'+name+'.h') or not (root/header).is_file():
            raise ValueError('missing/noncanonical module header: '+name)
        if not re.fullmatch(r'MULIB_IMPL_[A-Z0-9_]+', item['macro']):
            raise ValueError('invalid implementation macro: '+name)
        result[name] = item
    for item in result.values():
        if any(dep not in result for dep in item['manifest_dependencies']):
            raise ValueError('unknown MANIFEST dependency')
    return result


def plan(root, libraries):
    config = contracts.load(CATALOG)
    if config['schema'] != 'on1x.exotract-catalog/v1' or config['safe_export'] is not False:
        raise ValueError('unsupported Exotract catalog')
    modules = inventory(root)
    if not libraries or len(libraries) != len(set(libraries)) or any(n not in modules for n in libraries):
        raise ValueError('select unique known MuLIB modules')
    ordered, active, links, requirements = [], set(), set(), {}
    def visit(name):
        if name in active:
            raise ValueError('MANIFEST dependency cycle: '+name)
        if name in ordered:
            return
        active.add(name)
        if len(active)>128:
            raise ValueError('dependency depth limit')
        for dep in modules[name]['manifest_dependencies']:
            visit(dep)
        active.remove(name)
        ordered.append(name)
    for name in libraries:
        profile = config['profiles'].get(name, {'recipe': config['default_recipe']})
        if profile['recipe'] == 'exoml-core':
            exoml = contracts.catalog(root)
            core = exoml['profiles'][name]
            for dep in core['implementation_modules']:
                if dep not in ordered:
                    ordered.append(dep)
            links.update(core['link_libraries'])
        elif profile['recipe'] == 'manifest':
            visit(name)
        else:
            raise ValueError('unknown recipe')
        links.update(profile.get('link_libraries', []))
        requirements[name] = profile.get('required_symbols', [])
    entries = [dict(name=n, **modules[n]) for n in ordered]
    macros = [m['macro'] for m in entries]
    if len(macros) != len(set(macros)):
        raise ValueError('duplicate implementation switch')
    return dict(schema='on1x.exotract-plan/v1', libraries=libraries, modules=entries,
                implementation_macros=macros, implementation_translation_units=1,
                required_symbols=requirements, link_libraries=sorted(links), safe_export=False)


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
    recipe = plan(root, args.libraries)
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
    for path in [CATALOG, root/'MANIFEST.yaml', Path(__file__), HOME/'tools/on1x-contracts.py', exotract, compiler, ar, nm]:
        observe(path)
    for entry in recipe['modules']:
        observe(root/entry['header'])
        observe(root/entry['manual'])
    for name in args.libraries:
        if name in contracts.load(CATALOG)['profiles'] and contracts.load(CATALOG)['profiles'][name]['recipe']=='exoml-core':
            for path in contracts.CONTRACTS.iterdir():
                if path.is_file():observe(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.exotract-', dir=output.parent))
    try:
        imports={}
        for name in args.libraries:
            header=root/'include'/('mulib-'+name+'.h')
            common=[exotract, '--name', name, '--header', header, '--include', root/'include',
                    '--compiler', compiler, '--resource-dir', resource, '--preprocessor', 'compiler',
                    '--revision', 'sha256:'+contracts.digest(header), '--provenance', root/'MANIFEST.yaml',
                    '--provenance', CATALOG]
            common += discovery
            for symbol in recipe['required_symbols'][name]:common += ['--require-symbol', symbol]
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
        (stage/'mulib.c').write_bytes(invoke(command))
        (stage/'mulib.h').write_text('/* Declaration-only static binding surface. */\n#pragma once\n'+
            ''.join('#include <mulib-'+name+'.h>\n' for name in args.libraries))
        invoke([compiler,'-std=c11',*args.cflag,'-I',root/'include','-c',stage/'mulib.c','-o',stage/'mulib.o'])
        invoke([ar,'rcsD',stage/'libmulib.a',stage/'mulib.o'])
        # Header declarations can include optional APIs whose implementation
        # switch is not in this recipe. Record actual linker exports separately
        # while preserving Exotract's complete upstream metadata unmodified.
        symbols=invoke([nm,'--extern-only','--defined-only','--format=posix',stage/'mulib.o']).decode()
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
        artifacts={p.name:contracts.digest(p) for p in sorted(stage.iterdir())}
        (stage/'lock.json').write_text(contracts.dumps(dict(schema='on1x.exotract-lock/v1',
            safe_export=False, inputs=observed, artifacts=artifacts, compiler=str(compiler),
            compiler_version=invoke([compiler,'--version']).decode(), cflags=args.cflag,
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
    p=sub.add_parser('plan');p.add_argument('libraries',nargs='+')
    b=sub.add_parser('build');b.add_argument('libraries',nargs='+');b.add_argument('--output',type=Path,required=True)
    b.add_argument('--exotract',required=True);b.add_argument('--compiler',default='clang');b.add_argument('--ar',default='ar');b.add_argument('--nm',default='nm')
    b.add_argument('--resource-dir');b.add_argument('--cflag',action='append',default=[])
    args=cli.parse_args();root=args.root.resolve()
    try:
        if args.mode=='list':
            modules=inventory(root)
            known={m['header'] for m in modules.values()}
            auxiliary=sorted(str(p.relative_to(root)) for p in (root/'include').glob('mulib-*.h') if str(p.relative_to(root)) not in known)
            print(contracts.dumps(dict(schema='on1x.exotract-inventory/v1',modules=modules,auxiliary_headers=auxiliary)),end='')
        elif args.mode=='plan':print(contracts.dumps(plan(root,args.libraries)),end='')
        else:build(args,root);print(args.output)
    except (ValueError,OSError,KeyError,subprocess.TimeoutExpired) as error:
        print('on1x-exotract: '+str(error),file=sys.stderr);return 1
    return 0


if __name__=='__main__':sys.exit(main())
