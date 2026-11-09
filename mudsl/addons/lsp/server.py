#!/usr/bin/env python3
"""MuLL LSP over stdio, plus supplemental MuDSL/MuLIB API lookup (Python 3.9+)."""
import argparse
import bisect
import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import unquote, urlparse

from api import KEYWORDS, MEMBERS, TOOLKIT

MAX_SOURCE = 4 * 1024 * 1024
TOKEN = re.compile(
    r'(?P<space>[ \t\r\n]+)|(?P<comment>\#[^\n]*|//[^\n]*)|'
    r'(?P<string>"(?:\\[\s\S]|[^"\\])*(?:"|\\?$))|'
    r'(?P<name>[A-Za-z_][A-Za-z_0-9]*)|'
    r'(?P<number>[0-9]+(?:\.[0-9]*)?(?:[eE][+-]?[0-9]*)?)|'
    r'(?P<punct><-|==|!=|<=|>=|[+*/%<>(){}\[\]:;,!\-])|(?P<invalid>[\s\S])'
)


def uri_path(uri):
    parsed = urlparse(uri)
    if parsed.scheme != "file" or parsed.netloc not in ("", "localhost"):
        return None
    path = unquote(parsed.path)
    if sys.platform == "win32" and re.match(r"/[A-Za-z]:", path):
        path = path[1:]
    return Path(path)


@dataclass
class Token:
    kind: str
    text: str
    start: int
    end: int
    scope: tuple = ()


@dataclass
class Binding:
    token: Token
    kind: int
    scope: tuple
    package: str = ""


class Document:
    def __init__(self, uri, text, language="mull", version=0):
        self.uri, self.text, self.language, self.version = uri, text, language, version
        self.lines = [0] + [m.end() for m in re.finditer("\n", text)]
        self.tokens = []
        self.bindings = []
        self.issues = []
        self.scopes = [(0, len(text), ())]
        self.too_large = len(text.encode("utf-8")) > MAX_SOURCE
        if language == "mull" and not self.too_large:
            self.scan()

    def position(self, offset):
        offset = max(0, min(offset, len(self.text)))
        line = bisect.bisect_right(self.lines, offset) - 1
        units = len(self.text[self.lines[line]:offset].encode("utf-16-le")) // 2
        return {"line": line, "character": units}

    def offset(self, position):
        line = max(0, min(position["line"], len(self.lines) - 1))
        start = self.lines[line]
        end = self.lines[line + 1] if line + 1 < len(self.lines) else len(self.text)
        units = max(0, position["character"])
        # LSP defaults to UTF-16, including when a Python string uses one code point.
        for index in range(start, end):
            if units <= 0 or self.text[index] in "\r\n":
                return index
            width = 2 if ord(self.text[index]) > 0xffff else 1
            if units < width:
                return index
            units -= width
        return end

    def range(self, start, end):
        return {"start": self.position(start), "end": self.position(end)}

    def diagnostic(self, start, end, message):
        return {"range": self.range(start, end), "severity": 1, "source": "MuLL", "message": message}

    def scan(self):
        stack, scopes = [], []
        for match in TOKEN.finditer(self.text):
            kind, value = match.lastgroup, match.group()
            token = Token(kind, value, match.start(), match.end(), tuple(scopes))
            if kind == "space":
                continue
            self.tokens.append(token)
            if kind == "string":
                # The final quote may itself be escaped.
                closed = re.fullmatch(r'"(?:\\[\s\S]|[^"\\])*"', value) is not None
                if not closed:
                    self.issues.append(self.diagnostic(token.start, token.end, "unterminated string"))
                for escape in re.finditer(r"\\([\s\S])", value):
                    if escape[1] not in 'nrt0\\"':
                        self.issues.append(self.diagnostic(token.start + escape.start(), token.start + escape.end(), "invalid string escape"))
            elif kind == "invalid":
                self.issues.append(self.diagnostic(token.start, token.end, "unexpected character"))
            elif kind == "number" and re.search(r"[eE][+-]?$", value):
                self.issues.append(self.diagnostic(token.start, token.end, "invalid exponent"))
            elif kind == "punct":
                if value in "({[":
                    stack.append(token)
                    if value == "{":
                        scopes.append(token.start)
                elif value in ")}]":
                    if not stack or stack[-1].text != {")": "(", "}": "{", "]": "["}[value]:
                        self.issues.append(self.diagnostic(token.start, token.end, "unmatched '" + value + "'"))
                    else:
                        opened = stack.pop()
                        if value == "}":
                            self.scopes.append((opened.start, token.end, tuple(scopes)))
                            scopes.pop()
        for opened in stack:
            self.issues.append(self.diagnostic(opened.start, opened.end, "unclosed '" + opened.text + "'"))
            if opened.text == "{":
                self.scopes.append((opened.start, len(self.text), opened.scope + (opened.start,)))
        code = [t for t in self.tokens if t.kind != "comment"]
        for index, token in enumerate(code):
            if token.kind != "name":
                continue
            if token.text in ("val", "var", "ref", "mulib") and index + 1 < len(code):
                name = code[index + 1]
                if name.kind != "name" or name.text in KEYWORDS:
                    continue
                tail = code[index + 2:index + 9]
                kind = 12 if len(tail) > 1 and tail[1].text == "fn" else (14 if token.text in ("val", "mulib") else 13)
                package = ""
                if len(tail) >= 7 and [t.text for t in tail[:3]] == ["<-", "G", ":"] and tail[3].text in ("LoadLib", "MulibWrap") and tail[4].text == "(" and tail[5].kind == "string" and tail[6].text == ")":
                    package = ("native:" if tail[3].text == "MulibWrap" else "") + tail[5].text[1:-1]
                self.bindings.append(Binding(name, kind, token.scope, package))
            elif token.text == "fn" and index + 1 < len(code) and code[index + 1].text == "(":
                params, cursor = [], index + 2
                while cursor < len(code) and code[cursor].text not in (")", "{", ";"):
                    if code[cursor].kind == "name" and code[cursor].text not in KEYWORDS:
                        params.append(code[cursor])
                    cursor += 1
                if cursor + 1 < len(code) and code[cursor].text == ")" and code[cursor + 1].text == "{":
                    scope = code[cursor + 1].scope + (code[cursor + 1].start,)
                    self.bindings.extend(Binding(param, 13, scope) for param in params)

    def scope_at(self, offset):
        return max((scope for start, end, scope in self.scopes if start < offset <= end), key=len, default=())

    def visible(self, offset):
        scope = self.scope_at(offset)
        found = {}
        for binding in sorted(self.bindings, key=lambda b: (len(b.scope), b.token.start)):
            if binding.token.start <= offset and scope[:len(binding.scope)] == binding.scope:
                found[binding.token.text] = binding
        return found

    def token_at(self, offset):
        return next((t for t in self.tokens if t.start <= offset < t.end), None)

    def word_at(self, offset):
        if self.language == "mull":
            token = self.token_at(offset)
            return token if token and token.kind == "name" else None
        for match in re.finditer(r"[A-Za-z_][A-Za-z_0-9]*", self.text):
            if match.start() <= offset < match.end():
                return Token("name", match.group(), match.start(), match.end())
        return None


