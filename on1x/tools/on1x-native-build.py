#!/usr/bin/env python3
"""Build verified On1x MIR through real Archon assembly and a trusted ELF toolchain.

The explicit --cc executable performs assembly/linking outside the compiler.
No generated program is run. The qualified executable profile is Linux AMD64
SysV; other targets fail closed until their runtime/toolchain is qualified.
"""
import argparse
import json
import math
import os
from pathlib import Path
import resource
import shutil
import signal
import subprocess
import sys
import tempfile
import time


class BuildError(Exception):
    pass


TARGET = "x86_64-unknown-linux-gnu"
MAX_BYTES = 16 * 1024 * 1024
SAFE_FLAGS = {
    "-O0", "-O1", "-O2", "-O3", "-g", "-fno-omit-frame-pointer",
    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
}


def file_path(value, executable=False):
    path = Path(value)
    if not path.is_absolute():
        raise BuildError("trusted tool/artifact path must be absolute: " + str(path))
    path = path.resolve(strict=True)
    if not path.is_file() or (executable and not os.access(path, os.X_OK)):
        raise BuildError("not a readable file/executable: " + str(path))
    return path


def frontend_flags(flags):
    """Keep user ordering, while preventing a second mode/output/backend selection."""
    arities = {
        "--no-prelude": 0, "--memory-check": 0, "--verify-types": 0,
        "--no-analysis-repairs": 0, "--allow-source-ppx": 0,
        "--opt-profile": 1, "--profile": 1, "--enable-opt": 1, "--disable-opt": 1,
        "--all-optimizations": 0, "--no-optimizations": 0,
        "-I": 1, "--module-path": 1, "--stdlib": 1, "--prelude": 1,
        "--lib-path": 1, "--stdlib-path": 1, "--ppx-path": 1,
        "--stdppx-path": 1, "--stdppx": 1, "--use-ppx": 1,
        "--enable": 1, "--disable": 1, "--ppx-timeout-ms": 1,
        "--scrutiny": 1, "--project-scrutiny": 1, "--scrutiny-work": 1, "--diagnostics": 1,
        "--color": 1, "--diagnostic-catalog": 1, "--opt-work": 1, "--opt-rewrites": 1,
        "--ppx": 2, "--ppx-schema": 2,
    }
    index = 0
    while index < len(flags):
        flag = flags[index]
        if flag not in arities or index + arities[flag] >= len(flags):
            raise BuildError("unsupported/incomplete forwarded frontend flag: " + flag)
        index += arities[flag] + 1
    return flags


class Runner:
    def __init__(self, directory, timeout, memory, max_bytes, verbose):
        self.directory = directory
        self.deadline = time.monotonic() + timeout
        self.timeout = timeout
        self.memory = memory
        self.max_bytes = max_bytes
        self.verbose = verbose
        self.serial = 0

    def limits(self):
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_AS, (self.memory, self.memory))
        resource.setrlimit(resource.RLIMIT_CPU,
                           (math.ceil(self.timeout), math.ceil(self.timeout) + 1))
        resource.setrlimit(resource.RLIMIT_FSIZE, (self.max_bytes, self.max_bytes))
        resource.setrlimit(resource.RLIMIT_NOFILE, (128, 128))

    def run(self, command, toolchain=False, cwd=None):
        remaining = self.deadline - time.monotonic()
        if remaining <= 0:
            raise BuildError("native build deadline exceeded")
        command = [str(item) for item in command]
        if self.verbose:
            print(json.dumps(command), file=sys.stderr)
        self.serial += 1
        log = self.directory / ("stage-%d.log" % self.serial)
        environment = None
        if toolchain:
            environment = os.environ.copy()
            # Driver resolution must not inherit ambient compiler/tool overrides.
            for name in ("GCC_EXEC_PREFIX", "COMPILER_PATH", "LIBRARY_PATH",
                         "COLLECT_GCC", "COLLECT_LTO_WRAPPER"):
                environment.pop(name, None)
            environment["PATH"] = os.defpath
            environment["LC_ALL"] = "C"
        with log.open("wb") as output:
            child = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=output,
                                     stderr=subprocess.STDOUT, start_new_session=True,
                                     preexec_fn=self.limits, env=environment, cwd=cwd)
            try:
                code = child.wait(timeout=remaining)
            except BaseException:
                # Kill the whole process group, including assembler/linker/PPX.
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                child.wait()
                raise
        with log.open("rb") as output:
            text = output.read(65536).decode("utf-8", errors="replace")
        if code:
            raise BuildError("stage failed (%d): %s\n%s" % (code, command[0], text))
        return text.strip()


