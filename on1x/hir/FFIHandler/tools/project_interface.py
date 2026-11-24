"""Run the pinned Cephyr C11Interface CLI with a controlled per-TU profile.

This is not a sandbox. The caller selects trusted tool files, input roots and
TUs, and must ensure includes stay within those roots. A Projection is bounded
transport output, not a decoded interface, export proof, or ABI admission.
"""

from dataclasses import asdict, dataclass
import hashlib
from itertools import islice
import json
import math
import os
from pathlib import Path
import re
import selectors
import signal
import stat
import subprocess
import tempfile
import time


TARGET_ABIS = {
    "x86_64-linux-gnu": "SysVAMD64",
    "aarch64-linux-gnu": "AAPCS64",
    "riscv64-linux-gnu": "LP64",
}
_ENV = {"PATH": "/usr/bin:/bin", "LC_ALL": "C", "TZ": "UTC0", "HOME": ".", "TMPDIR": "."}
_CONFIG = b"# On1x projection: no ambient settings or extensions\n"
_STAGE = "--emit-interface=sexp"
_LANGUAGES = ("c", "cpp-output", "c11-ast-json", "c11-ast-yaml", "c11-ast-sexp")
_MACRO = re.compile(r"[A-Za-z_][A-Za-z_0-9]*(?:=[^\x00-\x1f\x7f]*)?")
_HEADER = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_.+-]*(?:/[A-Za-z0-9_][A-Za-z0-9_.+-]*)*")
_MIB = 1024 * 1024
_PROFILE_ITEMS = 128


@dataclass(frozen=True)
class Limits:
    timeout_seconds: float = 15.0
    input_bytes: int = 16 * _MIB
    output_bytes: int = 16 * _MIB
    diagnostic_bytes: int = 256 * 1024
    address_space_bytes: int = 1024 * _MIB
    inventory_bytes: int = 1024 * _MIB
    inventory_entries: int = 32768

    def __post_init__(self):
        if not 0 < self.timeout_seconds <= 300 or not math.isfinite(self.timeout_seconds):
            raise ValueError("invalid subprocess timeout")
        maxima = {"input_bytes": 64 * _MIB, "output_bytes": 64 * _MIB,
                  "diagnostic_bytes": _MIB, "address_space_bytes": 8 * 1024 * _MIB,
                  "inventory_bytes": 4 * 1024 * _MIB, "inventory_entries": 131072}
        for name, maximum in maxima.items():
            value = getattr(self, name)
            if type(value) is not int or not 1 <= value <= maximum:
                raise ValueError(f"invalid limit: {name}")


@dataclass(frozen=True)
class Projection:
    output: bytes
    diagnostics: bytes
    provenance: dict


class ProjectionError(Exception):
    def __init__(self, code, message, *, diagnostics=b"", provenance=None):
        super().__init__(message)
        self.code = code
        self.diagnostics = diagnostics
        self.provenance = {} if provenance is None else provenance


def _absolute(value):
    path = Path(value)
    if not path.is_absolute() or len(os.fsencode(path)) > 4096 or "\0" in str(path):
        raise ProjectionError("invalid_profile", "paths must be bounded and absolute")
    # Normalize '..' only by resolving the actual filesystem. Refuse symlinks
    # when inventorying beneath a root, where they could escape the inventory.
    try:
        return path.resolve(strict=True)
    except (OSError, RuntimeError) as exc:
        raise ProjectionError("missing_input", f"cannot resolve selected path: {path}") from exc


def _digest(data):
    return hashlib.sha256(data).hexdigest()


def _record_digest(record):
    return _digest(json.dumps(record, separators=(",", ":"), sort_keys=True).encode("ascii"))


def _bounded_tuple(group):
    result = tuple(islice(group, _PROFILE_ITEMS + 1))
    if len(result) > _PROFILE_ITEMS:
        raise ProjectionError("invalid_profile", "too many include paths, tool inputs or macros")
    return result


