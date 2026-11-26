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
from dataclasses import dataclass, field
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
MAX_MANIFEST = 64 * 1024  # on1x-project.py's plain manifest limit.
MANIFESTS = ("On1xfile", "On1xfile.m", "On1xfile.i")
QMR_TAG = re.compile(rb"\{[{%#]")
MAX_CODE_ACTIONS = 256
MAX_CODE_ACTION_BYTES = 2 * MIB
MAX_OPERATOR_RECORDS = 32768
CLI_TIMEOUT = 5
ENCODINGS = ("utf-16", "utf-8", "utf-32")
SEVERITIES = {"error": 1, "warning": 2, "note": 3}
OPERATOR_GLYPHS = b"!%&*+?^|~<>=$.-"
OPERATOR_KEYWORD = b"operator"


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


def source_offset(data, point, encoding):
    """Convert a client position to a byte offset in the saved UTF-8 source."""
    point = object_or_empty(point)
    line, character = point.get("line"), point.get("character")
    if (type(line) is not int or type(character) is not int
            or not 0 <= line <= len(data) or not 0 <= character <= len(data)):
        return None
    start, end = 0, len(data)
    for eol in re.finditer(rb"\r\n|[\r\n]", data):
        if not line:
            end = eol.start()
            break
        start = eol.end()
        line -= 1
    else:
        if line:
            return None
    raw = data[start:end]
    if encoding == "utf-8":
        if character > len(raw):
            return None
        raw[:character].decode("utf-8")
        return start + character
    text = raw.decode("utf-8")
    if encoding == "utf-16":
        wide = text.encode("utf-16-le")
        if character * 2 > len(wide):
            return None
        prefix = wide[:character * 2].decode("utf-16-le")
    else:
        if character > len(text):
            return None
        prefix = text[:character]
    return start + len(prefix.encode("utf-8"))


def operator_range(source, item, length, encoding):
    offset, line, column = item.get("offset"), item.get("line"), item.get("column")
    if (type(offset) is not int or type(length) is not int
            or not 0 <= offset < offset + length <= len(source) or length > 128
            or type(line) is not int or type(column) is not int
            or line != 1 + source.count(b"\n", 0, offset)
            or column != offset - source.rfind(b"\n", 0, offset)
            or b"\r" in source[offset:offset + length] or b"\n" in source[offset:offset + length]):
        return None
    return {"start": position(source, offset, encoding),
            "end": position(source, offset + length, encoding)}


def operator_report(data, path, source, offset, encoding):
    report = object_or_empty(parse_json(data))
    declarations, uses = report.get("declarations"), report.get("uses")
    if (type(report.get("version")) is not int or report["version"] != 1
            or not isinstance(declarations, list) or len(declarations) > MAX_OPERATOR_RECORDS
            or not isinstance(uses, list) or len(uses) > MAX_OPERATOR_RECORDS):
        return None
    matches = []
    for record in uses:
        item = object_or_empty(record)
        start, length = item.get("offset"), item.get("length")
        if (item.get("file") == str(path) and type(start) is int and type(length) is int
                and start <= offset < start + length):
            matches.append((item.get("declaration"), item, length))
    if not matches:
        for index, record in enumerate(declarations):
            item = object_or_empty(record)
            start = item.get("offset")
            if (item.get("file") == str(path) and type(start) is int
                    and start <= offset < start + len(OPERATOR_KEYWORD)
                    and source[start:start + len(OPERATOR_KEYWORD)] == OPERATOR_KEYWORD):
                matches.append((index, item, len(OPERATOR_KEYWORD)))
    if len(matches) != 1:
        return None
    index, item, length = matches[0]
    if type(index) is not int or not 0 <= index < len(declarations):
        return None
    span = operator_range(source, item, length, encoding)
    if span is None:
        return None
    return object_or_empty(declarations[index]), span


