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

class OrderedAction(argparse.Action):
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
    cli.add_argument('--admitted-extern', type=Path, action='append', default=[],
                     help='reviewed scalar-only extern evidence (requires --memory-check)')
    cli.add_argument('--verify-types', action='store_true', help='require Nerotypical structural HIR type verification')
    cli.add_argument('--optimize-ir', action='store_true', help='enable verified scalar passes (requires --via-mir)')
    cli.add_argument('--extra-c', action='append', default=[], help='extra C translation unit to compile/link')
    cli.add_argument('--cflag', action='append', default=[], help='explicit extra C compiler option')
    cli.add_argument('--ppx', nargs=2, action=OrderedAction, dest='ppx_chain', metavar=('LABEL', 'PROGRAM'))
    cli.add_argument('--ppx-schema', nargs=2, action='append', default=[], metavar=('LABEL', 'FILE'))
    cli.add_argument('--allow-source-ppx', action='store_true')
    cli.add_argument('--ppx-timeout-ms', type=int, default=5000)
    cli.add_argument('-I', '--module-path', action=OrderedAction, dest='search_chain')
    cli.add_argument('--stdlib', type=Path)
    for option in ('lib-path', 'stdlib-path', 'ppx-path', 'stdppx-path'):
        cli.add_argument('--'+option, action=OrderedAction, dest='search_chain', metavar='DIR:DIR')
    cli.add_argument('--mulib-path', action='append', default=[], metavar='DIR:DIR')
    cli.add_argument('--no-prelude', action='store_true')
    cli.add_argument('--prelude', action='append', default=[])
    cli.add_argument('--stdppx', action=OrderedAction, dest='ppx_chain', metavar='PROCESSOR')
    cli.add_argument('--use-ppx', action=OrderedAction, dest='ppx_chain', metavar='PROCESSOR')
    cli.add_argument('--enable', action='append', default=[], metavar='FEATURES')
    cli.add_argument('--disable', action='append', default=[], metavar='FEATURES')
    cli.add_argument('--mulib-root', type=Path)
    cli.add_argument('--mulib', action='append', default=[], metavar='KERNEL')
    cli.add_argument('--mulib-fixture', action='append', default=[], metavar='GROUP')
    cli.add_argument('--mulib-archive', type=Path, help='locked on1x-exotract output directory')
    cli.add_argument('--mulib-plan', type=Path, help='write the resolved native link plan and input fingerprints')
    cli.add_argument('--inspect-kernels', action='store_true',
                     help='publish KernelBind discovery JSON without building a program or archive')
    cli.add_argument('--kernel-bind', action='store_true', help='build with the unsafe clamp KernelBind C11 profile')
    cli.add_argument('--exotract', help='trusted Lua-enabled Exotract executable for KernelBind')
    cli.add_argument('--resource-dir', help='Clang resource directory for KernelBind')
    args = cli.parse_args()
    if args.optimize_ir and not args.via_mir:
        cli.error('--optimize-ir requires --via-mir')
    if args.admitted_extern and not args.memory_check:
        cli.error('--admitted-extern requires --memory-check')
    if args.mulib_plan and args.mulib_plan.resolve() in (args.input.resolve(),args.output.resolve()):
        cli.error('--mulib-plan must be separate from the input and executable')
    if args.inspect_kernels or args.kernel_bind:
        if not args.exotract:
            cli.error('KernelBind requires --exotract PATH (LibStone Lua/ExoML build)')
    if args.inspect_kernels:
        if args.kernel_bind:
            cli.error('--inspect-kernels and --kernel-bind are mutually exclusive')
        if (args.emit_only or args.via_mir or args.memory_check or args.verify_types or args.extra_c or
            args.ppx_chain or args.ppx_schema or args.allow_source_ppx or args.mulib or args.mulib_fixture or
            args.mulib_archive or args.mulib_plan or
            any(option in ('--ppx-path', '--stdppx-path') for option, _ in args.search_chain or [])):
            cli.error('--inspect-kernels cannot be combined with build, linkage or PPX options')
        if args.output.exists() or args.output.is_symlink():
            cli.error('KernelBind discovery destination already exists')
    elif args.kernel_bind:
        if (args.emit_only or args.via_mir or args.memory_check or args.verify_types or
            args.extra_c or args.ppx_chain or args.ppx_schema or args.allow_source_ppx or args.enable or
            args.mulib or args.mulib_fixture or args.mulib_archive):
            cli.error('--kernel-bind is restricted to direct C11 with no user C, PPX, extensions, or additional kernels')
        if any(flag not in ('-O0','-O1','-O2','-O3','-g','-fno-omit-frame-pointer',
                           '-fsanitize=address,undefined','-fno-sanitize-recover=all') for flag in args.cflag):
            cli.error('--kernel-bind permits only the documented optimization/debug/sanitizer flags')
        for path in [args.output, args.mulib_plan]:
            if path is not None and (path.exists() or path.is_symlink()):
                cli.error('KernelBind requires fresh output and plan paths')
    elif args.exotract or args.resource_dir:
        cli.error('--exotract and --resource-dir require KernelBind')
    with tempfile.TemporaryDirectory(prefix='on1x-build-') as temp:
        generated = Path(temp) / ('kernel-requests.json' if args.inspect_kernels else 'main.c')
        mode = '--kernel-imports' if args.inspect_kernels else '--emit-ir-c' if args.via_mir else '--emit-c'
        compiler = [args.on1xc, mode, str(args.input), '-o', str(generated),
                    '--ppx-timeout-ms', str(args.ppx_timeout_ms)]
        if args.allow_source_ppx:
            compiler.append('--allow-source-ppx')
        if args.optimize_ir:
            compiler.append('--optimize-ir')
        if args.memory_check:
            compiler.append('--memory-check')
        for evidence in args.admitted_extern:
            compiler += ['--admitted-extern', str(evidence)]
        if args.verify_types:
            compiler.append('--verify-types')
        if args.no_prelude:
            compiler.append('--no-prelude')
        if args.stdlib:
            compiler += ['--stdlib', str(args.stdlib)]
        loader_flags=[]
        if args.stdlib:loader_flags += ['--stdlib',str(args.stdlib)]
        processor_paths={'--ppx-path':[], '--stdppx-path':[]}
        for option,arguments in args.search_chain or []:
            compiler += [option,*arguments]
            if option in ('-I','--module-path','--lib-path','--stdlib-path'):
                loader_flags += [option,*arguments]
            if option in processor_paths:processor_paths[option] += arguments
        for path in args.prelude:
            compiler += ['--prelude', path]
        for option, arguments in args.ppx_chain or []:
            compiler += [option, *arguments]
        for name in args.enable:
            compiler += ['--enable', name]
        for name in args.disable:
            compiler += ['--disable', name]
        for label, schema in args.ppx_schema:
            compiler += ['--ppx-schema', label, schema]
        kernel_report = kernel_interface = None
        if args.inspect_kernels or args.kernel_bind:
            spec = importlib.util.spec_from_file_location('on1x_exotract', ROOT/'tools/on1x-exotract.py')
            exo = importlib.util.module_from_spec(spec);spec.loader.exec_module(exo)
            compiler[0] = exo.executable(compiler[0])
            driver_pins = {path: exo.contracts.digest(path) for path in [compiler[0], Path(__file__).resolve()]}
            requests_path = Path(temp)/'kernel-requests.json'
            inspect = [str(compiler[0]), '--kernel-imports', str(args.input), '-o', str(requests_path), *compiler[5:]]
            subprocess.run(inspect, check=True, timeout=60)
            paths = exo.housekeeping.search_paths(args.mulib_path)
            root = exo.housekeeping.kernel_root(args.mulib_root, MULIB, paths)
            requests = exo.contracts.load(requests_path)
            report = exo.kernel_discovery(requests, root, paths, args.exotract, args.cc, args.resource_dir,
                                          [] if args.kernel_bind else args.cflag, driver_pins)
            if args.inspect_kernels:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                with tempfile.NamedTemporaryFile(dir=args.output.parent, prefix='.kernel-discovery-', delete=False) as stream:
                    staged = Path(stream.name)
                try:
                    staged.write_text(exo.contracts.dumps(report), encoding='ascii')
                    exo.contracts.publish_new(staged, args.output)
                finally:
                    staged.unlink(missing_ok=True)
                return
            kernel_report = report
            kernel_interface = exo.unsafe_scalar_interface(report)
            args.cc = report['compiler']
            compiler.append('--kernel-c11')
        subprocess.run(compiler, check=True)
        code = generated.read_text()
        if args.emit_only:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(code)
            return
        args.output.parent.mkdir(parents=True, exist_ok=True)
        spec = importlib.util.spec_from_file_location('mulib_housekeeping', ROOT/'stdlib/mulib/housekeeping.py')
        house = importlib.util.module_from_spec(spec);spec.loader.exec_module(house)
        paths=house.search_paths(args.mulib_path)
        mulib_root=house.kernel_root(args.mulib_root,MULIB,paths)
        includes=house.include_paths(mulib_root,paths)
        include_flags=[part for path in includes for part in ['-I',str(path)]]
        cmd = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
               '-I', str(ROOT / 'runtime'), *include_flags,
                str(generated), str(ROOT / 'runtime/on1x_runtime.c')]
        has_json = 'on1x_json_' in code
        has_csp = '#include <on1x_concurrency.h>' in code
        has_pretty = bool(re.search(r'^extern [^;\n]*\bon1x_pretty_(?:render|status)\(', code, re.M))
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
        native = kernel_interface or has_json or has_csp or has_pretty or has_mulib or has_bridge or args.mulib or args.mulib_fixture or args.mulib_archive or args.mulib_plan
        recipe, fingerprints, native_links = None, {}, []
        if native:
            cc=shutil.which(args.cc)
            if not cc:raise ValueError('C compiler unavailable: '+args.cc)
            cc=Path(cc).resolve()
            compiler_version=subprocess.check_output([str(cc),'--version'],text=True,timeout=30)
            compiler_target=subprocess.check_output([str(cc),'-dumpmachine'],text=True,timeout=30).strip()
            for path in [cc,Path(__file__).resolve(),Path(house.__file__)]:fingerprints[str(path)]=house.contracts.digest(path)
            kernels = list(args.mulib)
            if kernel_interface:
                if has_json or has_csp or has_pretty or has_mulib or has_bridge:
                    raise ValueError('KernelBind clamp profile cannot be mixed with built-in raw kernel bridges')
                if compiler_target != kernel_report['target'] or root != mulib_root:
                    raise ValueError('KernelBind target or MuLIB root drift')
                fingerprints.update(kernel_report['inputs'])
                kernels.append('clamp')
                cmd.append('-DON1X_KERNEL_ABI_V1=1')
            if has_csp:
                kernels += [name for name in ('channel', 'fiber', 'xthreads') if name not in kernels]
                cmd.append(str(ROOT/'runtime/on1x_concurrency.c'))
            for group in sorted(groups):
                for kernel in bridge_groups[group][0]:
                    if kernel not in kernels:kernels.append(kernel)
            if has_json and 'json' not in kernels:kernels.append('json')
            if has_pretty and 'prettysexpr' not in kernels:kernels.append('prettysexpr')
            recipe = house.plan(mulib_root, kernels, args.mulib_fixture,paths) if kernels or args.mulib_fixture or args.mulib_archive else None
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
                locked={m['name']:m['header_sha256'] for m in saved['modules']}
                if any(locked.get(m['name'])!=m['header_sha256'] for m in recipe['modules']):
                    raise ValueError('MuLIB archive kernel search-path/header mismatch')
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
                if kernel_interface and recipe['implementation_macros'] != ['MULIB_IMPL_CLAMP']:
                    raise ValueError('KernelBind implementation closure drift')
                if kernel_interface:
                    for path, digest in recipe['inputs'].items():
                        if fingerprints.get(path) != digest:
                            raise ValueError('KernelBind discovery/link input drift: '+path)
                fingerprints.update(recipe['inputs'])
                house.check_generated(mulib_root, recipe)
                for record in recipe['files']:
                    if house.contracts.digest(house.local(mulib_root,record['path']))!=record['sha256']:
                        raise ValueError('MuLIB fixture/archive mismatch: '+record['path'])
                    fingerprints[str(house.local(mulib_root,record['path']))]=record['sha256']
                for module in recipe['modules']:
                    for field in ['header','manual']:
                        path=house.header_path(mulib_root,module) if field=='header' else house.local(mulib_root,module[field])
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
            if has_pretty:
                bridge = Path(subprocess.check_output(
                    [args.on1xc, '--stdlib-resource', 'prettyprint/runtime.c', *loader_flags], text=True).strip())
                if bridge.resolve() not in {Path(p).resolve() for p in args.extra_c}:cmd.append(str(bridge))
            if has_bridge:
                bridge=ROOT/'stdlib/_bridge/mulib_bridge.c'
                if bridge.resolve() not in {Path(p).resolve() for p in args.extra_c}:cmd.append(str(bridge))
                cmd += ['-DON1X_BRIDGE_'+group for group in sorted(groups)]
            if has_mulib:
                metadata=Path(temp)/'mulib_metadata.c'
                metadata.write_text(house.metadata_source(mulib_root,recipe or {'modules':[], 'files':[]}))
                cmd.append(str(metadata))
                cmd += ['-I',str(ROOT/'stdlib/mulib')]
        # Ask the compiler, rather than implementing a second manifest resolver.
        library = Path(subprocess.check_output([args.on1xc,'--print-stdlib',*loader_flags],text=True).strip())
        adapters = []
        if re.search(r'\bextern\s+[^;\n]*\bbasis_native_\w+\(', code):
            for file in ('runtime/basis_native.c','runtime/basis_real.c'):
                adapters.append(Path(subprocess.check_output(
                    [args.on1xc,'--stdlib-resource',file,*loader_flags],text=True).strip()))
        if re.search(r'\bextern\s+[^;\n]*\bon1x_ppx_clock_ns\(', code):
            ppx_roots=[*house.search_paths(processor_paths['--stdppx-path'],'ON1X_STDPPX_PATH'),
                       *house.search_paths(processor_paths['--ppx-path'],'ON1X_PPX_PATH'),library.parent/'stdppx',ROOT/'stdppx']
            timing=next((p/'runtime/on1x_ppx_timing.c' for p in ppx_roots if (p/'runtime/on1x_ppx_timing.c').is_file()),None)
            if timing is None:raise ValueError('PPX timing runtime not found in configured paths')
            adapters += [timing]
        supplied = {Path(path).resolve() for path in args.extra_c}
        cmd += [str(path) for path in adapters if path.resolve() not in supplied]
        cmd += args.extra_c
        if adapters:
            cmd += ['-lm']
        cmd += args.cflag + native_links
        if native:
            flags=['-I',str(ROOT/'runtime'),*include_flags,'-I',str(ROOT/'stdlib/mulib'),*args.cflag]
            if recipe:flags += ['-D'+item for item in recipe['compile_definitions']]
            flags += ['-DON1X_BRIDGE_'+group for group in sorted(groups)]
            if kernel_interface:flags.append('-DON1X_KERNEL_ABI_V1=1')
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
                if kernel_interface:report['kernel_interface']=kernel_interface
                args.mulib_plan.parent.mkdir(parents=True,exist_ok=True)
                if kernel_interface:
                    with tempfile.NamedTemporaryFile(dir=args.mulib_plan.parent, prefix='.kernel-plan-', delete=False) as stream:
                        staged_plan = Path(stream.name)
                    try:
                        staged_plan.write_text(house.contracts.dumps(report), encoding='ascii')
                        house.contracts.publish_new(staged_plan, args.mulib_plan)
                    finally:
                        staged_plan.unlink(missing_ok=True)
                else:
                    args.mulib_plan.write_text(house.contracts.dumps(report))
        with tempfile.NamedTemporaryFile(dir=args.output.resolve().parent,prefix='.on1x-publish-',delete=False) as stream:
            staged=Path(stream.name)
        try:
            shutil.copy2(binary,staged)
            if kernel_interface:
                house.contracts.publish_new(staged, args.output)
            else:
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
