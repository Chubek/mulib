"""Process boundary for private stdio inspection tools; never use for HTTP."""
import ctypes
import errno
import os
from pathlib import Path
import platform
import re
import signal
import stat
import subprocess
import sys
import tempfile

from core import fail

MAX_SOURCE_BYTES = 8192
MAX_STREAM = 256 * 1024
TIMEOUT = 8
ENV = {"LC_ALL": "C"}
SYSTEM_DIRS = {
    "x86_64": ("/lib64/ld-linux-x86-64.so.2", "x86_64-linux-gnu"),
    "aarch64": ("/lib/ld-linux-aarch64.so.1", "aarch64-linux-gnu"),
    "riscv64": ("/lib/ld-linux-riscv64-lp64d.so.1", "riscv64-linux-gnu"),
}
SYSTEM_LIBS = ("libc.so.6", "libm.so.6", "libstdc++.so.6", "libgcc_s.so.1")


def source_bytes(source):
    if not isinstance(source, str) or len(source) > MAX_SOURCE_BYTES or not source.strip():
        fail("INVALID_ARGUMENT", "Expected bounded, nonempty standalone source.")
    try:
        data = source.encode("utf-8")
    except UnicodeError:
        fail("INVALID_ARGUMENT", "Source must be UTF-8.")
    if len(data) > MAX_SOURCE_BYTES or b"\0" in data:
        fail("INVALID_ARGUMENT", f"Source must fit {MAX_SOURCE_BYTES} UTF-8 bytes without NUL.")
    return data


def native_file(value, *, executable=False):
    """Only the operator configures tool/library paths. No scripts or set-ID executables."""
    path = Path(value)
    if not path.is_absolute():
        fail("CONFIGURATION", "Compiler sandbox tools and dependencies require absolute paths.")
    try:
        path = path.resolve(strict=True)
        info = path.stat()
        if (not stat.S_ISREG(info.st_mode) or info.st_mode & (stat.S_ISUID | stat.S_ISGID)
                or executable and not os.access(path, os.X_OK)):
            fail("CONFIGURATION", "Sandbox tool or dependency is not a regular native file.")
        with path.open("rb") as stream:
            if stream.read(4) != b"\x7fELF":
                fail("CONFIGURATION", "Sandbox tools and dependencies must be native ELF files.")
    except (OSError, RuntimeError):
        fail("CONFIGURATION", "Cannot resolve a configured native sandbox tool or dependency.")
    return path


def _system_file(name, triplet):
    for base in (f"/lib/{triplet}", f"/usr/lib/{triplet}", "/lib64", "/usr/lib64", "/lib", "/usr/lib"):
        path = Path(base) / name
        if path.is_file():
            return path
    fail("SANDBOX_UNAVAILABLE", f"Required sandbox runtime {name} was not found.")


def _seccomp_filter(stream, library):
    # No exec of generated code: the native compiler may not create processes
    # or open sockets. Namespace isolation is independent of this filter.
    lib = ctypes.CDLL(str(library))
    lib.seccomp_init.argtypes = [ctypes.c_uint32]
    lib.seccomp_init.restype = ctypes.c_void_p
    lib.seccomp_release.argtypes = [ctypes.c_void_p]
    lib.seccomp_syscall_resolve_name.argtypes = [ctypes.c_char_p]
    lib.seccomp_rule_add.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int, ctypes.c_uint]
    lib.seccomp_export_bpf.argtypes = [ctypes.c_void_p, ctypes.c_int]
    ctx = lib.seccomp_init(0x7fff0000)  # SCMP_ACT_ALLOW
    if not ctx:
        fail("SANDBOX_UNAVAILABLE", "Could not initialize the compiler seccomp filter.")
    try:
        for name in (b"clone", b"clone3", b"fork", b"vfork", b"socket", b"socketpair",
                     b"io_uring_setup", b"unshare", b"setns"):
            syscall = lib.seccomp_syscall_resolve_name(name)
            if syscall < 0 and name in (b"fork", b"vfork"):
                continue  # Absent from some Linux native ABIs.
            if syscall < 0 or lib.seccomp_rule_add(ctx, 0x00050000 | errno.EPERM, syscall, 0):
                fail("SANDBOX_UNAVAILABLE", "Could not build the compiler seccomp filter.")
        if lib.seccomp_export_bpf(ctx, stream.fileno()):
            fail("SANDBOX_UNAVAILABLE", "Could not export the compiler seccomp filter.")
        stream.seek(0)
    finally:
        lib.seccomp_release(ctx)


def diagnostic(raw):
    text = raw[:2048].decode("utf-8", "backslashreplace")
    return "".join(c if c in "\n\t" or c.isprintable() else f"\\u{ord(c):04x}"
                   for c in text).strip()