def describe_operator(declaration):
    lexeme, lexeme2 = declaration.get("lexeme"), declaration.get("lexeme2")
    arity, fixity, binds = (declaration.get(key) for key in ("arity", "position", "binds"))
    precedence, tightness = (declaration.get(key) for key in ("precedence", "tightness"))
    signature = declaration.get("signature")
    if (type(arity) is not int or arity not in (1, 2, 3)
            or fixity not in ("prefix", "postfix", "infix") or binds not in ("left", "right")
            or any(type(rank) is not int or not 0 <= rank <= 9 for rank in (precedence, tightness))
            or (signature is not None and (not isinstance(signature, str) or len(signature) > 4096))
            or not isinstance(lexeme, str) or not re.fullmatch(r"[!%&*+?^|~<>=$.-]{2,32}", lexeme)
            or not isinstance(lexeme2, str)
            or (arity == 3 and not re.fullmatch(r"[!%&*+?^|~<>=$.-]{2,32}", lexeme2))
            or (arity != 3 and lexeme2)):
        return None
    spelling = f'"{lexeme}" "{lexeme2}"' if lexeme2 else f'"{lexeme}"'
    kind = ("unary", "binary", "ternary")[arity - 1]
    prototype = concise(signature.encode("utf-8", errors="replace")) + "\n" if signature else ""
    return (f"operator {spelling} ({kind} {fixity})\n"
            f"{prototype}"
            f"Declared precedence {precedence}, tightness {tightness}, {binds}-associative")


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


def run_cli(compiler, args, cwd, foreignlibs=()):
    env = {"PATH": os.defpath, "LC_ALL": "C", "TMPDIR": str(cwd)}
    if foreignlibs:
        env["MULIB_FOREIGNLIB_PATH"] = os.pathsep.join(map(str, foreignlibs))
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


