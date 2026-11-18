#!/usr/bin/env python3
"""A narrow On1x LSP adapter. See the adjacent README for the trust boundary."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from typing import Optional
from urllib.parse import unquote, urlsplit

if os.name == "posix":
    import resource


MIB = 1024 * 1024
MAX_HEADER = 8192
MAX_MESSAGE = 16 * MIB  # A 2 MiB source can grow when JSON-escaped.
MAX_SOURCE = 2 * MIB
MAX_DOCUMENTS = 64
MAX_DOCUMENT_BYTES = 16 * MIB
MAX_CLI_OUTPUT = 4 * MIB  # Per stream.
CLI_TIMEOUT = 5
ENCODINGS = ("utf-16", "utf-8", "utf-32")
SEVERITIES = {"error": 1, "warning": 2, "note": 3}


class FrameError(Exception):
    pass


class RequestError(Exception):
    def __init__(self, code, message):
        self.code = code
        super().__init__(message)


class ToolError(Exception):
    pass


class StopServer(BaseException):
    pass


def read_frame(stream):
    headers = {}
    remaining = MAX_HEADER
    while True:
        line = stream.readline(remaining + 1)
        if not line and not headers:
            return None
        remaining -= len(line)
        if remaining < 0 or not line.endswith(b"\r\n"):
            raise FrameError("invalid or oversized message headers")
        if line == b"\r\n":
            break
        key, sep, value = line[:-2].partition(b":")
        key = key.lower()
        if not sep or not re.fullmatch(rb"[a-z0-9-]+", key) or key in headers:
            raise FrameError("invalid or duplicate message header")
        headers[key] = value.strip(b" \t")
    length = headers.get(b"content-length", b"")
    if not re.fullmatch(rb"[0-9]{1,8}", length) or int(length) > MAX_MESSAGE:
        raise FrameError("invalid or oversized Content-Length")
    count = int(length)
    body = stream.read(count)
    if len(body) != count:
        raise FrameError("incomplete message body")
    return body


def json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON member")
        result[key] = value
    return result


def invalid_constant(_):
    raise ValueError("invalid JSON constant")


def json_integer(value):
    if len(value) > 20:
        raise ValueError("JSON integer out of range")
    return int(value)


def parse_json(data):
    return json.loads(data.decode("utf-8"), object_pairs_hook=json_object,
                      parse_constant=invalid_constant, parse_int=json_integer)


def file_uri(uri):
    if (not isinstance(uri, str) or len(uri) > 16384
            or re.search(r"[\s\x00-\x1f\x7f?#]|%(?![a-fA-F0-9]{2})", uri)):
        return None
    try:
        parsed = urlsplit(uri)
        if parsed.scheme != "file" or parsed.netloc.lower() not in ("", "localhost"):
            return None
        path = unquote(parsed.path, errors="strict")
        path.encode("utf-8")
        if (not path.startswith("/") or path.startswith("//")
                or re.search(r"[\x00-\x1f\x7f]", path)):
            return None
        return Path(path)
    except (ValueError, UnicodeError):
        return None


def object_or_empty(value):
    return value if isinstance(value, dict) else {}


def source_bytes(text):
    if not isinstance(text, str) or len(text) > MAX_SOURCE:
        return None
    try:
        data = text.encode("utf-8")
    except UnicodeError:
        return None
    return data if len(data) <= MAX_SOURCE else None


def valid_version(version):
    return type(version) is int and -(2**31) <= version < 2**31


def normalized_eols(data):
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def position(data, offset, encoding):
    # The compiler counts LF and bytes. LSP also treats standalone CR as EOL,
    # and cannot address the gap between CR and LF in a CRLF pair.
    if 0 < offset < len(data) and data[offset - 1:offset + 1] == b"\r\n":
        offset -= 1
    line = (data.count(b"\n", 0, offset) + data.count(b"\r", 0, offset)
            - data.count(b"\r\n", 0, offset))
    start = 1 + max(data.rfind(b"\r", 0, offset), data.rfind(b"\n", 0, offset))
    prefix = data[start:offset]
    decoded = prefix.decode("utf-8")  # Reject non-character-boundary byte offsets.
    column = (len(prefix) if encoding == "utf-8"
              else len(decoded.encode("utf-16-le")) // 2 if encoding == "utf-16"
              else len(decoded))
    return {"line": line, "character": column}


def file_identity(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def read_saved(path):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, "rb") as stream:
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size > MAX_SOURCE:
            raise ToolError("expected a saved regular file no larger than 2 MiB")
        data = stream.read(MAX_SOURCE + 1)
        after = os.fstat(fd)
    if len(data) != before.st_size or file_identity(before) != file_identity(after):
        raise ToolError("saved file changed while reading")
    return data, file_identity(after)


def child_limits():
    # The server is single-threaded; preexec_fn is not safe in threaded code.
    for kind, cap in ((resource.RLIMIT_FSIZE, MAX_CLI_OUTPUT),
                      (resource.RLIMIT_CPU, CLI_TIMEOUT),
                      (resource.RLIMIT_AS, 1024 * MIB),
                      (resource.RLIMIT_CORE, 0)):
        soft, hard = resource.getrlimit(kind)
        limit = min(value for value in (soft, hard, cap) if value != resource.RLIM_INFINITY)
        resource.setrlimit(kind, (limit, limit))


def kill_child(child):
    try:
        os.killpg(child.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def run_cli(compiler, args, cwd):
    env = {"PATH": os.defpath, "LC_ALL": "C", "TMPDIR": str(cwd)}
    with tempfile.TemporaryFile(dir=cwd) as stdout, tempfile.TemporaryFile(dir=cwd) as stderr:
        try:
            with subprocess.Popen([str(compiler), *args], cwd=cwd, env=env,
                                  stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                                  start_new_session=True, preexec_fn=child_limits) as child:
                try:
                    code = child.wait(timeout=CLI_TIMEOUT)
                except subprocess.TimeoutExpired as exc:
                    kill_child(child)
                    raise ToolError(f"on1xc timed out after {CLI_TIMEOUT} seconds") from exc
                except StopServer:
                    kill_child(child)
                    raise
        except (OSError, subprocess.SubprocessError) as exc:
            raise ToolError(f"could not start on1xc: {exc}") from exc
        if any(os.fstat(stream.fileno()).st_size >= MAX_CLI_OUTPUT for stream in (stdout, stderr)):
            raise ToolError("on1xc exceeded the output limit")
        if code < 0:
            raise ToolError(f"on1xc terminated by signal {-code}")
        stdout.seek(0)
        stderr.seek(0)
        return code, stdout.read(MAX_CLI_OUTPUT), stderr.read(MAX_CLI_OUTPUT)


def concise(data):
    text = data[:512].decode("utf-8", errors="replace")
    return " ".join("".join(c if c.isprintable() else " " for c in text).split())


def compiler_diagnostics(data, path, source, encoding):
    try:
        report = parse_json(data)
    except (ValueError, RecursionError) as exc:
        raise ToolError("on1xc did not return diagnostic JSON") from exc
    report = object_or_empty(report)
    records, dropped = report.get("diagnostics"), report.get("dropped")
    if (report.get("schema") != "on1x.diagnostics/v1" or not isinstance(records, list)
            or len(records) > 1024 or type(dropped) is not int or dropped < 0):
        raise ToolError("unsupported on1xc diagnostic report")
    diagnostics, elsewhere, invalid = [], [], 0
    for item in records:
        item = object_or_empty(item)
        loc = object_or_empty(item.get("location"))
        file, offset = loc.get("file"), loc.get("offset")
        line, column = loc.get("line"), loc.get("byteColumn")
        message, severity = item.get("message"), item.get("severity")
        if (isinstance(file, str) and isinstance(message, str) and file != str(path)):
            elsewhere.append(f"{file}:{line}: {message}")
            continue
        if (file != str(path) or not isinstance(message, str) or not message
                or not isinstance(severity, str) or severity not in SEVERITIES
                or type(offset) is not int or not 0 <= offset <= len(source)
                or type(line) is not int or type(column) is not int
                or line != 1 + source.count(b"\n", 0, offset)
                or column != offset - source.rfind(b"\n", 0, offset)):
            invalid += 1
            continue
        try:
            point = position(source, offset, encoding)
        except UnicodeError:
            invalid += 1
            continue
        diagnostic = {"range": {"start": point, "end": point}, "source": "on1xc",
                      "message": message, "severity": SEVERITIES[severity]}
        if isinstance(item.get("id"), str) and item["id"]:
            diagnostic["code"] = item["id"]
        diagnostics.append(diagnostic)
    return diagnostics, elsewhere, invalid + dropped


@dataclass
class Document:
    path: Path
    version: int
    source: Optional[bytes]
    published: bool = False


class Server:
    def __init__(self, compiler, out, scratch):
        self.compiler = compiler
        self.out = out
        self.scratch = scratch
        self.state = "new"
        self.encoding = "utf-16"
        self.documents = {}
        self.roots = []

    def send(self, **message):
        body = json.dumps({"jsonrpc": "2.0", **message},
                          ensure_ascii=True, separators=(",", ":")).encode("ascii")
        self.out.write(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii"))
        self.out.write(body)
        self.out.flush()

    def log(self, message):
        self.send(method="window/logMessage", params={"type": 2, "message": message})

    def publish(self, uri, doc, diagnostics):
        self.send(method="textDocument/publishDiagnostics",
                  params={"uri": uri, "version": doc.version, "diagnostics": diagnostics})
        doc.published = bool(diagnostics)

    def compiler_for(self, doc):
        # A configured path containing "/" is an explicit choice by the launcher.
        if self.compiler and "/" in self.compiler:
            return Path(self.compiler).resolve(strict=True)
        name = self.compiler or "on1xc"
        parents = [Path(os.path.normpath(doc.path)).parent, doc.path.parent.resolve(),
                   doc.path.resolve().parent]
        roots = self.roots + parents
        cwd = Path.cwd().resolve()
        # Editors that omit rootUri commonly start the server at the project
        # root instead. Don't treat / or the user's home as a workspace.
        if cwd != Path(cwd.anchor) and cwd != Path.home() and any(cwd in p.parents for p in parents):
            roots.append(cwd)
        for folder in os.get_exec_path():
            base = Path(folder)
            if not base.is_absolute():
                continue
            base = Path(os.path.normpath(base))
            canonical = base.resolve()
            candidate = canonical / name
            resolved = candidate.resolve()
            if canonical == cwd or any(root == p or root in p.parents for root in roots
                                       for p in (base, candidate, resolved)):
                continue
            if resolved.is_file() and os.access(resolved, os.X_OK):
                return resolved
        raise ToolError(f"{name} not found on a trusted absolute PATH entry; use --on1xc PATH")

    def initialize(self, params):
        general = object_or_empty(object_or_empty(params.get("capabilities")).get("general"))
        wanted = general.get("positionEncodings")
        encoding = (next((e for e in wanted if e in ENCODINGS), "utf-16")
                    if isinstance(wanted, list) else "utf-16")
        folders = params.get("workspaceFolders")
        uris = [params.get("rootUri")]
        if isinstance(folders, list):
            if len(folders) > MAX_DOCUMENTS:
                raise RequestError(-32602, "too many workspace folders")
            uris.extend(object_or_empty(root).get("uri") for root in folders)
        paths = [path for uri in uris if (path := file_uri(uri)) is not None]
        root_path = params.get("rootPath")
        if (not params.get("rootUri") and isinstance(root_path, str)
                and root_path.startswith("/") and len(root_path) <= 16384):
            paths.append(Path(root_path))
        roots = [p for path in paths for p in (Path(os.path.normpath(path)), path.resolve())]
        self.roots, self.encoding = roots, encoding
        self.state = "ready"
        return {"capabilities": {
            "positionEncoding": self.encoding,
            "textDocumentSync": {"openClose": True, "change": 1, "save": {"includeText": False}},
            "documentFormattingProvider": True,
        }, "serverInfo": {"name": "on1x-lsp"}}

    def formatting(self, doc):
        if not doc or doc.source is None:
            raise RequestError(-32602, "formatting requires an open local On1x document within limits")
        try:
            compiler = self.compiler_for(doc)
            input_path = self.scratch / "buffer.on1x"
            input_path.write_bytes(doc.source)
            try:
                code, formatted, errors = run_cli(compiler, ["fmt", "--target", "plain", str(input_path)],
                                                  self.scratch)
            finally:
                input_path.unlink()
            if code:
                raise ToolError(f"on1xc fmt failed (exit {code}): {concise(errors)}")
            if len(formatted) > MAX_SOURCE or (doc.source.strip() and not formatted):
                raise ToolError("on1xc fmt returned missing or oversized output")
            if formatted == doc.source:
                return []
            end = position(doc.source, len(doc.source), self.encoding)
            return [{"range": {"start": {"line": 0, "character": 0}, "end": end},
                     "newText": formatted.decode("utf-8")}]
        except (ToolError, OSError, RuntimeError, UnicodeError) as exc:
            raise RequestError(-32803, str(exc)) from exc

    def save(self, uri, doc):
        diagnostics = []
        try:
            if doc.source is None:
                raise ToolError("document text exceeds the synchronization limits")
            path = doc.path.resolve(strict=True)
            before = read_saved(path)
            if before[0] != doc.source and normalized_eols(before[0]) != normalized_eols(doc.source):
                raise ToolError("saved file does not match the open document")
            compiler = self.compiler_for(doc)
            # --deps loads the real source graph, but disables native declaration
            # extension execution and exits before ordinary PPX/macro rewriting.
            code, _, errors = run_cli(compiler, ["--deps", "--diagnostics", "json",
                                                 "--color", "never", str(path)], self.scratch)
            if doc.path.resolve(strict=True) != path or read_saved(path) != before:
                raise ToolError("saved file changed during inspection")
            if code not in (0, 1) or (code == 1 and not errors):
                raise ToolError(f"on1xc --deps failed (exit {code}): {concise(errors)}")
            if errors:
                diagnostics, elsewhere, dropped = compiler_diagnostics(errors, path, before[0],
                                                                      self.encoding)
                for report in elsewhere[:3]:
                    self.log(concise(report.encode("utf-8", errors="replace")))
                if dropped or len(elsewhere) > 3:
                    self.log(f"on1xc findings omitted: {dropped + max(0, len(elsewhere) - 3)}")
        except (ToolError, OSError, RuntimeError, ValueError) as exc:
            self.log(f"On1x save inspection skipped: {exc}")
        self.publish(uri, doc, diagnostics)

    def document_event(self, method, params, uri, document_id, doc):
        if method == "textDocument/didOpen":
            path = file_uri(uri)
            version = document_id.get("version")
            if (doc or not path or path.suffix != ".on1x" or not valid_version(version)
                    or len(self.documents) >= MAX_DOCUMENTS):
                return
            doc = Document(path, version, None)
            self.documents[uri] = doc
            self.update_source(doc, document_id.get("text"))
        elif doc and method == "textDocument/didChange":
            version = document_id.get("version")
            if not valid_version(version) or version <= doc.version:
                return
            changes = params.get("contentChanges")
            doc.version = version
            if isinstance(changes, list) and changes:
                # Full replacements apply in order, so only the last text matters.
                change = object_or_empty(changes[-1])
                self.update_source(doc, change.get("text") if "range" not in change and
                                   "rangeLength" not in change else None)
            elif not isinstance(changes, list):
                self.update_source(doc, None)
            if doc.published:
                self.publish(uri, doc, [])
        elif doc and method == "textDocument/didSave":
            self.save(uri, doc)
        elif doc and method == "textDocument/didClose":
            if doc.published:
                self.publish(uri, doc, [])
            del self.documents[uri]

    def update_source(self, doc, text):
        data = source_bytes(text)
        doc.source = None
        held = sum(len(d.source) for d in self.documents.values() if d.source is not None)
        if data is not None and held + len(data) <= MAX_DOCUMENT_BYTES:
            doc.source = data

    def receive(self, body):
        try:
            message = parse_json(body)
        except (ValueError, RecursionError):
            self.send(id=None, error={"code": -32700, "message": "parse error"})
            return None
        message = object_or_empty(message)
        ident, method = message.get("id"), message.get("method")
        request = "id" in message
        if (message.get("jsonrpc") != "2.0" or not isinstance(method, str)
                or (request and type(ident) not in (int, str))):
            self.send(id=None, error={"code": -32600, "message": "invalid request"})
            return None
        if method == "exit" and not request:
            return 0 if self.state == "shutdown" else 1
        params = object_or_empty(message.get("params"))
        document_id = object_or_empty(params.get("textDocument"))
        uri = document_id.get("uri")
        doc = self.documents.get(uri) if isinstance(uri, str) else None
        if not request:
            if self.state == "ready":
                self.document_event(method, params, uri, document_id, doc)
            return None
        try:
            if self.state == "new":
                if method != "initialize":
                    raise RequestError(-32002, "server not initialized")
                try:
                    result = self.initialize(params)
                except (OSError, RuntimeError, ValueError) as exc:
                    raise RequestError(-32602, "invalid workspace root") from exc
            elif self.state == "shutdown" or method == "initialize":
                raise RequestError(-32600, "invalid request in current server state")
            elif method == "shutdown":
                self.state = "shutdown"
                result = None
            elif method == "textDocument/formatting":
                result = self.formatting(doc)
            else:
                raise RequestError(-32601, "method not found")
            self.send(id=ident, result=result)
        except RequestError as exc:
            self.send(id=ident, error={"code": exc.code, "message": str(exc)})
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--on1xc", metavar="PATH",
                        help="trusted compiler executable (default: on1xc on PATH)")
    args = parser.parse_args()
    if os.name != "posix":
        parser.error("this adapter requires POSIX process and resource limits")

    def stop(_signum, _frame):
        raise StopServer()

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    try:
        # Do not honor a workspace TMPDIR or ambient formatter configuration.
        with tempfile.TemporaryDirectory(prefix="on1x-lsp-", dir="/tmp") as temporary:
            scratch = Path(temporary)
            (scratch / ".on1xfmt").write_bytes(b"")
            server = Server(args.on1xc, sys.stdout.buffer, scratch)
            while (body := read_frame(sys.stdin.buffer)) is not None:
                status = server.receive(body)
                if status is not None:
                    return status
            return 0 if server.state == "shutdown" else 1
    except StopServer:
        return 1
    except (FrameError, OSError) as exc:
        print(f"on1x-lsp: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