class Sandbox:
    def __init__(self, bwrap="/usr/bin/bwrap", prlimit="/usr/bin/prlimit", runtime_files=()):
        if sys.platform != "linux" or platform.machine() not in SYSTEM_DIRS:
            fail("SANDBOX_UNAVAILABLE", "Compiler inspection requires supported Linux sandboxing.")
        if os.getuid() == 0 or os.getuid() != os.geteuid() or os.getgid() != os.getegid():
            fail("CONFIGURATION", "Run compiler MCP as a non-root, non-set-ID user.")
        self.bwrap = native_file(bwrap, executable=True)
        self.prlimit = native_file(prlimit, executable=True)
        loader, triplet = SYSTEM_DIRS[platform.machine()]
        self.seccomp = _system_file("libseccomp.so.2", triplet).resolve(strict=True)
        self.mounts = [("--ro-bind", str(native_file(loader)), loader)]
        libraries = {name: _system_file(name, triplet) for name in SYSTEM_LIBS}
        if len(runtime_files) > 32:
            fail("CONFIGURATION", "At most 32 additional native runtime files are permitted.")
        for file in runtime_files:
            path = Path(file)
            if not re.fullmatch(r"lib[A-Za-z0-9_+.-]+\.so(?:\.[A-Za-z0-9_.+-]+)?", path.name):
                fail("CONFIGURATION", "Additional runtime files must have shared-library names.")
            if path.name in libraries:
                fail("CONFIGURATION", "Duplicate or reserved compiler runtime library name.")
            libraries[path.name] = path
        self.mounts += [("--ro-bind", str(native_file(path)), f"/runtime/{name}")
                        for name, path in libraries.items()]

    def run(self, executable, argv, *, source, filename):
        """Run one allowlisted provider operation. argv is trusted code, never request flags."""
        if (not isinstance(source, bytes) or not 0 < len(source) <= MAX_SOURCE_BYTES or b"\0" in source
                or not re.fullmatch(r"source\.[a-z0-9]{1,8}", filename)):
            fail("INVALID_ARGUMENT", "Invalid sandbox source or fixed sandbox filename.")
        compiler = native_file(executable, executable=True)
        try:
            with tempfile.TemporaryDirectory(prefix="on1x-mcp-compile-") as stage:
                return self._run(compiler, argv, source, filename, Path(stage))
        except (OSError, AttributeError, subprocess.SubprocessError):
            fail("SANDBOX_UNAVAILABLE", "Cannot establish local compiler isolation; no fallback was run.")

    def _run(self, compiler, argv, source, filename, stage):
        path = stage / filename
        path.write_bytes(source)
        with (tempfile.TemporaryFile(dir=stage) as stdout,
              tempfile.TemporaryFile(dir=stage) as stderr,
              tempfile.TemporaryFile(dir=stage) as seccomp):
            _seccomp_filter(seccomp, self.seccomp)
            command = [
                str(self.prlimit), "--as=1073741824", "--cpu=6", f"--fsize={MAX_STREAM}",
                "--core=0", "--nofile=64", "--",
                str(self.bwrap), "--unshare-user", "--unshare-net", "--unshare-pid", "--unshare-ipc",
                "--unshare-uts", "--disable-userns", "--assert-userns-disabled", "--cap-drop", "ALL",
                "--die-with-parent", "--new-session", "--clearenv", "--setenv", "LC_ALL", "C",
                "--setenv", "PATH", "/nonexistent", "--setenv", "LD_LIBRARY_PATH", "/runtime",
                "--setenv", "TMPDIR", "/tmp", "--size", str(4 * 1024 * 1024), "--tmpfs", "/tmp",
                "--ro-bind", str(path), f"/work/{filename}", "--ro-bind", str(compiler), "/driver/tool",
                *(arg for mount in self.mounts for arg in mount),
                "--remount-ro", "/", "--chdir", "/work", "--seccomp", str(seccomp.fileno()),
                "--", "/driver/tool", *argv,
            ]
            with subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                                  env=ENV, cwd=stage, start_new_session=True,
                                  pass_fds=(seccomp.fileno(),)) as process:
                try:
                    code = process.wait(timeout=TIMEOUT)
                except subprocess.TimeoutExpired:
                    fail("COMPILE_TIMEOUT", "Sandboxed inspection exceeded the wall-time limit.")
                finally:
                    if process.returncode is None:
                        try:
                            os.killpg(process.pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
            if any(os.fstat(stream.fileno()).st_size >= MAX_STREAM for stream in (stdout, stderr)):
                fail("RESULT_LIMIT", "Compiler output exceeded its stream budget; use smaller source.")
            stderr.seek(0)
            errors = stderr.read(MAX_STREAM)
            if code:
                if errors.startswith((b"bwrap:", b"prlimit:", b"/driver/tool: error while loading")):
                    fail("SANDBOX_UNAVAILABLE", diagnostic(errors))
                fail("COMPILER_FAILED", diagnostic(errors) or f"Compiler exited {code}.")
            stdout.seek(0)
            return stdout.read(MAX_STREAM), errors
