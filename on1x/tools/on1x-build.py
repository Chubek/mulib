#!/usr/bin/env python3
"""Build one On1x file through the checked C11 backend (no shell interpolation).

Installed helpers accept --mulib-root or ON1X_MULIB_ROOT for kernel builds.
"""
import argparse
import importlib.util
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MULIB = ROOT.parent

class PpxAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        chain = getattr(namespace, self.dest, None)
        if chain is None:
            chain = []
            setattr(namespace, self.dest, chain)
        chain.append((option_string, values if isinstance(values, list) else [values]))

def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('input', type=Path)
    cli.add_argument('-o', '--output', type=Path, required=True)
    cli.add_argument('--on1xc', default=str(ROOT / 'build' / 'on1xc') if (ROOT / 'build' / 'on1xc').is_file() else shutil.which('on1xc') or 'on1xc')
    cli.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    cli.add_argument('--emit-only', action='store_true')
    cli.add_argument('--via-mir', action='store_true', help='use the verified scalar MIR-to-C11 path')
    cli.add_argument('--memory-check', action='store_true', help='require Chisel resource-usage admission')
    cli.add_argument('--verify-types', action='store_true', help='require Nerotypical structural HIR type verification')
    cli.add_argument('--optimize-ir', action='store_true', help='enable verified scalar passes (requires --via-mir)')
    cli.add_argument('--extra-c', action='append', default=[], help='extra C translation unit to compile/link')
    cli.add_argument('--cflag', action='append', default=[], help='explicit extra C compiler option')
    cli.add_argument('--ppx', nargs=2, action=PpxAction, dest='ppx_chain', metavar=('LABEL', 'PROGRAM'))
    cli.add_argument('--ppx-schema', nargs=2, action='append', default=[], metavar=('LABEL', 'FILE'))
    cli.add_argument('--allow-source-ppx', action='store_true')
    cli.add_argument('--ppx-timeout-ms', type=int, default=5000)
    cli.add_argument('-I', '--module-path', action='append', default=[])
    cli.add_argument('--stdlib', type=Path)
    cli.add_argument('--no-prelude', action='store_true')
    cli.add_argument('--prelude', action='append', default=[])
    cli.add_argument('--stdppx', action=PpxAction, dest='ppx_chain', metavar='PROCESSOR')
    cli.add_argument('--mulib-root', type=Path, default=Path(os.environ.get('ON1X_MULIB_ROOT', MULIB)))
    cli.add_argument('--mulib', action='append', default=[], metavar='KERNEL')
    cli.add_argument('--mulib-fixture', action='append', default=[], metavar='GROUP')
    cli.add_argument('--mulib-archive', type=Path, help='locked on1x-exotract output directory')
    cli.add_argument('--mulib-plan', type=Path, help='write the resolved native link plan and input fingerprints')
    args = cli.parse_args()
    if args.optimize_ir and not args.via_mir:
        cli.error('--optimize-ir requires --via-mir')
    if args.mulib_plan and args.mulib_plan.resolve() in (args.input.resolve(),args.output.resolve()):
        cli.error('--mulib-plan must be separate from the input and executable')
    with tempfile.TemporaryDirectory(prefix='on1x-build-') as temp:
        generated = Path(temp) / 'main.c'
        compiler = [args.on1xc, '--emit-ir-c' if args.via_mir else '--emit-c', str(args.input), '-o', str(generated),
                    '--ppx-timeout-ms', str(args.ppx_timeout_ms)]
        if args.allow_source_ppx:
            compiler.append('--allow-source-ppx')
        if args.optimize_ir:
            compiler.append('--optimize-ir')
        if args.memory_check:
            compiler.append('--memory-check')
        if args.verify_types:
            compiler.append('--verify-types')
        if args.no_prelude:
            compiler.append('--no-prelude')
        if args.stdlib:
            compiler += ['--stdlib', str(args.stdlib)]
        for path in args.module_path:
            compiler += ['-I', path]
        for path in args.prelude:
            compiler += ['--prelude', path]
        for option, arguments in args.ppx_chain or []:
            compiler += [option, *arguments]
        for label, schema in args.ppx_schema:
            compiler += ['--ppx-schema', label, schema]
        subprocess.run(compiler, check=True)
        code = generated.read_text()
        if args.emit_only:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(code)
            return
        args.output.parent.mkdir(parents=True, exist_ok=True)
        mulib_root = args.mulib_root.resolve()
        cmd = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
               '-I', str(ROOT / 'runtime'), '-I', str(mulib_root / 'include'),
                str(generated), str(ROOT / 'runtime/on1x_runtime.c')]
        has_json = 'on1x_json_' in code
        externs=set(re.findall(r'\bextern\s+[^;\n]*\b(on1x_mulib_\w+)\(',code))
        has_mulib = any(re.fullmatch(r'on1x_mulib_(abi_version|pointer_bits|max_size|(?:kernel|fixture)_\w+|has_kernel|has_fixture)',name) for name in externs)
        bridge_groups={
            'JSON':(['json'], ('json_is_valid','json_normalize','json_array_length')),
            'YAML':(['yaml'], ('yaml_to_json',)),
            'TOML':(['toml'], ('toml_to_json',)),
            'INI':(['ini'], ('ini_has','ini_get')),
            'CRYPTO':(['cryptohash'], ('blake3_hex','blake2s_hex')),
            'SOCKET':(['socket'], ('socket_connect_ipv4','socket_send','socket_send_from','socket_recv','socket_recv_hex','socket_close')),
            'UNIX':([], ('unix_getpid','unix_getuid')),
        }
        groups={group for group,(_,symbols) in bridge_groups.items() if any('on1x_mulib_'+s in externs for s in symbols)}
        has_bridge=bool(groups) or 'on1x_mulib_bridge_status' in externs
        native = has_json or has_mulib or has_bridge or args.mulib or args.mulib_fixture or args.mulib_archive or args.mulib_plan
        recipe, fingerprints, native_links = None, {}, []
        if native:
            spec = importlib.util.spec_from_file_location('mulib_housekeeping', ROOT/'std/mulib/housekeeping.py')
            house = importlib.util.module_from_spec(spec);spec.loader.exec_module(house)
            cc=shutil.which(args.cc)
            if not cc:raise ValueError('C compiler unavailable: '+args.cc)
            cc=Path(cc).resolve()
            compiler_version=subprocess.check_output([str(cc),'--version'],text=True,timeout=30)
            compiler_target=subprocess.check_output([str(cc),'-dumpmachine'],text=True,timeout=30).strip()
            for path in [cc,Path(__file__).resolve(),Path(house.__file__)]:fingerprints[str(path)]=house.contracts.digest(path)
            kernels = list(args.mulib)
            for group in sorted(groups):
                for kernel in bridge_groups[group][0]:
                    if kernel not in kernels:kernels.append(kernel)
            if has_json and 'json' not in kernels:kernels.append('json')
            recipe = house.plan(mulib_root, kernels, args.mulib_fixture) if kernels or args.mulib_fixture or args.mulib_archive else None
            if any('MULIB_IMPL_' in flag for flag in args.cflag):
                raise ValueError('MuLIB implementation macros belong to the single generated TU')
            if args.mulib_archive:
                archive = args.mulib_archive.resolve()
                lock = house.contracts.load(archive/'lock.json')
                if lock['schema']!='on1x.exotract-lock/v1' or lock['safe_export'] is not False:
                    raise ValueError('unsupported MuLIB archive lock')
                if not {'build-plan.json','libmulib.a','symbols.json'}<=set(lock['artifacts']):
                    raise ValueError('incomplete MuLIB archive lock')
                for name, digest in lock['artifacts'].items():
                    if house.contracts.digest(house.local(archive,name))!=digest:raise ValueError('MuLIB archive artifact drift: '+name)
                for path, digest in lock['inputs'].items():
                    if house.contracts.digest(path)!=digest:raise ValueError('MuLIB archive input drift: '+path)
                    fingerprints[path]=digest
                saved = house.contracts.load(archive/'build-plan.json')
                if saved['schema']!='on1x.exotract-plan/v1' or saved['safe_export'] is not False or saved['implementation_translation_units']!=1:
                    raise ValueError('unsupported MuLIB archive plan')
                if not set(recipe['implementation_macros'])<=set(saved['implementation_macros']):
                    raise ValueError('MuLIB archive does not supply requested kernel/fixture implementations')
                for field in ['native_sources','include_headers','link_libraries']:
                    if not set(recipe[field])<=set(saved[field]):raise ValueError('MuLIB archive lacks requested '+field)
                if not {f['path'] for f in recipe['files']}<={f['path'] for f in saved['files']}:
                    raise ValueError('MuLIB archive lacks requested fixture files')
                if args.cflag!=lock['cflags'] or compiler_target!=lock['compiler_target']:
                    raise ValueError('MuLIB archive compiler flags/target differ; rebuild with matching --cflag options and target')
                recipe = saved
                native_links.append(str(archive/'libmulib.a'))
                fingerprints[str(archive/'libmulib.a')]=house.contracts.digest(archive/'libmulib.a')
            if recipe:
                fingerprints.update(recipe['inputs'])
                house.check_generated(mulib_root, recipe)
                for record in recipe['files']:
                    if house.contracts.digest(house.local(mulib_root,record['path']))!=record['sha256']:
                        raise ValueError('MuLIB fixture/archive mismatch: '+record['path'])
                    fingerprints[str(house.local(mulib_root,record['path']))]=record['sha256']
                for module in recipe['modules']:
                    for field in ['header','manual']:
                        path=house.local(mulib_root,module[field])
                        if house.contracts.digest(path)!=module[field+'_sha256']:raise ValueError('MuLIB kernel/archive mismatch: '+str(path))
                        fingerprints[str(path)]=module[field+'_sha256']
                cmd += ['-D'+item for item in recipe['compile_definitions']]
                if not args.mulib_archive:
                    impl = Path(temp)/'mulib.c';impl.write_text(house.implementation(mulib_root,recipe));cmd.append(str(impl))
                    cmd += [str(mulib_root/path) for path in recipe['native_sources']]
                native_links += ['-l'+link for link in recipe['link_libraries']]
                for extra in args.extra_c:
                    path=Path(extra)
                    if path.suffix=='.c' and path.is_file():
                        defines=set(re.findall(r'^\s*#\s*define\s+(MULIB_IMPL_[A-Z0-9_]+)\b',path.read_text(),re.M))
                        if defines & set(recipe['implementation_macros']):raise ValueError('duplicate MuLIB implementation in --extra-c: '+extra)
            if has_json:cmd.append(str(ROOT/'runtime/on1x_json.c'))
            if has_bridge:
                bridge=ROOT/'stdlib/_bridge/mulib_bridge.c'
                if bridge.resolve() not in {Path(p).resolve() for p in args.extra_c}:cmd.append(str(bridge))
                cmd += ['-DON1X_BRIDGE_'+group for group in sorted(groups)]
            if has_mulib:
                metadata=Path(temp)/'mulib_metadata.c'
                metadata.write_text(house.metadata_source(mulib_root,recipe or {'modules':[], 'files':[]}))
                cmd.append(str(metadata))
                cmd += ['-I',str(ROOT/'std/mulib')]
        library = (args.stdlib or Path(os.environ.get('ON1X_STDLIB', ROOT / 'stdlib'))).resolve()
        adapters = []
        if re.search(r'\bextern\s+[^;\n]*\bbasis_native_\w+\(', code):
            adapters += [library / 'runtime/basis_native.c', library / 'runtime/basis_real.c']
        if re.search(r'\bextern\s+[^;\n]*\bon1x_ppx_clock_ns\(', code):
            adapters += [library.parent / 'stdppx/runtime/on1x_ppx_timing.c']
        supplied = {Path(path).resolve() for path in args.extra_c}
        cmd += [str(path) for path in adapters if path.resolve() not in supplied]
        cmd += args.extra_c
        if adapters:
            cmd += ['-lm']
        cmd += args.cflag + native_links
        if native:
            flags=['-I',str(ROOT/'runtime'),'-I',str(mulib_root/'include'),'-I',str(ROOT/'std/mulib'),*args.cflag]
            if recipe:flags += ['-D'+item for item in recipe['compile_definitions']]
            flags += ['-DON1X_BRIDGE_'+group for group in sorted(groups)]
            for source in [Path(value) for value in cmd if value.endswith('.c')]:
                for path,digest in house.compile_inputs(args.cc,flags,source).items():
                    if path in fingerprints and fingerprints[path]!=digest:raise ValueError('native input drift: '+path)
                    fingerprints[path]=digest
        binary=Path(temp)/'program'
        cmd += ['-o', str(binary)]
        subprocess.run(cmd, check=True)
        if native:
            for path, digest in fingerprints.items():
                if house.contracts.digest(path)!=digest:raise ValueError('native input drift: '+path)
            if args.mulib_plan:
                # Generated temporary inputs are captured by content, not stale temp paths.
                stable={path:digest for path,digest in fingerprints.items() if not Path(path).is_relative_to(Path(temp))}
                generated_inputs={Path(path).name:digest for path,digest in fingerprints.items() if Path(path).is_relative_to(Path(temp))}
                report=dict(schema='on1x.mulib-link/v1',plan=recipe,inputs=stable,generated_inputs=generated_inputs,
                            compiler=str(cc),compiler_version=compiler_version,compiler_target=compiler_target,
                            cflags=args.cflag,bridge_groups=sorted(groups),
                            executable_sha256=house.contracts.digest(binary),safe_export=False)
                args.mulib_plan.parent.mkdir(parents=True,exist_ok=True)
                args.mulib_plan.write_text(house.contracts.dumps(report))
        with tempfile.NamedTemporaryFile(dir=args.output.resolve().parent,prefix='.on1x-publish-',delete=False) as stream:
            staged=Path(stream.name)
        try:
            shutil.copy2(binary,staged)
            os.replace(staged,args.output.resolve())
        finally:
            staged.unlink(missing_ok=True)

if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
    except (ValueError, OSError, KeyError, subprocess.TimeoutExpired) as e:
        print('on1x-build: '+str(e),file=sys.stderr)
        sys.exit(1)