def manifest_path(build, toolchain_prefix):
    if toolchain_prefix:
        return toolchain_prefix.resolve() / "share/archon/backend/BackendManifest.json"
    installed = build.parent / "share/on1x/backends/mir-archon/BackendManifest.json"
    if build.name == 'bin' and not (build / 'CMakeCache.txt').is_file():
        return installed
    staged = build / "backends/mir-archon/BackendManifest.json"
    if staged.is_file():
        return staged
    cache = build / "CMakeCache.txt"
    if cache.stat().st_size > 4 * 1024 * 1024:
        raise BuildError("CMake cache exceeds limit")
    for line in cache.read_text().splitlines():
        if line.startswith(("ArchonBackend_DIR:PATH=", "ArchonBackend_DIR:UNINITIALIZED=")):
            package = Path(line.partition("=")[2]).resolve()
            return package.parents[2] / "share/archon/backend/BackendManifest.json"
    raise BuildError("missing ArchonBackend_DIR; provide --toolchain-prefix")


def layout(build):
    """Fixed build-tree or installed-prefix artifacts beside the selected compiler."""
    if build.name == 'bin' and not (build / 'CMakeCache.txt').is_file():
        return (build / 'on1x-archon-mir', build.parent / 'lib/libon1x_archon_scalar_abi.a',
                build.parent / 'lib/libon1x_runtime.a')
    return (build / 'backends/mir-archon/on1x-archon-mir',
            build / 'backends/mir-archon/libon1x_archon_scalar_abi.a', build / 'libon1x_runtime.a')


def bounded_artifact(path, limit):
    if not path.is_file() or not 0 < path.stat().st_size <= limit:
        raise BuildError("missing, empty or oversized artifact: " + str(path))