def manifest_diagnostic(data, path, source, encoding):
    # The plain manifest driver reports one error and an optional line span.
    # Never attach tool startup errors or locations for a different file.
    text = data[:4096].decode("utf-8", errors="replace").split("\n", 1)[0]
    match = re.fullmatch(r"on1x-project: " + re.escape(str(path))
                         + r"(?::([0-9]{1,6})(?:-([0-9]{1,6}))?)?: (.+)", text)
    if not match:
        raise ToolError(f"on1xc aux format failed: {concise(data)}")
    start, end, message = match.groups()
    begin, finish = int(start or "1"), int(end or start or "1")
    breaks = [m.end() for m in re.finditer(b"\n", source)]
    offsets = [0, *breaks]
    if not 1 <= begin <= finish <= len(offsets):
        raise ToolError("project driver reported an invalid manifest line")
    last = source.find(b"\n", offsets[finish - 1])
    last = len(source) if last < 0 else last
    if last and source[last - 1:last] == b"\r":
        last -= 1
    return {"range": {"start": position(source, offsets[begin - 1], encoding),
                      "end": position(source, last, encoding)},
            "source": "on1x-project", "severity": 1, "code": "ON1XFILE",
            "message": concise(message.encode("utf-8"))}


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
    diagnostics, actions, elsewhere, invalid = [], [], [], 0
    action_bytes = 0
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
        if item.get("category") == "operator" and diagnostic.get("code", "").startswith("ON1X-OPERATOR-"):
            # Operator errors have no automatic repair: surface the catalog's
            # bounded remediation without inventing a source span or edit.
            remediation = item.get("remediation")
            if isinstance(remediation, str):
                hint = concise(remediation[:512].encode("utf-8", errors="replace"))
                if hint:
                    diagnostic["message"] += "\n" + hint
        kernel_code = message.partition(":")[0]
        if (kernel_code in ("ON1X-KERNEL-NOT-ADMITTED", "ON1X-KERNEL-UNSAFE-UNSUPPORTED")
                and item.get("id") in (kernel_code, "ON1X-SOURCE")
                and message.endswith("; use --kernel-imports to inspect source requests")):
            # --deps has no KernelBind authority. Preserve other compiler
            # policy diagnostics with the same IDs (e.g. unsafe-only modules).
            diagnostic["severity"] = 3
            diagnostic["code"] = kernel_code
            diagnostic["message"] = ("KernelBind admission was not checked by the LSP. "
                                     "Use the project's KernelBind build workflow for validation.")
        action_ids = []
        repairs = item.get("repairs", [])
        if isinstance(repairs, list) and len(repairs) <= 64:
            for repair in repairs:
                repair = object_or_empty(repair)
                identity = repair.get("id")
                status = repair.get("status")
                edits = repair.get("edits")
                try:
                    identity_bytes = identity.encode("utf-8")
                except (AttributeError, UnicodeError):
                    identity_bytes = b""
                if (not identity_bytes
                        or len(identity_bytes) > 256
                        or re.search(r"[\x00-\x1f\x7f]", identity)
                        or status not in ("candidate", "validated")
                        or not isinstance(edits, list) or not edits
                        or len(edits) > 64
                        or len(actions) >= MAX_CODE_ACTIONS):
                    invalid += 1
                    continue
                converted, cursor, previous = [], 0, None
                output_size, repair_bytes = len(source), 0
                valid = True
                for edit in edits:
                    edit = object_or_empty(edit)
                    begin, end = edit.get("begin"), edit.get("end")
                    replacement = edit.get("replacement")
                    try:
                        replacement_bytes = replacement.encode("utf-8")
                    except (AttributeError, UnicodeError):
                        valid = False
                        break
                    if (type(begin) is not int or type(end) is not int
                            or not 0 <= begin <= end <= len(source)
                            or begin < cursor or begin == previous
                            or len(replacement_bytes) > MAX_SOURCE):
                        valid = False
                        break
                    output_size -= end - begin
                    output_size += len(replacement_bytes)
                    if (output_size > MAX_SOURCE
                            or action_bytes + repair_bytes + len(replacement_bytes)
                            > MAX_CODE_ACTION_BYTES):
                        valid = False
                        break
                    try:
                        start = position(source, begin, encoding)
                        finish = position(source, end, encoding)
                    except UnicodeError:
                        valid = False
                        break
                    converted.append({
                        "range": {"start": start, "end": finish},
                        "newText": replacement,
                    })
                    repair_bytes += len(replacement_bytes)
                    cursor, previous = end, begin
                if not valid:
                    invalid += 1
                    continue
                action_bytes += repair_bytes
                action_ids.append(len(actions))
                actions.append({
                    "id": identity,
                    "status": status,
                    "edits": converted,
                    "diagnostic": diagnostic,
                })
        elif repairs:
            invalid += 1
        if action_ids:
            diagnostic["data"] = {"on1xRepairActions": action_ids}
        diagnostics.append(diagnostic)
    return diagnostics, actions, elsewhere, invalid + dropped


@dataclass
class Document:
    path: Path
    version: int
    source: Optional[bytes]
    published: bool = False
    actions: list = field(default_factory=list)


