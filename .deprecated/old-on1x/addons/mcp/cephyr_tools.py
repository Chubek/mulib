"""Private stdio-only C11 extraction through isolated Cephyr Python bindings."""

import json
import os
from pathlib import Path
import platform
import re
import stat
import subprocess
import sys
import tempfile

from compiler_sandbox import SYSTEM_DIRS, _seccomp_filter, _system_file, native_file
from core import EvidenceError, decode_json, encode, fail

TOOL = "cephyr_extract"
FORMATS = {
    "parse_c11": "sexp",
    "emit_interface": "sexp",
    "emit_mir": "archon-mir",
}
MAX_SOURCE = 8192
# The MCP response includes the result twice, once escaped as JSON text.
MAX_RESULT = 20 * 1024
WALL_SECONDS = 8
ENV = {"LC_ALL": "C"}

# Inventory exact standard-library files instead of exposing a Python prefix,
# site-packages, or a vendor checkout. Run only this trusted probe on the host;
# the Cephyr binding and worker never load or run there.
PYTHON_INVENTORY = """
import __future__, ctypes, dataclasses, json, pathlib, resource, threading, sys
import encodings.ascii, encodings.latin_1, encodings.utf_8
class NativeLayoutProbe(ctypes.Structure):
    _fields_ = [("value", ctypes.c_int)]
print(json.dumps(sorted({
    p for m in list(sys.modules.values())
    if (p := getattr(m, '__file__', None)) and p.startswith('/')
})))
"""
LDD_LINE = re.compile(r"(?:(\S+) => )?(/[^\n]+) \(0x[0-9a-fA-F]+\)")


def _file(value, *, executable=False):
    path = Path(value)
    try:
        if (not path.is_absolute() or ".." in path.parts
                or not stat.S_ISREG(path.stat().st_mode)
                or (executable and not os.access(path, os.X_OK))):
            raise OSError
        # Resolve the host side when creating the mount, not the sandbox name.
        return str(path.resolve(strict=True))
    except (OSError, RuntimeError, ValueError):
        fail("CONFIGURATION", "Cephyr runtime inputs must be absolute, trusted regular files.")


def _probe(argv):
    # No request content is available in configuration probes. The explicit
    # library and Python paths must be trusted by the local operator; ldd is
    # not appropriate for inspecting an arbitrary native executable.
    try:
        result = subprocess.run(argv, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, env=ENV, cwd="/",
                                check=True, timeout=WALL_SECONDS)
        if len(result.stdout) <= 64 * 1024:
            return result.stdout.decode("utf-8")
    except (OSError, subprocess.SubprocessError, UnicodeError):
        pass
    fail("CONFIGURATION", "Cannot inspect the trusted Cephyr runtime dependencies.")


def _runtime(python, library):
    def mount(source, destination):
        path = _file(source)
        if destination in mounts and mounts[destination] != path:
            fail("CONFIGURATION", "Conflicting Cephyr runtime dependencies.")
        mounts[destination] = path

    mounts = {}
    inventory = _probe([python, "-I", "-S", "-B", "-c", PYTHON_INVENTORY])
    try:
        files = json.loads(inventory)
        if not isinstance(files, list) or not 1 <= len(files) <= 128:
            raise ValueError
    except ValueError:
        fail("CONFIGURATION", "Unexpected Cephyr Python runtime inventory.")
    native = [python, library]
    for filename in files:
        if not isinstance(filename, str) or not filename.startswith("/"):
            fail("CONFIGURATION", "Unexpected Cephyr Python runtime path.")
        mount(filename, filename)
        if filename.endswith(".so"):
            native.append(filename)
    ldd = _file("/usr/bin/ldd", executable=True)
    for binary in native:
        for line in _probe([ldd, binary]).splitlines():
            line = line.strip()
            if line.startswith(("linux-vdso.so.", "linux-gate.so.")):
                continue
            match = LDD_LINE.fullmatch(line)
            if match is None:
                fail("CONFIGURATION", "A Cephyr runtime dependency is unresolved.")
            name, path = match.groups()
            # Keep the ELF interpreter's original absolute name, and put
            # DT_NEEDED libraries into an otherwise empty private directory.
            if name is None or name.startswith("/"):
                mount(path, name or path)
            else:
                if not re.fullmatch(r"[A-Za-z0-9_.+-]{1,255}", name) or name in (".", ".."):
                    fail("CONFIGURATION", "Unexpected Cephyr runtime dependency name.")
                mount(path, "/runtime/lib/" + name)
    return mounts


