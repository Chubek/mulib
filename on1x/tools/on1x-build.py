#!/usr/bin/env python3
"""Build one On1x file with the selected verified backend (default: direct C11).

Installed helpers accept --mulib-root or ON1X_MULIB_ROOT for kernel builds.
"""
import argparse
import hashlib
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

def project_debug_flags(stage, generated_dir, mulib, stdlib):
    """Give project DWARF stable source paths without changing file lookups or C macros."""
    roots = {
        mulib.resolve(): '/__on1x__/mulib',
        ROOT: '/__on1x__/sdk',
        stdlib.resolve(): '/__on1x__/stdlib',
        stage.resolve(): '/__on1x__/build',
        Path(generated_dir).resolve(): '/__on1x__/generated',
    }
    # GCC and Clang take the last matching mapping. Prefer nested roots
    # (e.g. the SDK and stdlib in the MuLIB checkout) over their parents.
    flags = []
    cwd = Path.cwd()
    for root in sorted(roots, key=lambda path: len(path.parts)):
        prefix = str(root)
        if root == Path(root.anchor):
            raise ValueError('cannot remap an entire filesystem in project debug info: ' + prefix)
        # GCC and Clang disagree on which "=" terminates the old prefix.
        if '=' in prefix:
            raise ValueError('debug path remapping requires build and SDK paths without "=": ' + prefix)
        if root == cwd:
            flags.append(f'-fdebug-prefix-map={prefix}={roots[root]}')
        flags.append(f'-fdebug-prefix-map={prefix}/={roots[root]}/')
    return flags