class Server:
    def __init__(self, output, root=None, checker=None, package_paths=()):
        self.output = output
        self.documents = {}
        self.root = None
        self.mudsl = None
        self.modules = {}
        self.native = {}
        self.checker = checker or shutil.which("mull-check")
        self.package_paths = [Path(p).expanduser().resolve() for p in package_paths]
        self.stopped = False
        self.initialized = False
        self.configure_root(root)

    def configure_root(self, root):
        candidates = [Path(root).expanduser().resolve()] if root else list(Path(__file__).resolve().parents)
        for candidate in candidates:
            if (candidate / "MuDSL.hpp").is_file():
                self.mudsl = candidate
                self.root = candidate.parent if (candidate.parent / "include").is_dir() else None
                break
            if (candidate / "mudsl/MuDSL.hpp").is_file():
                self.root, self.mudsl = candidate, candidate / "mudsl"
                break
        inventories = [Path(__file__).with_name("Inventory.json")]
        if self.mudsl:
            inventories.insert(0, self.mudsl / "mull/bridge/Mulib/Inventory.json")
        for path in inventories:
            if path.is_file():
                self.modules = json.loads(path.read_text(encoding="utf-8"))["modules"]
                break
        self.native = {name: module["header"] for module in self.modules.values() for name in module["functions"]}

    def send(self, message):
        data = json.dumps({"jsonrpc": "2.0", **message}, ensure_ascii=False).encode("utf-8")
        self.output.write(f"Content-Length: {len(data)}\r\n\r\n".encode("ascii") + data)
        self.output.flush()

    def notify(self, method, params):
        self.send({"method": method, "params": params})

    def diagnostics(self, document):
        if document.language != "mull":
            return
        issues = document.issues[:32]
        if self.checker:
            try:
                result = subprocess.run([self.checker], input=document.text.encode("utf-8"), capture_output=True, timeout=5, check=True)
                error = json.loads(result.stdout)
                issues = []
                if error:
                    line = max(0, min(error["line"] - 1, len(document.lines) - 1))
                    start = document.lines[line]
                    end = document.lines[line + 1] if line + 1 < len(document.lines) else len(document.text)
                    prefix = document.text[start:end].encode("utf-8")[:max(0, error["column"] - 1)]
                    offset = start + len(prefix.decode("utf-8", errors="ignore"))
                    issues = [document.diagnostic(offset, min(offset + 1, len(document.text)), error["message"])]
            except (OSError, subprocess.SubprocessError, ValueError, KeyError, TypeError) as error:
                self.notify("window/logMessage", {"type": 2, "message": f"MuLL checker unavailable; using lexical diagnostics: {error}"})
        self.notify("textDocument/publishDiagnostics", {"uri": document.uri, "version": document.version, "diagnostics": issues})

    def module(self, name):
        name = name.lower()
        for key, module in self.modules.items():
            if name in (key.lower(), module["header"].lower()):
                return module
        folded = re.sub(r"[^a-z0-9]", "", name)
        matches = [m for k, m in self.modules.items() if re.sub(r"[^a-z0-9]", "", k.lower()) == folded]
        return matches[0] if len(matches) == 1 else None

    def members(self, document, receiver, offset):
        binding = document.visible(offset).get(receiver)
        package = binding.package if binding else ("G" if receiver == "G" else "")
        if package.startswith("native:"):
            module = self.module(package[7:])
            members = {k: v for k, v in MEMBERS["Mulib"].items() if k != "Wrap"}
            if module:
                members.update({name: (name, "Native C function from " + module["header"] + ". See the module manual for its argument and ownership contract.") for name in module["functions"]})
            return members
        return MEMBERS.get(package, {})

    @staticmethod
    def item(name, kind, detail="", documentation=""):
        return {"label": name, "kind": kind, "detail": detail, "documentation": {"kind": "markdown", "value": documentation}}

    def completion(self, document, offset):
        before = document.text[:offset]

        def edits(items, prefix):
            span = document.range(offset - len(prefix), offset)
            return [{**item, "textEdit": {"range": span, "newText": item["label"]}} for item in items]

        if document.language == "mull":
            token = document.token_at(max(0, offset - 1))
            if token and token.kind == "comment":
                return []
            package = re.search(r'\bG\s*:\s*(LoadLib|MulibWrap)\s*\(\s*"([^"\\]*)$', before)
            if package:
                names = self.modules if package[1] == "MulibWrap" else set(MEMBERS) - {"G"}
                if package[1] == "LoadLib":
                    names = set(names)
                    for root in self.package_roots():
                        names.update(p.parent.name for p in root.glob("*/Package.mull"))
                return edits([self.item(n, 9) for n in sorted(names) if n.lower().startswith(package[2].lower())], package[2])
            if token and token.kind == "string":
                return []
            member = re.search(r"\b([A-Za-z_]\w*)\s*:\s*([A-Za-z_0-9]*)$", before, re.ASCII)
            if member:
                return edits([self.item(n, 3 if "(" in sig or n.startswith("mulib_") else 21, sig, doc)
                              for n, (sig, doc) in self.members(document, member[1], offset).items() if n.startswith(member[2])], member[2])
            prefix = re.search(r"[A-Za-z_0-9]*$", before)[0]
            items = {name: self.item(name, 14, documentation=doc) for name, doc in KEYWORDS.items()}
            items["G"] = self.item("G", 6, "Host services")
            items.update({name: self.item(name, 6, "Local binding") for name in document.visible(offset)})
            return edits([item for name, item in items.items() if name.startswith(prefix)], prefix)
        include = re.search(r'#\s*include\s*[<"]([^>"\n]*)$', before)
        if include:
            headers = sorted({m["header"] for m in self.modules.values()} | {"MuDSL.hpp"})
            return edits([self.item(n, 17) for n in headers if n.startswith(include[1])], include[1])
        prefix = re.search(r"[A-Za-z_0-9]*$", before)[0]
        if prefix.startswith(("mulib_", "MULIB_")):
            return edits([self.item(n, 3, header) for n, header in self.native.items() if n.startswith(prefix)], prefix)
        if re.search(r"\b(?:dsl|mudsl|mull)::[A-Za-z_0-9]*$", before) or (document.language == "d" and prefix):
            return edits([self.item(n, 7, sig, doc) for n, (sig, doc, _) in TOOLKIT.items() if n.startswith(prefix)], prefix)
        return []

    def hover(self, document, offset):
        token = document.word_at(offset)
        if not token:
            return None
        name, description = token.text, ""
        if document.language == "mull":
            member = re.search(r"\b([A-Za-z_]\w*)\s*:\s*$", document.text[:token.start], re.ASCII)
            if member:
                entry = self.members(document, member[1], offset).get(name)
                if entry:
                    description = f"`{entry[0]}`\n\n{entry[1]}"
            elif name in KEYWORDS:
                description = KEYWORDS[name]
            elif name == "G":
                description = "MuLL host services: " + ", ".join(f"`{n}`" for n in MEMBERS["G"])
            elif name in document.visible(offset):
                binding = document.visible(offset)[name]
                description = f"`{name}` — local binding" + (f" from `{binding.package}`" if binding.package else "")
        elif name in TOOLKIT:
            signature, doc, manual = TOOLKIT[name]
            description = f"`{signature}` — {doc}."
            if self.mudsl:
                description += "\n\n[MuDSL reference](" + (self.mudsl / "manual" / manual).resolve().as_uri() + ")"
        if name in self.native and (document.language != "mull" or description):
            header = self.native[name]
            description += f"\n\n`{name}` — `{header}`."
            if self.root:
                manual = self.root / "manual" / (Path(header).stem + ".md")
                if manual.is_file():
                    description += "\n\n[MuLIB module manual](" + manual.as_uri() + ")"
        return {"contents": {"kind": "markdown", "value": description.strip()}, "range": document.range(token.start, token.end)} if description else None

    def package_roots(self):
        return self.package_paths + ([self.mudsl / "mull/stdlib"] if self.mudsl else [])

    def definition(self, document, offset):
        token = document.token_at(offset)
        if token and token.kind == "string":
            before = document.text[:token.start]
            name = token.text[1:-1]
            if re.search(r"\bG\s*:\s*LoadLib\s*\(\s*$", before) and re.fullmatch(r"[A-Za-z_0-9-]+", name):
                for root in self.package_roots():
                    path = root / name / "Package.mull"
                    if path.is_file():
                        return {"uri": path.as_uri(), "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}}}
        token = document.word_at(offset)
        if not token:
            return None
        member = re.search(r":\s*$", document.text[:token.start])
        if document.language == "mull" and not member:
            binding = document.visible(offset).get(token.text)
            # Parameter names are outside their body's scope in the signature.
            binding = binding or next((b for b in document.bindings if b.token.start == token.start), None)
            if binding:
                return {"uri": document.uri, "range": document.range(binding.token.start, binding.token.end)}
        if self.root and token.text in self.native:
            if document.language == "mull":
                receiver = re.search(r"\b([A-Za-z_]\w*)\s*:\s*$", document.text[:token.start], re.ASCII)
                if not receiver or token.text not in self.members(document, receiver[1], offset):
                    return None
            path = self.root / "include" / self.native[token.text]
            if path.is_file():
                text = path.read_text(encoding="utf-8", errors="replace")
                match = re.search(r"\b" + re.escape(token.text) + r"\s*\(", text)
                target = Document(path.as_uri(), text, "c")
                start = match.start() if match else 0
                return {"uri": target.uri, "range": target.range(start, start + len(token.text) if match else 0)}
        return None

    def dispatch(self, method, params):
        if method == "initialize":
            options = params.get("initializationOptions") or {}
            root = options.get("repositoryRoot")
            if not root:
                root = uri_path(params.get("rootUri") or "")
            if root:
                self.configure_root(root)
            self.package_paths.extend(Path(p).expanduser().resolve() for p in options.get("packagePaths", []))
            self.initialized = True
            return {"capabilities": {
                "positionEncoding": "utf-16", "textDocumentSync": {"openClose": True, "change": 2},
                "completionProvider": {"triggerCharacters": [":", '"', "<"]},
                "hoverProvider": True, "definitionProvider": True, "documentSymbolProvider": True,
            }, "serverInfo": {"name": "MuLL / MuDSL / MuLIB", "version": "0.1.0"}}
        if method == "shutdown":
            self.stopped = True
            return None
        if method == "exit":
            raise SystemExit(0 if self.stopped else 1)
        if method in ("initialized", "$/cancelRequest", "$/setTrace"):
            return None
        if not self.initialized or self.stopped:
            raise ValueError("server is not running")
        if method == "textDocument/didOpen":
            item = params["textDocument"]
            self.update(item["uri"], item["text"], item.get("languageId", "mull"), item["version"])
            return None
        if method == "textDocument/didChange":
            item = params["textDocument"]
            document = self.documents[item["uri"]]
            if item["version"] <= document.version:
                return None
            text = document.text
            for change in params["contentChanges"]:
                if "range" in change:
                    current = Document(document.uri, text, "plaintext")
                    start, end = current.offset(change["range"]["start"]), current.offset(change["range"]["end"])
                    text = text[:start] + change["text"] + text[end:]
                else:
                    text = change["text"]
            self.update(document.uri, text, document.language, item["version"])
            return None
        if method == "textDocument/didClose":
            uri = params["textDocument"]["uri"]
            document = self.documents.pop(uri, None)
            if document and document.language == "mull":
                self.notify("textDocument/publishDiagnostics", {"uri": uri, "diagnostics": []})
            return None
        supported = ("textDocument/completion", "textDocument/hover", "textDocument/definition", "textDocument/documentSymbol")
        if method not in supported:
            raise LookupError(method)
        document = self.documents.get(params["textDocument"]["uri"])
        if not document:
            return None
        if document.too_large:
            return [] if method in ("textDocument/completion", "textDocument/documentSymbol") else None
        if method == "textDocument/documentSymbol":
            return [{"name": b.token.text, "kind": b.kind,
                     "range": document.range(b.token.start, b.token.end),
                     "selectionRange": document.range(b.token.start, b.token.end)} for b in document.bindings]
        offset = document.offset(params["position"])
        return {"textDocument/completion": self.completion, "textDocument/hover": self.hover,
                "textDocument/definition": self.definition}[method](document, offset)

    def update(self, uri, text, language, version):
        document = Document(uri, text, language, version)
        self.documents[uri] = document
        if document.too_large:
            self.notify("window/logMessage", {"type": 2, "message": "MuLL document exceeds the 4 MiB editor limit."})
            if language == "mull":
                self.notify("textDocument/publishDiagnostics", {"uri": uri, "version": version, "diagnostics": [document.diagnostic(0, 0, "source exceeds 4 MiB")]})
            return
        self.diagnostics(document)

    def run(self, stream):
        while True:
            headers = {}
            while True:
                line = stream.readline(8193)
                if not line:
                    return
                if len(line) > 8192:
                    raise ValueError("LSP header too long")
                if line in (b"\r\n", b"\n"):
                    break
                key, value = line.decode("ascii").split(":", 1)
                headers[key.lower()] = value.strip()
            length = int(headers["content-length"])
            if not 0 <= length <= 32 * 1024 * 1024:
                raise ValueError("invalid LSP content length")
            data = stream.read(length)
            if len(data) != length:
                return
            try:
                message = json.loads(data)
            except (ValueError, UnicodeError):
                self.send({"id": None, "error": {"code": -32700, "message": "Parse error"}})
                continue
            if not isinstance(message, dict) or message.get("jsonrpc") != "2.0" or not isinstance(message.get("method"), str):
                self.send({"id": None, "error": {"code": -32600, "message": "Invalid Request"}})
                continue
            try:
                params = message.get("params")
                if params is not None and not isinstance(params, dict):
                    raise ValueError("params must be an object")
                result = self.dispatch(message["method"], params or {})
                if "id" in message:
                    self.send({"id": message["id"], "result": result})
            except (KeyError, ValueError, TypeError, LookupError, OSError) as error:
                code = -32601 if type(error) is LookupError else -32602
                if "id" in message:
                    self.send({"id": message["id"], "error": {"code": code, "message": str(error)}})
                else:
                    self.notify("window/logMessage", {"type": 1, "message": str(error)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stdio", action="store_true", help="stdio transport (the default)")
    parser.add_argument("--root", help="MuLIB repository or standalone MuDSL directory")
    parser.add_argument("--checker", help="path to the parse-only mull-check executable")
    parser.add_argument("--package-path", action="append", default=[], help="MuLL package root; repeatable")
    args = parser.parse_args()
    sibling = Path(__file__).with_name("mull-check.exe" if sys.platform == "win32" else "mull-check")
    checker = args.checker or (str(sibling) if sibling.is_file() else None)
    try:
        Server(sys.stdout.buffer, args.root, checker, args.package_path).run(sys.stdin.buffer)
    except (ValueError, KeyError, UnicodeError) as error:
        print(f"mull-lsp: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