def _stat_id(info):
    return (info.st_dev, info.st_ino, info.st_mode, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def _file_identity(path, remaining):
    # O_NONBLOCK prevents a concurrent replacement with a FIFO from hanging.
    with os.fdopen(os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW), "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > remaining:
            raise ProjectionError("inventory_limit", f"invalid file or inventory byte limit: {path}")
        digest = hashlib.sha256()
        size = 0
        while block := stream.read(min(128 * 1024, remaining - size + 1)):
            size += len(block)
            if size > remaining:
                raise ProjectionError("inventory_limit", "input inventory byte limit exceeded")
            digest.update(block)
        stamp = _stat_id(info)
        if size != info.st_size or stamp != _stat_id(path.lstat()):
            raise ProjectionError("input_changed", f"selected input changed: {path}")
        return stamp, size, digest.hexdigest()


def _inventory(roots, limits):
    records, stamps, byte_count, entry_count = [], {}, 0, 0
    try:
        for label, root in roots:
            entries = []
            pending = [root]
            # Count directories too, so empty directory trees cannot evade the budget.
            queued = 1
            while pending:
                path = pending.pop()
                info = path.lstat()
                entry_count += 1
                if entry_count > limits.inventory_entries:
                    raise ProjectionError("inventory_limit", "input inventory entry limit exceeded")
                relative = path.relative_to(root).as_posix()
                if stat.S_ISDIR(info.st_mode):
                    with os.scandir(path) as children:
                        for child in children:
                            queued += 1
                            if queued > limits.inventory_entries:
                                raise ProjectionError("inventory_limit", "input inventory entry limit exceeded")
                            pending.append(Path(child.path))
                    stamp = _stat_id(info)
                    if stamp != _stat_id(path.lstat()):
                        raise ProjectionError("input_changed", f"selected directory changed: {path}")
                    entries.append((relative, "directory"))
                elif stat.S_ISREG(info.st_mode):
                    stamp, size, digest = _file_identity(path, limits.inventory_bytes - byte_count)
                    byte_count += size
                    entries.append((relative, digest))
                else:
                    raise ProjectionError("invalid_profile", f"non-regular input in inventory: {path}")
                stamps[(label, relative)] = stamp
            if len(entries) == 1 and entries[0][1] != "directory":
                records.append({"role": label, "kind": "file", "sha256": entries[0][1]})
            else:
                records.append({"role": label, "kind": "directory", "entries": len(entries),
                                "sha256": _record_digest(sorted(entries))})
    except OSError as exc:
        raise ProjectionError("input_io", f"cannot inventory selected inputs: {exc}") from exc
    return records, stamps


def _capture(argv, source, cwd, limits):
    deadline = time.monotonic() + limits.timeout_seconds
    buffers = [bytearray(), bytearray()]
    caps = [limits.output_bytes, limits.diagnostic_bytes]
    try:
        with tempfile.TemporaryFile(dir=cwd) as stdin:
            stdin.write(source)
            stdin.seek(0)
            process = subprocess.Popen(argv, stdin=stdin, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, cwd=cwd, env=_ENV,
                                       start_new_session=True, close_fds=True)
            try:
                with selectors.DefaultSelector() as selector:
                    selector.register(process.stdout, selectors.EVENT_READ, 0)
                    selector.register(process.stderr, selectors.EVENT_READ, 1)
                    while selector.get_map():
                        left = deadline - time.monotonic()
                        if left <= 0:
                            raise subprocess.TimeoutExpired(argv, limits.timeout_seconds)
                        for key, _ in selector.select(left):
                            i = key.data
                            capacity = caps[i] - len(buffers[i])
                            chunk = os.read(key.fd, min(65536, capacity + 1))
                            if not chunk:
                                selector.unregister(key.fileobj)
                            elif len(chunk) > capacity:
                                buffers[i].extend(chunk[:capacity])
                                stream = ("stdout", "stderr")[i]
                                raise ProjectionError(
                                    ("output_limit", "diagnostic_limit")[i],
                                    f"Cephyr {stream} exceeded {caps[i]} bytes",
                                    diagnostics=bytes(buffers[1]),
                                )
                            else:
                                buffers[i].extend(chunk)
                status = process.wait(timeout=max(0.0, deadline - time.monotonic()))
                return status, bytes(buffers[0]), bytes(buffers[1])
            finally:
                # Avoid signaling an already reaped process group after normal exit.
                # On error, kill the group even if its leader has already exited.
                if process.returncode is None:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    process.wait()
                process.stdout.close()
                process.stderr.close()
    except subprocess.TimeoutExpired as exc:
        raise ProjectionError("timeout", "Cephyr exceeded its wall-time limit",
                              diagnostics=bytes(buffers[1])) from exc
    except OSError as exc:
        raise ProjectionError("process_io", f"could not run Cephyr: {exc}",
                              diagnostics=bytes(buffers[1])) from exc


class CephyrProvider:
    """Reusable batch snapshot for one target and explicit include profile.

    Select the actual ELF, shared compiler library and built-in target resource
    bank for reproducibility (``tool_inputs``). Call ``verify_inputs`` after all
    TUs and before publishing. Use provenance["identity"] for relocatable
    records; provenance["invocation"] is local evidence with absolute paths.

    ``--stdlib=none -nostdinc`` suppresses all Cephyr default include roots.
    An explicit sysroot is inventoried but not searched automatically. Add
    any intended sysroot include directories to the explicit search profile.
    """

    def __init__(self, cephyr, target, *, libstone_revision, scratch_root,
                 include_dirs=(), system_include_dirs=(), tool_inputs=(),
                 defines=(), undefines=(), sysroot=None, freestanding=False,
                 limits=Limits(), prlimit="/usr/bin/prlimit"):
        if (not isinstance(target, str) or target not in TARGET_ABIS or
                not isinstance(libstone_revision, str) or not re.fullmatch(r"[0-9a-f]{40}", libstone_revision)):
            raise ProjectionError("invalid_profile", "select a built-in target and full LibStone revision")
        self.target = target
        self.limits = limits
        self.scratch_root = _absolute(scratch_root)
        self.cephyr = _absolute(cephyr)
        self.prlimit = _absolute(prlimit)
        if (not self.scratch_root.is_dir() or
                any(not p.is_file() or not os.access(p, os.X_OK) for p in (self.cephyr, self.prlimit))):
            raise ProjectionError("invalid_profile", "need an executable Cephyr and prlimit, and scratch directory")
        include_dirs, system_include_dirs, tool_inputs, defines, undefines = map(
            _bounded_tuple, (include_dirs, system_include_dirs, tool_inputs, defines, undefines)
        )
        if any(not isinstance(m, str) or len(m) > 4096 or not _MACRO.fullmatch(m)
               for m in (*defines, *undefines)) or any("=" in m for m in undefines):
            raise ProjectionError("invalid_profile", "invalid preprocessing macro")
        self.include_dirs, self.system_include_dirs, tools = (
            tuple(map(_absolute, group)) for group in (include_dirs, system_include_dirs, tool_inputs)
        )
        paths = (*self.include_dirs, *self.system_include_dirs)
        self.sysroot = _absolute(sysroot) if sysroot is not None else None
        if any(not p.is_dir() for p in paths) or (self.sysroot is not None and not self.sysroot.is_dir()):
            raise ProjectionError("invalid_profile", "include roots and sysroot must be directories")
        # Older callers also list the sysroot under tool_inputs. Inventory it
        # once under its own role so it cannot consume the budget twice.
        tools = tuple(path for path in tools if path != self.sysroot)
        groups = (("include", self.include_dirs), ("system", self.system_include_dirs), ("tool", tools))
        self._roots = (("cephyr", self.cephyr), ("prlimit", self.prlimit), *(
            (f"{kind}:{i}", path) for kind, group in groups for i, path in enumerate(group)
        ))
        if self.sysroot is not None:
            self._roots += (("sysroot", self.sysroot),)
        # Scratch must not alter a snapshotted header or resource inventory.
        if any(self.scratch_root.is_relative_to(path) for _, path in self._roots if path.is_dir()):
            raise ProjectionError("invalid_profile", "scratch directory is inside an input root")
        records, self._stamps = _inventory(self._roots, limits)
        self._tool_files = tuple((label, path) for (label, path), record in zip(self._roots, records)
                                 if record["kind"] == "file")
        self._wrapper = (str(self.prlimit), f"--as={limits.address_space_bytes}",
                         "--core=0", "--", str(self.cephyr))
        self._options = (f"--target={target}", "-std=c11", "--config", "Cephyr.toml",
                         "--no-cache", "--stdlib=none", "-nostdinc")
        if self.sysroot is not None:
            self._options += ("--sysroot=" + str(self.sysroot),)
        if freestanding:
            self._options += ("-ffreestanding",)
        self._options += tuple(item for option, group in (
            ("-I", self.include_dirs), ("-isystem", self.system_include_dirs),
            ("-D", defines), ("-U", undefines),
        ) for value in group for item in (option, str(value)))
        self._bound_argv(self._wrapper + self._options)
        version = self._run(("--version",), b"")
        help_text = self._run(("--help",), b"")
        if not re.fullmatch(rb"Cephyr [ -~]{1,96}\n", version.output) or _STAGE.encode() not in help_text.output:
            raise ProjectionError("unsupported_cephyr", "selected executable lacks Cephyr C11Interface support")
        self._identity = {
            "schema": 1, "target": target, "abi": TARGET_ABIS[target],
            "libstone_revision": libstone_revision, "version": version.output.decode("ascii").strip(),
            "cephyr_sha256": records[0]["sha256"],
            "config_sha256": _digest(_CONFIG), "environment": dict(_ENV),
            "dependency_method": "conservative_declared_root_inventory",
            "roots": records, "tool_inputs_declared": bool(tools),
            "preprocessor": {"standard": "c11", "default_includes": False, "defines": defines,
                             "undefines": undefines, "freestanding": freestanding,
                             "sysroot_selected": self.sysroot is not None},
            "limits": asdict(limits),
        }

    def verify_inputs(self):
        """Fail if selected tool/input trees changed since profile construction."""
        records, stamps = _inventory(self._roots, self.limits)
        if records != self._identity["roots"] or stamps != self._stamps:
            raise ProjectionError("input_changed", "projection tool or input inventory changed during the batch")

    @staticmethod
    def _bound_argv(argv):
        if len(argv) > 1024 or sum(len(os.fsencode(a)) + 1 for a in argv) > 65536:
            raise ProjectionError("invalid_profile", "Cephyr argument budget exceeded")

    def _run(self, arguments, source):
        argv = self._wrapper + tuple(arguments)
        self._bound_argv(argv)
        invocation = {"argv": list(argv), "cwd_policy": "private scratch directory", "env": dict(_ENV),
                      "selected_input_paths": {label: str(path) for label, path in self._roots}}
        try:
            self._verify_tool_files()
            # Never inherit caller cwd, CEPHYR_CONFIG, compiler/tool variables or hooks.
            with tempfile.TemporaryDirectory(prefix="on1x-cephyr-", dir=self.scratch_root) as cwd:
                invocation["cwd"] = cwd
                (Path(cwd) / "Cephyr.toml").write_bytes(_CONFIG)
                status, output, diagnostics = _capture(argv, source, cwd, self.limits)
            invocation["exit_status"] = status
            self._verify_tool_files()
        except ProjectionError as exc:
            exc.provenance["invocation"] = invocation
            raise
        except OSError as exc:
            raise ProjectionError("scratch_io", f"could not prepare Cephyr scratch: {exc}",
                                  provenance={"invocation": invocation}) from exc
        if status != 0:
            code = "cephyr_failed" if status > 0 else "cephyr_signaled"
            raise ProjectionError(code, f"Cephyr exited with status {status}", diagnostics=diagnostics,
                                  provenance={"invocation": invocation})
        return Projection(output, diagnostics, {"invocation": invocation})

    def _verify_tool_files(self):
        for label, path in self._tool_files:
            try:
                if _stat_id(path.lstat()) == self._stamps[(label, ".")]:
                    continue
            except OSError:
                pass
            raise ProjectionError("input_changed", f"selected tool input changed: {path}")

    def check_public_header(self, header):
        """Require a unique explicit include root for a relative manifest header."""
        if not isinstance(header, str) or len(header) > 2048 or not _HEADER.fullmatch(header):
            raise ProjectionError("invalid_header", "expected a relative manifest include name")
        roots = dict.fromkeys((*self.include_dirs, *self.system_include_dirs))
        matches = [root / header for root in roots if (root / header).is_file()]
        if not matches:
            raise ProjectionError("missing_header", f"header {header!r} is absent from the selected roots")
        if len(matches) != 1:
            raise ProjectionError("ambiguous_header", f"header {header!r} is present in multiple selected roots")
        return matches[0]

    def project_header(self, header):
        """Construct an include-only TU for a manifest header, without implementation switches."""
        self.check_public_header(header)
        return self._project(f'#include "{header}"\n'.encode("ascii"), "c", header)

    def project(self, source, *, language="c"):
        """Project one raw C TU, preprocessed C TU, or serialized C11Syntax input."""
        return self._project(source, language, None)

    def _project(self, source, language, header):
        if language not in _LANGUAGES or not isinstance(source, bytes):
            raise ProjectionError("invalid_input", "expected bytes in a supported translation-unit language")
        if len(source) > self.limits.input_bytes:
            raise ProjectionError("input_limit", "Cephyr input exceeds its byte limit")
        if b"\x00" in source:
            raise ProjectionError("invalid_input", "Cephyr input contains a NUL byte")
        # Deep-copy JSON data before exposing it to callers sharing this profile.
        identity = json.loads(json.dumps(self._identity))
        identity.update({"stdin_sha256": _digest(source), "language": language, "header": header,
                         "stage": _STAGE, "origin": "unavailable"})
        try:
            run = self._run((*self._options, _STAGE, "-x", language, "-"), source)
        except ProjectionError as exc:
            exc.provenance["identity"] = identity
            raise
        output = run.output
        # Check only the pinned CLI transport envelope here. W01 checks actual
        # S-expression structure, schema and target again before consumption.
        prefix = (f'(interface 1\n  (target "{self.target}")\n  (data-model LP64)\n'
                  f'  (abi "{TARGET_ABIS[self.target]}")').encode("ascii")
        try:
            output.decode("utf-8")
            valid = output.startswith(prefix) and output.endswith(b"))\n") and b"\x00" not in output
        except UnicodeDecodeError:
            valid = False
        run.provenance["identity"] = identity
        if not valid:
            raise ProjectionError("invalid_output", "Cephyr output lacks the expected v1 transport envelope",
                                  diagnostics=run.diagnostics, provenance=run.provenance)
        identity["projection_sha256"] = _digest(output)
        return run