def main():
    if len(sys.argv) > 1 and sys.argv[1] in ('init', 'exec'):
        from on1x_project import main as project_main
        return project_main(sys.argv[1:])
    cli = argparse.ArgumentParser(description=__doc__, epilog=
        'Projects: use "init --bin|--proj|--lib [DIRECTORY]" to scaffold, '
        'or "exec COMMAND" to run a command defined by an On1xfile.')
    cli.add_argument('input', type=Path)
    cli.add_argument('-o', '--output', type=Path, required=True)
    cli.add_argument('--on1xc', default=str(ROOT / 'build' / 'on1xc') if (ROOT / 'build' / 'on1xc').is_file() else shutil.which('on1xc') or 'on1xc')
    cli.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    cli.add_argument('--emit-only', action='store_true')
    backend_args = cli.add_mutually_exclusive_group()
    backend_args.add_argument('--backend', choices=('c11', 'mir-c11', 'mir-vm', 'mir-archon'),
                              help='output: direct C11, MIR C11, VM bytecode, or native Archon executable')
    backend_args.add_argument('--via-mir', dest='backend', action='store_const', const='mir-c11',
                              help='shorthand for --backend mir-c11')
    cli.add_argument('--memory-check', action='store_true', help='require Chisel resource-usage admission')
    cli.add_argument('--admitted-extern', type=Path, action='append', default=[],
                     help='reviewed scalar-only extern evidence (requires --memory-check)')
    cli.add_argument('--verify-types', action='store_true', help='require Nerotypical structural HIR type verification')
    cli.add_argument('--scrutiny', metavar='LEVEL', help='override source and project static-analysis scrutiny')
    cli.add_argument('--project-scrutiny', metavar='LEVEL', help=argparse.SUPPRESS)
    cli.add_argument('--scrutiny-work', type=int, metavar='UNITS',
                     help='bounded Wetstone work budget (1..10000000)')
    cli.add_argument('--no-analysis-repairs', action='store_true',
                     help='disable advisory Wetstone repair analysis')
    cli.add_argument('--diagnostic-catalog', type=Path, metavar='FILE',
                     help='validated external diagnostic catalog')
    cli.add_argument('--diagnostics', choices=('text', 'json', 'sarif'))
    cli.add_argument('--color', choices=('auto', 'always', 'never'))
    cli.add_argument('--optimize-ir', action='store_true', help='enable verified scalar passes (requires --via-mir)')
    import on1x_optimization as optimization
    optimization.arguments(cli)
    cli.add_argument('--extra-c', action='append', default=[], help='extra C translation unit to compile/link')
    cli.add_argument('--cflag', action='append', default=[], help='explicit extra C compiler option')
    cli.add_argument('--enable-pod-fixture', action='store_true',
                     help='authorize the fixed Linux Pod byte-echo worker (requires static C toolchain)')
    cli.add_argument('--debug-build-root', type=Path,
                     help='remap this project staging root, generated C and SDK source paths in debug info')
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
    cli.add_argument('--kernel-bind', action='store_true', help='build a supported KernelBind C11 profile')
    cli.add_argument('--kernel-review-candidate', action='store_true',
                     help='inspect a supported safe import and publish its unsigned review subject')
    cli.add_argument('--kernel-review', type=Path,
                     help='independent signed review; -o publishes an atomic directory containing program and evidence')
    cli.add_argument('--kernel-review-key', type=Path, help='operator-selected Ed25519 DER public trust anchor')
    cli.add_argument('--kernel-review-signature', type=Path, help='detached Ed25519 signature of the review file bytes')
    cli.add_argument('--exotract', help='trusted Lua-enabled Exotract executable for KernelBind')
    cli.add_argument('--resource-dir', help='Clang resource directory for KernelBind')
    args = cli.parse_args()
    args.via_mir = args.backend == 'mir-c11'
    alternate = args.backend in ('mir-vm', 'mir-archon')
    if alternate:
        if (args.kernel_bind or args.inspect_kernels or args.kernel_review_candidate or args.extra_c
                or args.enable_pod_fixture or args.mulib or args.mulib_fixture or args.mulib_archive
                or args.mulib_root or args.mulib_path or args.mulib_plan or args.debug_build_root):
            cli.error(f'{args.backend} does not accept C11/kernel-link options')
        if args.cflag and args.backend == 'mir-vm':
            cli.error('mir-vm emits bytecode and does not accept C compiler flags')
        compiler_tool = shutil.which(args.on1xc)
        if not compiler_tool:
            cli.error('compiler unavailable: ' + args.on1xc)
        args.on1xc = str(Path(compiler_tool).resolve(strict=True))
        protected = {args.input.resolve(), Path(args.on1xc)}
        if args.backend == 'mir-vm':
            protected.add(Path(args.on1xc).with_name('on1x-vm').resolve())
        if args.output.resolve() in protected:
            cli.error('output aliases a build input')
        args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.kernel_review_candidate:
        args.inspect_kernels = True
    review_options = (args.kernel_review, args.kernel_review_key, args.kernel_review_signature)
    if any(review_options):
        if not all(review_options) or not args.kernel_bind:
            cli.error('review, public key and signature must be supplied together with --kernel-bind')
        if args.mulib_plan:
            cli.error('reviewed builds publish their link plan inside the output directory')
    if args.optimize_ir and not (args.via_mir or alternate):
        cli.error('--optimize-ir requires --via-mir or another MIR backend')
    opt_flags = optimization.flags(args.opt_profile, args.opt_select)
    if opt_flags and (args.inspect_kernels or args.kernel_bind):
        cli.error('explicit compiler optimization policy is unsupported by KernelBind profiles')
    if args.admitted_extern and not args.memory_check:
        cli.error('--admitted-extern requires --memory-check')
    if args.debug_build_root and (args.inspect_kernels or args.kernel_bind):
        cli.error('--debug-build-root is not supported by KernelBind profiles')
    if args.enable_pod_fixture and (args.emit_only or args.via_mir or args.inspect_kernels or args.kernel_bind):
        cli.error('--enable-pod-fixture requires an ordinary direct C11 executable build')
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
        if args.backend == 'mir-vm':
            mode = '--emit-vm'
            generated = Path(temp) / 'program.o1vm'
        elif args.backend == 'mir-archon':
            mode = '--emit-ir'
            generated = Path(temp) / 'program.mir'
        compiler = [args.on1xc, mode, str(args.input), '-o', str(generated),
                    '--ppx-timeout-ms', str(args.ppx_timeout_ms), *opt_flags]
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
        if args.project_scrutiny:
            compiler += ['--project-scrutiny', args.project_scrutiny]
        if args.scrutiny:
            compiler += ['--scrutiny', args.scrutiny]
        if args.scrutiny_work is not None:
            compiler += ['--scrutiny-work', str(args.scrutiny_work)]
        if args.no_analysis_repairs:
            compiler.append('--no-analysis-repairs')
        if args.diagnostic_catalog:
            compiler += ['--diagnostic-catalog', str(args.diagnostic_catalog)]
        if args.diagnostics:
            compiler += ['--diagnostics', args.diagnostics]
        if args.color:
            compiler += ['--color', args.color]
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
        kernel_report = kernel_interface = raw_kernel_interface = None
        ipc_raw = False
        if args.inspect_kernels or args.kernel_bind:
            spec = importlib.util.spec_from_file_location('on1x_exotract', ROOT/'tools/on1x-exotract.py')
            exo = importlib.util.module_from_spec(spec);spec.loader.exec_module(exo)
            compiler[0] = exo.executable(compiler[0])
            driver_pins = {path: exo.contracts.digest(path) for path in
                           [compiler[0], Path(__file__).resolve(), Path(optimization.__file__).resolve(),
                            ROOT/'runtime/on1x_runtime.c', ROOT/'runtime/on1x_runtime.h',
                            ROOT/'runtime/on1x_bounds.h']}
            requests_path = Path(temp)/'kernel-requests.json'
            inspect = [str(compiler[0]), '--kernel-imports', str(args.input), '-o', str(requests_path), *compiler[5:]]
            subprocess.run(inspect, check=True, timeout=60)
            paths = exo.housekeeping.search_paths(args.mulib_path)
            root = exo.housekeeping.kernel_root(args.mulib_root, MULIB, paths)
            requests = exo.contracts.load(requests_path)
            ipc_raw = bool(args.kernel_bind and not args.kernel_review and any(
                r['module'] == 'mulib.ipc_framed' and r['unsafe'] for r in requests['requests']))
            if ipc_raw:
                for name in ('on1x_kernel_ipc.c', 'on1x_kernel_ipc.h'):
                    path = ROOT/'runtime'/name
                    driver_pins[path] = exo.contracts.digest(path)
            report = exo.kernel_discovery(requests, root, paths, args.exotract, args.cc, args.resource_dir,
                                          args.cflag, driver_pins,
                                          review=bool(args.kernel_review_candidate or args.kernel_review))
            candidate = exo.review_candidate(report) if args.kernel_review_candidate else None
            if args.inspect_kernels:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                with tempfile.NamedTemporaryFile(dir=args.output.parent, prefix='.kernel-discovery-', delete=False) as stream:
                    staged = Path(stream.name)
                try:
                    staged.write_text(exo.contracts.dumps(candidate or report), encoding='ascii')
                    exo.contracts.publish_new(staged, args.output)
                finally:
                    staged.unlink(missing_ok=True)
                return
            kernel_report = report
            if args.kernel_review:
                kernel_interface = exo.reviewed_interface(report)
                raw_requests = [r for r in report['requests'] if r['unsafe']]
                if raw_requests:
                    raw_kernel_interface = exo.clamp_scalar_interface(dict(report, requests=raw_requests), unsafe=True)
                review = exo.authorize_review(report, args.kernel_review_key, args.kernel_review,
                                              args.kernel_review_signature, Path(temp))
                compiler.append('--kernel-stage')
            elif ipc_raw:
                kernel_interface = exo.ipc_raw_interface(report)
            else:
                kernel_interface = exo.clamp_scalar_interface(report, unsafe=True)
            args.cc = report['compiler']
            compiler.append('--kernel-c11')
        subprocess.run(compiler, check=True)
        if args.backend == 'mir-vm':
            # The compiler writes verified portable bytecode. Verify the exact
            # published bytes with the installed standalone runner as well.
            runner = Path(args.on1xc).with_name('on1x-vm')
            subprocess.run([runner, '--verify', generated], check=True)
            with tempfile.NamedTemporaryFile(dir=args.output.parent, delete=False) as stream:
                staged = Path(stream.name)
            try:
                shutil.copyfile(generated, staged)
                os.replace(staged, args.output)
            finally:
                staged.unlink(missing_ok=True)
            return
        if args.backend == 'mir-archon':
            cc = shutil.which(args.cc)
            if not cc:
                raise ValueError('native C toolchain unavailable: ' + args.cc)
            subprocess.run([sys.executable, ROOT / 'tools/on1x-native-build.py',
                            '--on1xc', args.on1xc, '--target', 'x86_64-unknown-linux-gnu',
                            '--cc', str(Path(cc).resolve()), '--ir-input', generated, '-o', args.output,
                            *(['--emit-only'] if args.emit_only else []),
                            *(f'--cflag={flag}' for flag in args.cflag)], check=True)
            return
        if kernel_interface:
            # Re-resolve the graph after lowering as well as checking each
            # original source hash. Adding a higher-priority module candidate
            # must not leave an incomplete application input lock.
            subprocess.run(inspect, check=True, timeout=60)
            if exo.contracts.load(requests_path) != requests:
                raise ValueError('KernelBind application source graph drift')
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
        if args.kernel_review or ipc_raw:
            # Discovery checked alternate kernel candidates but did not add
            # those search roots to ExoLib's C header search path.
            includes=[mulib_root/'include']
        include_flags=[part for path in includes for part in ['-I',str(path)]]
        cmd = [args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
               '-I', str(ROOT / 'runtime'), *include_flags,
                str(generated), str(ROOT / 'runtime/on1x_runtime.c')]
        has_json = 'on1x_json_' in code
        has_csp = '#include <on1x_concurrency.h>' in code
        has_pretty = bool(re.search(r'^extern [^;\n]*\bon1x_pretty_(?:render|status)\(', code, re.M))
        has_tty = bool(re.search(r'^extern [^;\n]*\bon1x_tty_\w+\(', code, re.M))
        has_shared = '#include <on1x_container_shared.h>' in code
        container_externs = set(re.findall(r'^extern [^;\n]*\bon1x_container_(\w+)\(', code, re.M))
        container_groups = {
            'TLS': ('runtime', ('xthreads',), {'tls_i64_present', 'tls_i64_value', 'tls_i64_set', 'tls_i64_clear'}),
            'FOREIGN': ('runtime', (), {'file_byte_at'}),
            'POD': ('pod_runtime', ('sandbox-process',) if args.enable_pod_fixture else (), {'pod_echo_byte'}),
        }
        containers = [key for key, (_, _, symbols) in container_groups.items()
                      if container_externs & symbols]
        if args.enable_pod_fixture and 'POD' not in containers:
            raise ValueError('--enable-pod-fixture requires the Pod adapter in the program')
        container_flags = ['-DON1X_CONTAINER_'+group for group in containers if group != 'POD']
        if args.enable_pod_fixture:
            container_flags.append('-DON1X_CONTAINER_POD_EMBEDDED')
        if has_shared or 'TLS' in containers or args.enable_pod_fixture:
            container_flags.append('-pthread')
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
        native = (kernel_interface or has_json or has_csp or has_pretty or has_tty or has_mulib or
                  has_bridge or containers or has_shared or args.mulib or args.mulib_fixture or args.mulib_archive or args.mulib_plan)
        recipe, fingerprints, native_links = None, {}, []
        if native:
            cc=shutil.which(args.cc)
            if not cc:raise ValueError('C compiler unavailable: '+args.cc)
            cc=Path(cc).resolve()
            compiler_version=subprocess.check_output([str(cc),'--version'],text=True,timeout=30)
            compiler_target=subprocess.check_output([str(cc),'-dumpmachine'],text=True,timeout=30).strip()
            for path in (cc, Path(__file__).resolve(), Path(optimization.__file__).resolve(), Path(house.__file__)):
                fingerprints[str(path)] = house.contracts.digest(path)
            kernels = list(args.mulib)
            if kernel_interface:
                if has_json or has_csp or has_pretty or has_tty or has_mulib or has_bridge or containers or has_shared:
                    raise ValueError('KernelBind profile cannot be mixed with built-in raw kernel bridges')
                if compiler_target != kernel_report['target'] or root != mulib_root:
                    raise ValueError('KernelBind target or MuLIB root drift')
                fingerprints.update(kernel_report['inputs'])
                members = exo.interface_members(kernel_interface)
                selected = set(members)
                if raw_kernel_interface:
                    selected.add(raw_kernel_interface['module'])
                if ipc_raw:
                    kernels.append('ipc-framed')
                    expected_macros = exo.IPC_IMPL
                    cmd.append(str(ROOT/'runtime/on1x_kernel_ipc.c'))
                else:
                    kernels.extend(name.removeprefix('mulib.') for name in sorted(selected))
                    profiles = {name: exo.reviewed_profile_tools(name) for name in members}
                    expected_macros = list(dict.fromkeys(
                        macro for name in sorted(selected) for macro in exo.reviewed_profile_tools(name)[0]))
                cmd.append('-DON1X_KERNEL_ABI_V1=1')
                if args.kernel_review:
                    adapter = Path(temp)/'kernel-adapter.c'
                    adapter.write_text(''.join(
                        profiles[name][1](item)
                        for name, item in members.items()))
                    fingerprints[review['verifier']] = review['verifier_sha256']
            if has_csp:
                kernels += [name for name in ('channel', 'fiber', 'xthreads') if name not in kernels]
                cmd.append(str(ROOT/'runtime/on1x_concurrency.c'))
            for group in sorted(groups):
                for kernel in bridge_groups[group][0]:
                    if kernel not in kernels:kernels.append(kernel)
            if has_json and 'json' not in kernels:kernels.append('json')
            if has_pretty and 'prettysexpr' not in kernels:kernels.append('prettysexpr')
            if has_tty:
                for kernel in ('termutils', 'termkey', 'termstyle', 'vterm'):
                    if kernel not in kernels:kernels.append(kernel)
            for group in containers:
                for kernel in container_groups[group][1]:
                    if kernel not in kernels:
                        kernels.append(kernel)
            if has_shared:
                for kernel in ('xatomics', 'xthreads'):
                    if kernel not in kernels:
                        kernels.append(kernel)
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
                if kernel_interface and recipe['implementation_macros'] != expected_macros:
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
                    impl = Path(temp)/'mulib.c';impl.write_text(house.implementation(mulib_root,recipe))
                    if args.kernel_review:
                        # Reuse the canonical planner's single TU and compile it
                        # once; the adapter probe and application link this archive.
                        ar = exo.executable('ar')
                        fingerprints[str(ar)] = house.contracts.digest(ar)
                        obj, archive = Path(temp)/'mulib.o', Path(temp)/'libmulib.a'
                        compile_flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                                         *include_flags, *args.cflag]
                        for path, digest in house.compile_inputs(args.cc, compile_flags, impl).items():
                            if path in fingerprints and fingerprints[path] != digest:
                                raise ValueError('native input drift: '+path)
                            fingerprints[path] = digest
                        subprocess.run([args.cc, *compile_flags, '-c', str(impl), '-o', str(obj)], check=True)
                        subprocess.run([str(ar), 'rcsD', str(archive), str(obj)], check=True)
                        fingerprints[str(archive)] = house.contracts.digest(archive)
                        native_links.append(str(archive))
                    else:
                        cmd.append(str(impl))
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
            if has_tty:
                bridge = Path(subprocess.check_output(
                    [args.on1xc, '--stdlib-resource', 'tty/runtime.c', *loader_flags], text=True).strip())
                if bridge.resolve() not in {Path(p).resolve() for p in args.extra_c}:cmd.append(str(bridge))
            if containers or has_shared:
                # Check handwritten extern declarations in the same translation
                # unit as the selected ABI without pre-including system headers
                # into the native bridge ahead of its feature-test macros.
                abi_includes = []
                supplied = {Path(p).resolve() for p in args.extra_c}
                for stem in sorted({container_groups[group][0] for group in containers}):
                    bridge = Path(subprocess.check_output(
                        [args.on1xc, '--stdlib-resource', f'container/{stem}.c', *loader_flags], text=True).strip()).resolve(strict=True)
                    header = bridge.with_suffix('.h').resolve(strict=True)
                    header_bytes = house.contracts.read(header, 256 * 1024)
                    fingerprints[str(header)] = hashlib.sha256(header_bytes).hexdigest()
                    abi_name = f'container-{stem}-abi.h'
                    (Path(temp)/abi_name).write_bytes(header_bytes)
                    abi_includes.append(f'#include "{abi_name}"\n')
                    if bridge not in supplied:
                        cmd.append(str(bridge))
                    if stem == 'pod_runtime' and args.enable_pod_fixture:
                        if not re.fullmatch(r'(x86_64|aarch64)-[^ ]*linux[^ ]*', compiler_target):
                            raise ValueError('the Pod fixture requires a Linux x86-64 or AArch64 C target')
                        worker = Path(subprocess.check_output(
                            [args.on1xc, '--stdlib-resource', 'container/pod_worker.c', *loader_flags],
                            text=True).strip()).resolve(strict=True)
                        if worker.parent != bridge.parent:
                            raise ValueError('Pod worker and runtime must come from the same stdlib')
                        worker_flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                                        '-I', str(mulib_root/'include'), '-static']
                        for path, digest in house.compile_inputs(args.cc, worker_flags, worker).items():
                            if path in fingerprints and fingerprints[path] != digest:
                                raise ValueError('Pod worker input drift: '+path)
                            fingerprints[path] = digest
                        elf = Path(temp)/'pod-worker.elf'
                        subprocess.run([args.cc, *worker_flags, '-s', str(worker), '-o', str(elf)],
                                       check=True, timeout=60)
                        data = house.contracts.read(elf, 2 * 1024 * 1024)
                        if not data.startswith(b'\x7fELF\x02\x01'):
                            raise ValueError('Pod worker must be a 64-bit little-endian ELF')
                        fingerprints[str(elf)] = hashlib.sha256(data).hexdigest()
                        image = Path(temp)/'pod-worker-image.c'
                        with image.open('w', encoding='ascii') as stream:
                            stream.write('#include <stddef.h>\nconst unsigned char on1x_container_pod_worker_elf[] = {\n')
                            for i in range(0, len(data), 32):
                                stream.write(','.join(str(b) for b in data[i:i+32])+',\n')
                            stream.write('};\nconst size_t on1x_container_pod_worker_elf_size = '
                                         'sizeof on1x_container_pod_worker_elf;\n')
                        cmd.append(str(image))
                if has_shared:
                    bridge = Path(subprocess.check_output(
                        [args.on1xc, '--stdlib-resource', 'container/shared.c', *loader_flags], text=True).strip())
                    shared_header = bridge.with_name('on1x_container_shared.h').resolve(strict=True)
                    fingerprints[str(shared_header)] = house.contracts.digest(shared_header)
                    container_flags += ['-I', str(bridge.parent)]
                    if bridge not in supplied:
                        cmd.append(str(bridge))
                if abi_includes:
                    generated.write_text(''.join(abi_includes)+code)
                cmd += container_flags
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
        if args.debug_build_root:
            # Keep physical, private temp paths for file I/O and diagnostics;
            # filenames such as main.c and mulib.c are already deterministic.
            cmd += project_debug_flags(args.debug_build_root, temp, mulib_root, library)
        cmd += args.cflag + native_links
        if native:
            flags=['-I',str(ROOT/'runtime'),*include_flags,'-I',str(ROOT/'stdlib/mulib'),*args.cflag]
            if recipe:flags += ['-D'+item for item in recipe['compile_definitions']]
            flags += ['-DON1X_BRIDGE_'+group for group in sorted(groups)]
            flags += container_flags
            if kernel_interface:flags.append('-DON1X_KERNEL_ABI_V1=1')
            for source in [Path(value) for value in cmd if value.endswith('.c')]:
                for path,digest in house.compile_inputs(args.cc,flags,source).items():
                    if path in fingerprints and fingerprints[path]!=digest:raise ValueError('native input drift: '+path)
                    fingerprints[path]=digest
        binary=Path(temp)/'program'
        if args.kernel_review:
            adapter_obj = Path(temp)/'kernel-adapter.o'
            adapter_flags = [*compile_flags, *['-D'+item for item in recipe['compile_definitions']]]
            for path, digest in house.compile_inputs(args.cc, adapter_flags, adapter).items():
                if path in fingerprints and fingerprints[path] != digest:
                    raise ValueError('native input drift: '+path)
                fingerprints[path] = digest
            subprocess.run([args.cc, *adapter_flags, '-c', str(adapter), '-o', str(adapter_obj)], check=True)
            fingerprints[str(adapter_obj)] = house.contracts.digest(adapter_obj)
            cmd.insert(cmd.index(str(archive)), str(adapter_obj))
            probe = Path(temp)/'kernel-probe.c'
            if len(profiles) > 1:
                names = []
                source = []
                for i, (_, _, generate, _) in enumerate(profiles.values()):
                    name = f'kernel_probe_{i}'
                    names.append(name+'()')
                    source.extend((f'#define main {name}\n', generate(), '#undef main\n'))
                probe.write_text(''.join(source)+'int main(void) { return '+' || '.join(names)+'; }\n')
            else:
                probe.write_text(next(iter(profiles.values()))[2]())
            probe_binary = Path(temp)/'kernel-probe'
            subprocess.run([args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                            *include_flags, *args.cflag, str(probe), str(adapter_obj), *native_links,
                            '-o', str(probe_binary)], check=True)
            subprocess.run([str(probe_binary)], check=True, timeout=30)
        cmd += ['-o', str(binary)]
        subprocess.run(cmd, check=True)
        if native:
            for path, digest in fingerprints.items():
                if house.contracts.digest(path)!=digest:raise ValueError('native input drift: '+path)
            if args.kernel_review:
                kernel_interface['safe_export'] = True
                for member in members.values():
                    member['safe_export'] = True
                    for export in member['exports'].values():
                        export['status'] = 'admitted'
                # Everything is published as one non-overwriting directory.
                # No admission can be left beside a failed or missing executable.
                stage = Path(tempfile.mkdtemp(dir=args.output.absolute().parent, prefix='.kernel-admission-'))
                try:
                    files = [generated, adapter, adapter_obj, impl, archive, probe, Path(temp)/'review.json',
                             Path(temp)/'review.sig', Path(temp)/'review.ed25519.der', binary]
                    for path in files:
                        shutil.copy2(path, stage/path.name)
                    def save(name, value):
                        (stage/name).write_text(house.contracts.dumps(value), encoding='ascii')
                    save('interface.json', kernel_interface)
                    if raw_kernel_interface:
                        save('unsafe-interface.json', raw_kernel_interface)
                    save('discovery.json', kernel_report)
                    save('build-plan.json', recipe)
                    stable = {p: h for p, h in fingerprints.items() if not Path(p).is_relative_to(Path(temp))}
                    save('admission.json', dict(schema='on1x.kernel-admission/v1',
                         profile=kernel_interface['profile'], safe_export=True, review=review,
                         contains_unsafe_imports=bool(raw_kernel_interface),
                         executable_gate=(dict(profile='kernel-set-differential/v1', passed=True,
                                               profiles=[tools[3] for tools in profiles.values()])
                             if len(profiles)>1 else dict(profile=next(iter(profiles.values()))[3], passed=True,
                                  **({'comparisons':exo.CLAMP_PROBE_COMPARISONS} if 'mulib.clamp' in profiles else {}))),
                         inputs=stable, cflags=args.cflag, compiler_target=compiler_target,
                         artifacts={p.name: house.contracts.digest(p) for p in sorted(stage.iterdir())}))
                    house.contracts.publish_new(stage, args.output.absolute())
                finally:
                    if stage.exists():
                        shutil.rmtree(stage)
                return
            if args.mulib_plan:
                # Generated temporary inputs are captured by content, not stale temp paths.
                stable={path:digest for path,digest in fingerprints.items() if not Path(path).is_relative_to(Path(temp))}
                generated_inputs={Path(path).name:digest for path,digest in fingerprints.items() if Path(path).is_relative_to(Path(temp))}
                report=dict(schema='on1x.mulib-link/v1',plan=recipe,inputs=stable,generated_inputs=generated_inputs,
                            compiler=str(cc),compiler_version=compiler_version,compiler_target=compiler_target,
                            cflags=args.cflag,bridge_groups=sorted(groups),
                             executable_sha256=house.contracts.digest(binary),safe_export=False)
                if opt_flags or args.optimize_ir:
                    report['optimization_args'] = opt_flags + (['--optimize-ir'] if args.optimize_ir else [])
                if containers or has_shared:
                    report['container_bridge_groups'] = sorted(containers + (['SHARED'] if has_shared else []))
                if 'POD' in containers:
                    report['pod_fixture_enabled'] = args.enable_pod_fixture
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
        sys.exit(main())
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
    except (ValueError, OSError, KeyError, TypeError, RecursionError, subprocess.TimeoutExpired) as e:
        print('on1x-build: '+str(e),file=sys.stderr)
        sys.exit(1)