class Server:
    def __init__(self, compiler, out, scratch, foreignlibs=()):
        self.compiler = compiler
        self.out = out
        self.scratch = scratch
        self.state = "new"
        self.encoding = "utf-16"
        self.documents = {}
        self.roots = []
        self.foreignlibs = foreignlibs

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
            "hoverProvider": True,
            "definitionProvider": True,
            "codeActionProvider": {
                "codeActionKinds": ["quickfix"],
                "resolveProvider": False,
            },
        }, "serverInfo": {"name": "on1x-lsp"}}

    def format_copy(self, compiler, name, source):
        manifest = name in MANIFESTS
        if manifest and len(source) > MAX_MANIFEST:
            raise ToolError("expected a manifest no larger than 64 KiB")
        input_path = self.scratch / (name if manifest else "buffer.on1x")
        input_path.write_bytes(source)
        try:
            args = ["aux", "format"] if manifest else ["fmt", "--isolated", "--target", "plain"]
            return input_path, *run_cli(compiler, [*args, str(input_path)], self.scratch)
        finally:
            input_path.unlink()

    def formatting(self, doc):
        if not doc or doc.source is None:
            raise RequestError(-32602, "formatting requires an open local On1x document within limits")
        try:
            compiler = self.compiler_for(doc)
            input_path, code, formatted, errors = self.format_copy(compiler, doc.path.name, doc.source)
            if code:
                if doc.path.name in MANIFESTS:
                    raise ToolError(manifest_diagnostic(errors, input_path, doc.source, self.encoding)["message"])
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
        actions = []
        try:
            if doc.source is None:
                raise ToolError("document text exceeds the synchronization limits")
            path = doc.path.resolve(strict=True)
            before = read_saved(path)
            if before[0] != doc.source and normalized_eols(before[0]) != normalized_eols(doc.source):
                raise ToolError("saved file does not match the open document")
            compiler = self.compiler_for(doc)
            manifest = doc.path.name in MANIFESTS
            manifest_path = self.scratch / doc.path.name
            if manifest:
                if doc.path.name != "On1xfile" and QMR_TAG.search(before[0]):
                    raise ToolError("QMR manifests are not inspected or executed")
                manifest_path, code, _, errors = self.format_copy(compiler, doc.path.name, before[0])
            else:
                # --deps loads the real source graph, but disables native declaration
                # extension execution and exits before ordinary PPX/macro rewriting.
                code, _, errors = run_cli(compiler, ["--deps", "--diagnostics", "json",
                                                     "--color", "never", str(path)], self.scratch,
                                          self.foreignlibs)
            if doc.path.resolve(strict=True) != path or read_saved(path) != before:
                raise ToolError("saved file changed during inspection")
            if manifest:
                if code:
                    diagnostics = [manifest_diagnostic(errors, manifest_path, before[0], self.encoding)]
            elif code not in (0, 1) or (code == 1 and not errors):
                raise ToolError(f"on1xc --deps failed (exit {code}): {concise(errors)}")
            elif errors:
                diagnostics, actions, elsewhere, dropped = compiler_diagnostics(
                    errors, path, before[0], self.encoding)
                for report in elsewhere[:3]:
                    self.log(concise(report.encode("utf-8", errors="replace")))
                if dropped or len(elsewhere) > 3:
                    self.log(f"on1xc findings omitted: {dropped + max(0, len(elsewhere) - 3)}")
        except (ToolError, OSError, RuntimeError, ValueError) as exc:
            self.log(f"On1x save inspection skipped: {exc}")
        doc.actions = actions
        self.publish(uri, doc, diagnostics)

    def operator_navigation(self, method, uri, doc, params):
        if not doc or doc.source is None or doc.path.suffix != ".on1x":
            return None
        try:
            path = doc.path.resolve(strict=True)
            before = read_saved(path)
            if normalized_eols(before[0]) != normalized_eols(doc.source):
                return None
            offset = source_offset(before[0], params.get("position"), self.encoding)
            if offset is None or offset == len(before[0]):
                return None
            window = before[0][max(0, offset - 7):offset + 8]
            if before[0][offset] not in OPERATOR_GLYPHS and OPERATOR_KEYWORD not in window:
                return None
            compiler = self.compiler_for(doc)
            code, output, _ = run_cli(compiler, ["--operator-info", "--source-only", str(path)],
                                      self.scratch, self.foreignlibs)
            if code or doc.path.resolve(strict=True) != path or read_saved(path) != before:
                return None
            found = operator_report(output, path, before[0], offset, self.encoding)
            if not found:
                return None
            declaration, use_span = found
            summary = describe_operator(declaration)
            if not summary:
                return None
            if method == "textDocument/hover":
                return {"contents": {"kind": "plaintext", "value": summary}, "range": use_span}
            filename = declaration.get("file")
            if (not isinstance(filename, str) or not filename.startswith("/")
                    or len(filename) > 16384 or re.search(r"[\x00-\x1f\x7f]", filename)):
                return None
            target = Path(filename)
            if target.suffix != ".on1x" or target.resolve(strict=True) != target:
                return None
            if target == path:
                target_uri, data = uri, before[0]
            else:
                target_uri = target.as_uri()
                data, _ = read_saved(target)
                for opened in self.documents.values():
                    if opened.path != target and opened.path.resolve() != target:
                        continue
                    if opened.source is None or normalized_eols(opened.source) != normalized_eols(data):
                        return None
            start = declaration.get("offset")
            length = len(OPERATOR_KEYWORD)
            if type(start) is not int or data[start:start + length] != OPERATOR_KEYWORD:
                return None
            target_span = operator_range(data, declaration, length, self.encoding)
            return {"uri": target_uri, "range": target_span} if target_span else None
        except (ToolError, OSError, RuntimeError, ValueError):
            # No speculative index from unsaved or unchecked source.
            return None

    def code_actions(self, uri, doc, params):
        if not doc or doc.source is None:
            raise RequestError(
                -32602,
                "code actions require an open local On1x document within limits",
            )
        context = object_or_empty(params.get("context"))
        only = context.get("only")
        if isinstance(only, list) and not any(
                isinstance(kind, str)
                and (kind == "quickfix" or kind.startswith("quickfix."))
                for kind in only):
            return []
        result = []
        for action in doc.actions:
            item = {
                "title": f"On1x repair proposal: {action['id']}",
                "kind": "quickfix",
                "diagnostics": [action["diagnostic"]],
                "data": {"on1xRepairStatus": action["status"]},
            }
            if action["status"] == "validated":
                item["edit"] = {"documentChanges": [{
                    "textDocument": {"uri": uri, "version": doc.version},
                    "edits": action["edits"],
                }]}
                item["isPreferred"] = True
            else:
                item["disabled"] = {
                    "reason": (
                        "compiler reported an unverified candidate; run "
                        "explicit repair validation"
                    )
                }
            result.append(item)
        return result

    def document_event(self, method, params, uri, document_id, doc):
        if method == "textDocument/didOpen":
            path = file_uri(uri)
            version = document_id.get("version")
            if (doc or not path or (path.suffix != ".on1x" and path.name not in MANIFESTS)
                    or not valid_version(version)
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
            doc.actions = []
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
            elif method == "textDocument/codeAction":
                result = self.code_actions(uri, doc, params)
            elif method in ("textDocument/hover", "textDocument/definition"):
                result = self.operator_navigation(method, uri, doc, params)
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
    parser.add_argument("--foreignlib-path", metavar="PATH", action="append", type=Path, default=[],
                        help="trusted absolute generated stdffi recipe directory (repeatable)")
    args = parser.parse_args()
    if os.name != "posix":
        parser.error("this adapter requires POSIX process and resource limits")
    if len(args.foreignlib_path) > 16 or any(not p.is_absolute() for p in args.foreignlib_path):
        parser.error("--foreignlib-path requires up to 16 existing absolute directories without ':'")
    try:
        foreignlibs = [p.resolve(strict=True) for p in args.foreignlib_path]
    except (OSError, RuntimeError, ValueError):
        parser.error("cannot resolve configured --foreignlib-path directories")
    if any(os.pathsep in str(p) or not p.is_dir() for p in foreignlibs):
        parser.error("--foreignlib-path directories must not contain ':'")

    def stop(_signum, _frame):
        raise StopServer()

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    try:
        # Do not honor a workspace TMPDIR or ambient formatter configuration.
        with tempfile.TemporaryDirectory(prefix="on1x-lsp-", dir="/tmp") as temporary:
            scratch = Path(temporary)
            (scratch / ".on1xfmt").write_bytes(b"")
            server = Server(args.on1xc, sys.stdout.buffer, scratch, foreignlibs)
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