class CephyrTools:
    """An explicitly enabled provider; bindings is the cephyr/__init__.py path."""

    def __init__(self, bindings, library, *, resource_root, archon_root,
                 python="/usr/bin/python3", bwrap="/usr/bin/bwrap"):
        if (sys.platform != "linux" or os.geteuid() == 0
                or os.getuid() != os.geteuid() or os.getgid() != os.getegid()):
            fail("CONFIGURATION", "Run Cephyr MCP on Linux as a non-root, non-set-ID user.")
        if Path(bindings).name != "__init__.py":
            fail("CONFIGURATION", "Set the trusted path to Cephyr's Python package __init__.py.")
        binding = _file(bindings)
        sdk = _file(Path(bindings).with_name("_sdk.py"))
        library = str(native_file(library))
        python_executable = str(native_file(python, executable=True))
        bwrap = str(native_file(bwrap, executable=True))
        system = SYSTEM_DIRS.get(platform.machine())
        if system is None:
            fail("SANDBOX_UNAVAILABLE", "Cephyr extraction requires supported Linux sandboxing.")
        self.seccomp = native_file(_system_file("libseccomp.so.2", system[1]))
        target = _file(Path(resource_root) / "triples/x86_64-linux-gnu.lua")
        specification = _file(Path(archon_root) / "stdspec/amd64/root.arch")
        # Keep the configured Python name in the sandbox so it can find its
        # standard-library layout; only the resolved executable gets mounted.
        python = str(Path(python))
        mounts = _runtime(python, library)
        mounts.update({
            python: python_executable,
            "/app/worker.py": _file(Path(__file__).resolve().with_name("cephyr_worker.py")),
            "/app/cephyr/__init__.py": binding,
            "/app/cephyr/_sdk.py": sdk,
            "/toolchain/cephyr/triples/x86_64-linux-gnu.lua": target,
            "/toolchain/archon/stdspec/amd64/root.arch": specification,
            "/toolchain/libcephyr.so": library,
        })
        self.command = [
            bwrap, "--unshare-all", "--unshare-user", "--disable-userns",
            "--assert-userns-disabled", "--cap-drop", "ALL",
            "--die-with-parent", "--new-session", "--clearenv",
            "--setenv", "LD_LIBRARY_PATH", "/runtime/lib", "--chdir", "/",
        ]
        for destination, source in sorted(mounts.items()):
            self.command += ["--ro-bind", source, destination]
        self.command += ["--remount-ro", "/"]
        self.worker = [python, "-I", "-S", "-B", "-X", "utf8", "/app/worker.py"]

    def close(self):
        """No resources or worker processes persist between requests."""

    @staticmethod
    def tools():
        return [{
            "name": TOOL,
            "description": ("Extract a C11 AST, resolved interface, or Archon MachineIR from "
                            "standalone C source for x86_64-linux-gnu using Cephyr. No system "
                            "or project headers, custom flags, binary artifacts, or execution."),
            "inputSchema": {
                "type": "object",
                "required": ["source", "operation"],
                "additionalProperties": False,
                "properties": {
                    "source": {"type": "string", "minLength": 1, "maxLength": MAX_SOURCE,
                               "description": "Standalone C11, at most 8192 UTF-8 bytes."},
                    "operation": {"type": "string", "enum": list(FORMATS)},
                },
            },
            "annotations": {"readOnlyHint": True, "openWorldHint": False},
        }]

    def call(self, name, args):
        if (name != TOOL or not isinstance(args, dict) or set(args) != {"source", "operation"}
                or not isinstance(args["operation"], str) or args["operation"] not in FORMATS):
            fail("INVALID_ARGUMENT", "Expected a Cephyr extraction operation and source only.")
        source, operation = args["source"], args["operation"]
        if not isinstance(source, str) or len(source) > MAX_SOURCE or not source.strip():
            fail("INVALID_ARGUMENT", "Expected nonempty standalone C11 source of at most 8192 UTF-8 bytes.")
        try:
            data = source.encode("utf-8")
        except UnicodeError:
            fail("INVALID_ARGUMENT", "Expected valid UTF-8 C11 source.")
        if b"\0" in data or len(data) > MAX_SOURCE:
            fail("INVALID_ARGUMENT", "C11 source must fit 8192 UTF-8 bytes without NUL.")

        try:
            # stdout is an anonymous regular file so the child's RLIMIT_FSIZE
            # applies; unbounded capture_output/communicate pipes would not.
            with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as seccomp:
                _seccomp_filter(seccomp, self.seccomp)
                command = [*self.command, "--seccomp", str(seccomp.fileno()), "--", *self.worker, operation]
                with subprocess.Popen(command, stdin=subprocess.PIPE, pass_fds=(seccomp.fileno(),),
                                      stdout=output, stderr=subprocess.DEVNULL,
                                      cwd="/", env=ENV, start_new_session=True) as child:
                    try:
                        child.communicate(data, timeout=WALL_SECONDS)
                    except subprocess.TimeoutExpired:
                        fail("CEPHYR_TIMEOUT", "Sandboxed Cephyr extraction timed out.")
                    finally:
                        if child.poll() is None:
                            child.kill()
                        child.wait()
                    if child.returncode == 3:
                        fail("RESULT_LIMIT", "Cephyr extraction exceeded the output budget.")
                    if child.returncode != 0:
                        fail("CEPHYR_FAILED", "Cephyr extraction failed in isolation; no unsandboxed fallback.")
                output.seek(0)
                raw = output.read(MAX_RESULT + 1)
        except OSError:
            fail("CEPHYR_FAILED", "Could not run isolated Cephyr extraction.")
        if len(raw) > MAX_RESULT:
            fail("RESULT_LIMIT", "Cephyr extraction exceeded the output budget.")
        try:
            result = decode_json(raw)
        except EvidenceError:
            fail("CEPHYR_FAILED", "Cephyr worker did not return a valid result.")
        if not _valid_result(result) or len(encode(result)) > MAX_RESULT:
            fail("CEPHYR_FAILED", "Cephyr worker did not return a valid result.")
        return {"operation": operation, "format": FORMATS[operation], **result}


def _valid_result(result):
    if (not isinstance(result, dict)
            or set(result) != {"status", "output", "diagnostics", "diagnostics_truncated"}
            or result["status"] not in ("ok", "compile_error")
            or not isinstance(result["output"], str)
            or not isinstance(result["diagnostics"], list)
            or type(result["diagnostics_truncated"]) is not bool
            or len(result["diagnostics"]) > 12):
        return False
    for item in result["diagnostics"]:
        if (not isinstance(item, dict) or set(item) - {"severity", "code", "message", "line", "column"}
                or item.get("severity") not in ("error", "warning", "note")
                or not isinstance(item.get("code"), str) or len(item["code"]) > 100
                or not isinstance(item.get("message"), str) or len(item["message"]) > 384
                or any(type(item[key]) is not int or not 0 < item[key] <= 2147483647
                       for key in ("line", "column") if key in item)):
            return False
    return True