def main(argv=None):
    import on1x_optimization as optimization
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", nargs='?', type=Path)
    parser.add_argument("-o", "--output", type=Path)
    parser.add_argument("--build-dir", type=Path, help='tool directory (defaults to the selected compiler directory)')
    parser.add_argument("--probe", action='store_true', help='verify the configured native toolchain and print its tools')
    parser.add_argument("--target", required=True)
    parser.add_argument("--cc", required=True, help="absolute path to trusted GCC/Clang driver")
    parser.add_argument("--assembler", type=Path, help="absolute trusted assembler (default: CC-selected)")
    parser.add_argument("--linker", type=Path, help="absolute trusted linker (default: CC-selected)")
    parser.add_argument("--toolchain-prefix", type=Path)
    parser.add_argument("--archon-manifest", type=Path)
    parser.add_argument("--on1xc", type=Path)
    parser.add_argument("--archon-mir", type=Path)
    parser.add_argument("--scalar-abi", type=Path)
    parser.add_argument("--runtime", type=Path)
    parser.add_argument("--ir-input", action="store_true", help="input is serialized MIR")
    parser.add_argument("--optimize-ir", action="store_true",
                        help="enable MIR passes and opt into speculative Archon directives")
    parser.add_argument("--speculate", action="store_true",
                        help="opt into speculative Archon directives without enabling MIR passes")
    optimization.arguments(parser)
    parser.add_argument("--pic", action="store_true", help="emit PIC and link a PIE")
    parser.add_argument("--emit-only", action="store_true", help="publish assembly")
    parser.add_argument("--no-entry", action="store_true", help="library assembly; requires --emit-only")
    parser.add_argument("--max-assembly-bytes", type=int, default=MAX_BYTES)
    parser.add_argument("--max-output-bytes", type=int, default=64 * 1024 * 1024)
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("--memory-mib", type=int, default=2048)
    parser.add_argument("--cflag", action="append", default=[])
    parser.add_argument("--on1xc-arg", action="append", default=[],
                        help="ordered frontend argument; use --on1xc-arg=--no-prelude")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)
    if (args.probe and (args.output or args.input)) or (not args.probe and not (args.output and args.input)):
        parser.error("supply INPUT and -o OUTPUT, or --probe without either")
    if sys.platform != "linux" or args.target != TARGET:
        raise BuildError("native executable helper currently qualifies only Linux " + TARGET)
    if args.no_entry and not args.emit_only:
        raise BuildError("--no-entry requires --emit-only")
    if (not math.isfinite(args.timeout) or not 0 < args.timeout <= 3600 or
            not 64 <= args.memory_mib <= 65536 or
            not 1 <= args.max_assembly_bytes <= MAX_BYTES or
            not 1 <= args.max_output_bytes <= 1024 * 1024 * 1024):
        raise BuildError("invalid timeout, memory or artifact limit")
    if any(flag not in SAFE_FLAGS for flag in args.cflag):
        raise BuildError("unsupported C flag (target/ABI/fast-math overrides are forbidden)")
    forwarded = frontend_flags(args.on1xc_arg)
    default_compiler = (args.build_dir.resolve(strict=True) / 'on1xc' if args.build_dir else
                        Path(__file__).resolve().parents[3] / 'bin/on1xc')
    compiler = file_path(args.on1xc or default_compiler, True)
    build = args.build_dir.resolve(strict=True) if args.build_dir else compiler.parent
    archon_default, scalar_default, runtime_default = layout(build)
    archon = file_path(args.archon_mir or archon_default, True)
    cc = file_path(args.cc, True)
    scalar = file_path(args.scalar_abi or scalar_default)
    runtime = file_path(args.runtime or runtime_default)
    source = args.input.resolve(strict=True) if args.input else None
    # Preserve the final path component so atomic replacement replaces a symlink,
    # rather than following it and overwriting the symlink's destination.
    destination = args.output.parent.resolve(strict=True) / args.output.name if args.output else None
    manifest = file_path(args.archon_manifest or manifest_path(build, args.toolchain_prefix))
    if destination and destination.resolve() in {source, compiler, archon, cc, scalar, runtime, manifest}:
        raise BuildError("output aliases a build input")
    if manifest.stat().st_size > 1024 * 1024:
        raise BuildError("Archon manifest exceeds limit")
    profile = json.loads(manifest.read_text())
    if (profile.get("target"), profile.get("abi"), profile.get("backend_abi_version"),
            profile.get("machineir_schema_version")) != ("AMD64", "SysVAMD64", 1, 1):
        raise BuildError("Archon manifest target/ABI/schema mismatch")
    # Stage on the output filesystem; every failure leaves the old output intact.
    with tempfile.TemporaryDirectory(prefix=".on1x-native-", dir=destination.parent if destination else None) as temporary:
        stage = Path(temporary)
        runner = Runner(stage, args.timeout, args.memory_mib * 1024 * 1024,
                        args.max_output_bytes, args.verbose)
        if runner.run([archon, "--print-target"]) != args.target:
            raise BuildError("configured Archon target mismatch")
        if runner.run([archon, "--print-backend-target"]) != "AMD64/SysVAMD64":
            raise BuildError("generated Archon backend mismatch")
        cc_target = runner.run([cc, "-dumpmachine"], toolchain=True)
        if cc_target not in {TARGET, "x86_64-linux-gnu", "x86_64-pc-linux-gnu"}:
            raise BuildError("C assembler/linker target mismatch: " + cc_target)
        # Resolve the selected driver's actual tools once and pin their names
        # through a private -B prefix. Never execute an unqualified PATH command.
        tools = stage / "tools"
        tools.mkdir()
        resolved_tools = {}
        for name, override in (("as", args.assembler), ("ld", args.linker)):
            selected = str(override) if override else runner.run(
                [cc, "-print-prog-name=" + name], toolchain=True)
            if not Path(selected).is_absolute():
                if override:
                    raise BuildError("explicit assembler/linker must be an absolute path")
                selected = shutil.which(selected, path=os.defpath)
                if not selected:
                    raise BuildError("cannot resolve trusted tool: " + name)
            selected = file_path(selected, True)
            if destination and destination.resolve() == selected:
                raise BuildError("output aliases a toolchain executable")
            (tools / name).symlink_to(selected)
            resolved_tools[name] = selected
            if name == "ld":
                (tools / "ld.bfd").symlink_to(selected)
            if args.verbose:
                print("trusted " + name + ": " + str(selected), file=sys.stderr)
        if args.probe:
            print(json.dumps(dict(target=args.target, abi=profile['abi'], cc_target=cc_target,
                                  assembler=str(resolved_tools['as']), linker=str(resolved_tools['ld'])), sort_keys=True))
            return 0
        mir = stage / "program.mir"
        command = [compiler, "--emit-ir"]
        if args.ir_input:
            command.append("--ir-input")
        command += [source, "-o", mir]
        command += optimization.flags(args.opt_profile, args.opt_select)
        if args.optimize_ir:
            command.append("--optimize-ir")
        runner.run(command + forwarded)
        bounded_artifact(mir, MAX_BYTES)
        assembly = stage / "program.s"
        command = [archon, "--ir-input", mir, "--target", args.target, "-o", assembly,
                   "--max-assembly-bytes", args.max_assembly_bytes]
        if args.optimize_ir or args.speculate:
            command.append("--speculate")
        if args.pic:
            command.append("--pic")
        if args.no_entry:
            command.append("--no-entry")
        runner.run(command)
        bounded_artifact(assembly, args.max_assembly_bytes)
        artifact = assembly
        if not args.emit_only:
            obj = stage / "program.o"
            # The assembler runs in the private stage; don't embed temporary
            # project paths in native debug info.
            runner.run([resolved_tools["as"], "--64", assembly.name, "-o", obj] +
                       (["-g", "--debug-prefix-map", str(stage) + "=/__on1x__/native"]
                        if "-g" in args.cflag else []), toolchain=True, cwd=stage)
            bounded_artifact(obj, args.max_output_bytes)
            with obj.open("rb") as stream:
                header = stream.read(20)
            if (header[:7] != b"\x7fELF\x02\x01\x01" or header[18:20] != b"\x3e\x00" or
                    header[16:18] != b"\x01\x00"):
                raise BuildError("assembler did not produce an AMD64 ELF relocatable object")
            artifact = stage / "program"
            runner.run([cc, "-B" + str(tools) + "/", "-fuse-ld=bfd",
                        "-pie" if args.pic else "-no-pie",
                        obj, scalar, runtime, "-lm", "-Wl,-z,noexecstack", "-o", artifact] +
                       args.cflag, toolchain=True)
            bounded_artifact(artifact, args.max_output_bytes)
            with artifact.open("rb") as stream:
                header = stream.read(20)
            if (header[:7] != b"\x7fELF\x02\x01\x01" or header[18:20] != b"\x3e\x00" or
                    header[16:18] not in (b"\x02\x00", b"\x03\x00")):
                raise BuildError("linker did not produce a Linux AMD64 ELF executable")
        with artifact.open("rb") as stream:
            os.fsync(stream.fileno())
        os.replace(artifact, destination)
    return 0


def interrupted(signum, frame):
    raise KeyboardInterrupt("native build cancelled")


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, interrupted)
    try:
        sys.exit(main())
    except (BuildError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        print("on1x-native-build: " + str(error), file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("on1x-native-build: cancelled", file=sys.stderr)
        sys.exit(130)
