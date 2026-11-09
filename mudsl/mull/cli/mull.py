#!/usr/bin/env python3
"""MuLL command driver. Compiler workers and tools are invoked without a shell."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

INSTALL_LIBEXECDIR = "@CMAKE_INSTALL_LIBEXECDIR@"
VERSION = "1.2.0"
STAGES = {"parse": "ast", "mullier": "mullier", "mu3ac": "mu3ac",
          "assembly": "asm", "object": "obj", "link": "exe"}


def positive(text):
    try:
        value = int(text, 10)
    except ValueError:
        raise argparse.ArgumentTypeError("expected a positive integer")
    if not 0 < value < 2**63:
        raise argparse.ArgumentTypeError("expected an integer in 1..2^63-1")
    return value


def foreign(text):
    match = re.fullmatch(r"([A-Za-z_][A-Za-z_0-9]*)=([A-Za-z_][A-Za-z_0-9]*):([0-8])", text)
    if not match or match[1] in ("G", "Args"):
        raise argparse.ArgumentTypeError("expected NAME=SYMBOL:ARITY (0..8); G and Args are reserved")
    return match[1], match[2], match[3]


def parser():
    p = argparse.ArgumentParser(prog="mull", description="Run MuLL or compile it through MuLLIER and Mu3AC to x86-64.",
        epilog="Examples: mull -p -e 'return 6*7;' | mull --emit=mu3ac app.mull | "
               "mull --engine=d --emit=exe -o app app.mull")
    p.add_argument("--version", action="version", version="MuLL " + VERSION)
    p.add_argument("--list-engines", action="store_true", help="show installed host implementations")
    p.add_argument("--engine", "--host", choices=("cpp", "d"), default="cpp", help="MuDSL.hpp (cpp, default) or MuDSL.d (d)")
    p.add_argument("--backend", choices=("interpreter", "x86-64"), help="execution backend (interpreter by default)")
    p.add_argument("--target", default="x86_64-linux-gnu", help="native target (currently x86_64-linux-gnu)")
    mode = p.add_mutually_exclusive_group()
    mode.add_argument("--emit", choices=("ast", "mullier", "mu3ac", "asm", "obj", "exe"), help="stop and write this compilation artifact")
    mode.add_argument("--stop-after", choices=tuple(STAGES), help="named compilation-stage alias for --emit")
    mode.add_argument("--check", action="store_true", help="parse and analyze without executing or emitting")
    mode.add_argument("-S", action="store_const", const="asm", dest="emit", help="emit assembly")
    mode.add_argument("-c", action="store_const", const="obj", dest="emit", help="emit an ELF object")
    p.add_argument("-e", "--eval", dest="expression", help="use this source instead of a file")
    p.add_argument("--repl", action="store_true", help="persistent interpreter; one complete program per line")
    p.add_argument("-p", "--print-result", action="store_true", help="print the final MuLL value")
    p.add_argument("-o", "--output", help="output file; '-' means stdout for text artifacts")
    p.add_argument("--run", action="store_true", help="execute an emitted executable after linking")
    optimization = p.add_mutually_exclusive_group()
    optimization.add_argument("-O0", action="store_const", const=False, dest="optimize", help="disable MuLLIER and Mu3AC transforms, not analysis")
    optimization.add_argument("-O1", action="store_const", const=True, dest="optimize", help="enable bounded transforms (default)")
    p.set_defaults(optimize=True)
    libraries = p.add_argument_group("packages and native linking")
    libraries.add_argument("-I", "--package-path", action="append", default=[], metavar="DIR", help="prepend a MuLL package discovery root (repeatable)")
    libraries.add_argument("--no-default-paths", action="store_true", help="omit MULL_PATH and the installed standard-library root")
    libraries.add_argument("--no-stdlib", action="store_true", help="do not register the native standard-library packages")
    libraries.add_argument("--load", action="append", default=[], metavar="PACKAGE", help="preload a package at execution time, not at compile-only stages")
    libraries.add_argument("-L", "--library-path", action="append", default=[], metavar="DIR", help="native linker search directory")
    libraries.add_argument("-l", "--library", action="append", default=[], metavar="NAME", help="link a native library")
    libraries.add_argument("--link-object", action="append", default=[], metavar="FILE", help="link an additional object or archive")
    libraries.add_argument("--extern", action="append", type=foreign, default=[], metavar="NAME=SYMBOL:N", help="bind a C-ABI double function (0..8 double arguments), native backend only")
    libraries.add_argument("--rpath", action="append", default=[], metavar="DIR", help="add a native runtime library search path")
    libraries.add_argument("--link-arg", action="append", default=[], metavar="ARG", help="pass one argument to the host compiler at link time")
    compiler = p.add_argument_group("compiler configuration")
    compiler.add_argument("--mullier-rules", metavar="DIR", help="transactionally load MuLLIER .a/.t rules")
    compiler.add_argument("--mu3ac-rules", metavar="FILE", help="replace the trusted local Mu3AC rewrite rules")
    compiler.add_argument("--cc", help="assembler driver executable (default CC or cc)")
    compiler.add_argument("--cxx", help="C++ linker driver executable (default CXX or configured compiler)")
    compiler.add_argument("--dc", help="D linker driver executable (default DC or configured compiler)")
    compiler.add_argument("-v", "--verbose", action="store_true", help="print argument vectors for invoked tools")
    for name, default in (("steps", 1000000), ("call-depth", 128), ("frames", 100000),
                          ("source-bytes", 4194304), ("values", 2000000)):
        compiler.add_argument("--max-" + name, type=positive, default=default, metavar="N",
                              help=f"execution/compiler limit (default {default}); values is native-only")
    p.add_argument("file", nargs="?", help="MuLL source, or '-' for stdin; script arguments follow --")
    return p


def configuration():
    here = Path(__file__).resolve()
    candidates = [Path(os.environ["MULL_CONFIG"])] if "MULL_CONFIG" in os.environ else [
        here.parent / "mull-config.json", here.parent.parent / INSTALL_LIBEXECDIR / "mull/mull-config.json"]
    for path in candidates:
        if path.is_file():
            config = json.loads(path.read_text())
            if config["kind"] == "install":
                prefix = here.parent.parent
                for key in ("cpp", "d", "stdlib", "cpp_runtime", "d_runtime", "bridge"):
                    if config.get(key):
                        config[key] = str(prefix / config[key])
            return config
    raise FileNotFoundError("MuLL installation/build configuration not found; build mudsl/mull with CMake")


def invoke(command, verbose=False, capture=False):
    if verbose:
        print("mull:", repr([str(part) for part in command]), file=sys.stderr)
    result = subprocess.run(command, stdout=subprocess.PIPE if capture else None)
    if result.returncode:
        raise subprocess.CalledProcessError(result.returncode, command)
    return result.stdout


def linker_flags(engine, compiler, flags):
    # LDC/DMD wrap linker options in -L; GDC uses the GCC driver convention.
    if engine == "d" and "gdc" not in Path(compiler).name:
        return ["-L" + flag for flag in flags]
    return flags


def read_bounded(stream, limit):
    chunks = []
    remaining = limit + 1
    while remaining:
        chunk = stream.read(min(65536, remaining))
        if not chunk:
            break
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def publish(source, destination):
    """Only replace a requested artifact after all compilation has succeeded."""
    destination = Path(destination)
    fd, name = tempfile.mkstemp(prefix=".mull-", dir=destination.parent)
    os.close(fd)
    try:
        shutil.copy2(source, name)
        os.replace(name, destination)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    script_args = []
    if "--" in argv:
        boundary = argv.index("--")
        script_args, argv = argv[boundary + 1:], argv[:boundary]
    p = parser()
    a = p.parse_args(argv)
    stage = "check" if a.check else STAGES[a.stop_after] if a.stop_after else a.emit
    backend = a.backend or ("x86-64" if stage in ("asm", "obj", "exe") else "interpreter")
    if a.target != "x86_64-linux-gnu":
        p.error("only the x86_64-linux-gnu native target is currently supported")
    if a.expression is not None and a.file:
        p.error("choose either --eval or a source file")
    if a.run and stage not in (None, "exe"):
        p.error("--run requires executable emission")
    native = backend == "x86-64" or stage in ("asm", "obj", "exe")
    if a.backend == "interpreter" and stage in ("asm", "obj", "exe"):
        p.error("assembly/object/executable emission requires --backend=x86-64")
    if not native and (a.library or a.library_path or a.link_object or a.extern or a.rpath or a.link_arg):
        p.error("native linking/foreign options require --backend=x86-64")
    if a.output and stage is None and not native:
        p.error("--output requires a compilation artifact")
    if a.output and stage == "check":
        p.error("--check produces no output artifact")
    if a.output == "-" and (stage in ("obj", "exe") or (native and stage is None)):
        p.error("binary artifacts cannot be written to stdout")
    if a.repl and (a.file or a.expression is not None or stage or native or script_args):
        p.error("--repl cannot be combined with source, compilation, or script arguments")
    if len({f[0] for f in a.extern}) != len(a.extern):
        p.error("duplicate --extern binding")
    for name in a.load:
        if not re.fullmatch(r"[A-Za-z_0-9-]+", name):
            p.error("invalid package name: " + name)
    config = configuration()
    if a.list_engines:
        for engine in ("cpp", "d"):
            path = config.get(engine)
            runtime = config.get(engine + "_runtime")
            status = "available" if path and Path(path).is_file() else "not built"
            native_status = "available" if runtime and Path(runtime).is_file() else "not built"
            print(f"{engine}: {status}; x86-64 runtime: {native_status}")
        return 0
    worker = config.get(a.engine)
    if not worker or not Path(worker).is_file():
        raise FileNotFoundError(f"MuLL {a.engine} worker was not built; enable MULL_BUILD_D or use --engine=cpp")
    command = [worker]
    paths = [str(Path(path).resolve()) for path in a.package_path]
    if not a.no_default_paths:
        paths += [str(Path(path).resolve()) for path in os.environ.get("MULL_PATH", "").split(os.pathsep) if path]
        paths.append(config["stdlib"])
    for path in paths:
        command += ["--path", path]
    for name in a.load:
        command += ["--preload", name]
    for name, symbol, count in a.extern:
        command += ["--foreign", name, symbol, count]
    for key in ("steps", "call_depth", "frames", "source_bytes", "values"):
        command += ["--max-" + key.replace("_", "-"), str(getattr(a, "max_" + key))]
    for flag, value in (("--rules", a.mullier_rules), ("--mu3ac-rules", a.mu3ac_rules)):
        if value:
            command += [flag, str(Path(value).resolve())]
    if not a.optimize:
        command += ["--no-optimize"]
    if a.no_stdlib:
        command += ["--no-stdlib"]
    if a.print_result:
        command += ["--print-result"]
    if a.repl or (not a.file and a.expression is None and stage is None and not native and sys.stdin.isatty()):
        invoke(command + ["--repl", "--stage", "run"], a.verbose)
        return 0
    with tempfile.TemporaryDirectory(prefix="mull-") as temporary:
        tmp = Path(temporary)
        name = "<eval>" if a.expression is not None else a.file or "<stdin>"
        if a.expression is not None:
            source = a.expression.encode()
        elif a.file and a.file != "-":
            try:
                with open(a.file, "rb") as stream:
                    source = read_bounded(stream, a.max_source_bytes)
            except FileNotFoundError as error:
                raise ValueError(str(error)) from None
        else:
            source = read_bounded(sys.stdin.buffer, a.max_source_bytes)
            name = "<stdin>"
        if len(source) > a.max_source_bytes:
            raise ValueError("source size limit exceeded")
        if a.output and a.output != "-" and a.file and a.file != "-" and Path(a.output).resolve() == Path(a.file).resolve():
            p.error("output would overwrite the input source")
        input_path = tmp / "input.mull"
        input_path.write_bytes(source)
        base = command + ["--file", str(input_path), "--name", name]
        if stage in ("ast", "mullier", "mu3ac", "check") or not native:
            data = invoke(base + ["--stage", stage or "run", "--", *script_args], a.verbose,
                          capture=stage not in (None, "check"))
            if data is not None:
                if a.output and a.output != "-":
                    output = tmp / "artifact"
                    output.write_bytes(data)
                    publish(output, a.output)
                else:
                    sys.stdout.buffer.write(data)
            return 0
        assembly = invoke(base + ["--stage", "asm"], a.verbose, capture=True)
        asm = tmp / "program.s"
        asm.write_bytes(assembly)
        if stage == "asm":
            if a.output and a.output != "-":
                publish(asm, a.output)
            else:
                sys.stdout.buffer.write(assembly)
            return 0
        obj = tmp / "program.o"
        invoke([a.cc or os.environ.get("CC", "cc"), "-c", str(asm), "-o", str(obj)], a.verbose)
        if stage == "obj":
            publish(obj, a.output or "a.o")
            return 0
        compiler = (a.cxx or os.environ.get("CXX", config["cxx"])) if a.engine == "cpp" else (a.dc or os.environ.get("DC", config["dc"]))
        runtime = config.get(a.engine + "_runtime")
        if not runtime or not Path(runtime).is_file():
            raise FileNotFoundError(f"Mu3AC {a.engine} native runtime was not built for this system")
        exe = tmp / "program"
        link = [compiler, str(obj), *a.link_object, runtime]
        if config.get("bridge"):
            link += [config["bridge"]]
        flags = ["-L" + path for path in a.library_path] + ["-l" + name for name in a.library]
        flags += ["-lm", "-ldl", "-lpthread"]
        link += linker_flags(a.engine, compiler, flags)
        for path in a.rpath:
            if a.engine == "d" and "gdc" not in Path(compiler).name:
                link += ["-L-rpath", "-L" + path]
            else:
                link += ["-Xlinker", "-rpath", "-Xlinker", path]
        link += a.link_arg
        link += ["-of=" + str(exe)] if a.engine == "d" and "gdc" not in Path(compiler).name else ["-o", str(exe)]
        invoke(link, a.verbose)
        destination = None
        if stage == "exe" or a.output:
            destination = Path(a.output or "a.out").resolve()
            publish(exe, destination)
        if a.run or stage is None:
            invoke([str(destination or exe), *script_args], a.verbose)
        return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode if error.returncode > 0 else 128 - error.returncode)
    except FileNotFoundError as error:
        print("mull:", error, file=sys.stderr)
        sys.exit(127)
    except (OSError, ValueError, KeyError) as error:
        print("mull:", error, file=sys.stderr)
        sys.exit(1)
