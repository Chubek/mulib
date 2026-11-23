"""Host-only capability dispatcher for data requests from a harness client.

Nothing here registers a MuLL or On1x native import. Construct policy in the
trusted host, not from the request. One dispatcher is used serially per session.
"""
from dataclasses import dataclass, field
import os
from pathlib import Path
import selectors
import stat
import subprocess
import sys
import time


class Denied(Exception):
    """An ungranted, invalid, or unavailable operation."""


class Limit(Denied):
    """A session or operation limit was reached."""


@dataclass(frozen=True)
class Tool:
    # The host owns the image and must keep it immutable for the session.
    image: Path
    argv: tuple[str, ...]
    environment: dict[str, str] = field(default_factory=dict)


def _components(path):
    if type(path) is not str or not 0 < len(path) <= 4096 or "\0" in path or "\\" in path:
        raise Denied("invalid relative path")
    parts = path.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise Denied("invalid relative path")
    return parts


class Supervisor:
    """One Linux session with pinned workspace roots and fixed tool recipes."""

    def __init__(self, *, roots, tools, launcher=None, calls, output_bytes,
                 per_call_bytes, seconds, per_tool_seconds):
        if (sys.platform != "linux" or not hasattr(os, "O_PATH") or
                not Path("/proc/self/fd").is_dir()):
            raise Denied("descriptor supervisor unavailable")
        if (any(type(n) is not int or n <= 0 for n in (calls, output_bytes, per_call_bytes))
                or not (0 < per_tool_seconds <= seconds < float("inf"))):
            raise ValueError("explicit positive budgets required")
        self._deadline = time.monotonic() + seconds
        self._calls = calls
        self._bytes = output_bytes
        self._per_call = per_call_bytes
        self._per_tool = per_tool_seconds
        self._roots = {}
        self._tools = {}
        self._closed = False
        self._launcher = None if launcher is None else Path(launcher).resolve(strict=True)
        if self._launcher is not None and not self._launcher.is_file():
            raise ValueError("trusted launcher must be a regular file")
        for key, tool in tools.items():
            image = tool.image.resolve(strict=True)
            argv = tuple(tool.argv)
            if (image == Path("/") or not image.is_dir() or not argv or
                    any(type(arg) is not str or "\0" in arg for arg in argv) or
                    not argv[0].startswith("/") or argv[0].startswith("//")):
                raise ValueError("tool requires a dedicated image and absolute executable")
            _components(argv[0][1:])
            env = dict(tool.environment)
            if any(not k or "=" in k or "\0" in k or "\0" in v for k, v in env.items()):
                raise ValueError("invalid tool environment")
            self._tools[key] = Tool(image, argv, env)
        try:
            for key, root in roots.items():
                if not Path(root).is_absolute():
                    raise ValueError("workspace roots must be absolute host paths")
                self._roots[key] = os.open(root, os.O_PATH | os.O_DIRECTORY |
                                           os.O_NOFOLLOW | os.O_CLOEXEC)
        except BaseException:
            self.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def close(self):
        self._closed = True
        for fd in self._roots.values():
            os.close(fd)
        self._roots.clear()

    def dispatch(self, request):
        """Accept an exact data-only read/run request. Network is not granted."""
        if self._closed:
            raise Denied("session closed")
        if not self._calls or not self._bytes or time.monotonic() >= self._deadline:
            raise Limit("session budget exhausted")
        self._calls -= 1
        if (type(request) is not dict or len(request) not in (2, 3) or
                any(type(k) is not str or type(v) is not str or len(v) > 4096
                    for k, v in request.items())):
            raise Denied("invalid request")
        limit = min(self._per_call, self._bytes)
        try:
            if request.get("op") == "read" and request.keys() == {"op", "root", "path"}:
                fd = self._roots.get(request["root"])
                if fd is None:
                    raise Denied("workspace not granted")
                data = self._read(fd, request["path"], limit)
            elif request.get("op") == "run" and request.keys() == {"op", "tool"}:
                tool = self._tools.get(request["tool"])
                if tool is None or self._launcher is None:
                    raise Denied("tool or isolation not granted")
                data = self._run(tool, limit)
            else:
                raise Denied("operation not granted")
        except (OSError, UnicodeError):
            raise Denied("host operation unavailable") from None
        if time.monotonic() >= self._deadline:
            raise Limit("session deadline exceeded")
        self._bytes -= len(data)
        return data

    def _read(self, root, path, limit):
        flags = os.O_PATH | os.O_NOFOLLOW | os.O_CLOEXEC
        parts = _components(path)
        fd = os.dup(root)
        try:
            for part in parts[:-1]:
                child = os.open(part, flags | os.O_DIRECTORY, dir_fd=fd)
                os.close(fd)
                fd = child
            entry = os.open(parts[-1], flags, dir_fd=fd)
            try:
                info = os.fstat(entry)
                if not stat.S_ISREG(info.st_mode):
                    raise Denied("not a regular workspace file")
                if info.st_size > limit:
                    raise Limit("workspace file exceeds byte budget")
                # Use the already checked inode, never reopen the guest path.
                with open(f"/proc/self/fd/{entry}", "rb", buffering=0) as source:
                    data = bytearray()
                    while True:
                        if time.monotonic() >= self._deadline:
                            raise Limit("session deadline exceeded")
                        chunk = source.read(min(8192, limit + 1 - len(data)))
                        if not chunk:
                            return bytes(data)
                        data.extend(chunk)
                        if len(data) > limit:
                            raise Limit("workspace file exceeds byte budget")
            finally:
                os.close(entry)
        finally:
            os.close(fd)

    def _run(self, tool, limit):
        deadline = min(self._deadline, time.monotonic() + self._per_tool)
        command = [self._launcher, "--unshare-user", "--disable-userns", "--unshare-net",
                   "--unshare-pid", "--unshare-ipc", "--unshare-uts", "--cap-drop", "ALL",
                   "--die-with-parent", "--new-session", "--clearenv", "--ro-bind",
                   tool.image, "/", "--chdir", "/"]
        for k, v in sorted(tool.environment.items()):
            command.extend(("--setenv", k, v))
        command.extend(("--", *tool.argv))
        with subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, env={}, close_fds=True) as child:
            try:
                data = bytearray()
                with selectors.DefaultSelector() as ready:
                    ready.register(child.stdout, selectors.EVENT_READ)
                    while True:
                        remaining = deadline - time.monotonic()
                        if remaining <= 0 or not ready.select(remaining):
                            raise Limit("tool deadline exceeded")
                        chunk = os.read(child.stdout.fileno(), min(8192, limit + 1 - len(data)))
                        if not chunk:
                            break
                        data.extend(chunk)
                        if len(data) > limit:
                            raise Limit("tool output exceeds byte budget")
                if child.wait(timeout=max(0, deadline - time.monotonic())):
                    raise Denied("tool or isolation unavailable")
                return bytes(data)
            except subprocess.TimeoutExpired:
                raise Limit("tool deadline exceeded") from None
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait()
