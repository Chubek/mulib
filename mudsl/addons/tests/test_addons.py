#!/usr/bin/env python3
"""Behavioral checks for the editor integrations; no editor installation needed."""
import io
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

ADDONS = Path(__file__).resolve().parents[1]
MUDSL = ADDONS.parent
sys.path.insert(0, str(ADDONS / "lsp"))
from server import Document, Server

CHECKER = os.environ.get("MULL_CHECKER")


def frame(message):
    data = json.dumps(message, ensure_ascii=False).encode("utf-8")
    return f"Content-Length: {len(data)}\r\n\r\n".encode() + data


def messages(data):
    stream, result = io.BytesIO(data), []
    while line := stream.readline():
        headers = {}
        while line not in (b"\r\n", b"\n"):
            key, value = line.decode().split(":", 1)
            headers[key.lower()] = value.strip()
            line = stream.readline()
        result.append(json.loads(stream.read(int(headers["content-length"]))))
    return result


def rpc(method, params=None, identifier=None):
    result = {"jsonrpc": "2.0", "method": method, "params": params or {}}
    if identifier is not None:
        result["id"] = identifier
    return result


def expand_snippet(body):
    defaults = {}

    def default(match):
        defaults[match[1]] = match[2]
        return match[2]

    body = re.sub(r"\$\{(\d+):([^}]*)\}", default, body)
    body = re.sub(r"(?<!\\)\$(\d+)", lambda m: defaults.get(m[1], ""), body)
    return body.replace("\\$", "$")


class LanguageTests(unittest.TestCase):
    def setUp(self):
        self.server = Server(io.BytesIO(), str(MUDSL))

    def complete(self, text, language="mull"):
        document = Document("file:///sample.mull", text, language)
        return self.server.completion(document, len(text))

    def test_completion_is_contextual_and_edits_whole_prefix(self):
        self.assertEqual({item["label"] for item in self.complete('G:Lo')}, {"LoadLib"})
        text = 'val P <- G:LoadLib("printf");\nP:Pr'
        self.assertEqual({i["label"] for i in self.complete(text)}, {"Print", "Printf", "Println"})
        self.assertEqual(self.complete('# G:Lo'), [])
        self.assertEqual(self.complete('val s <- "G:Lo'), [])
        self.assertEqual(self.complete('val G <- {}; G:Lo'), [])
        include = '#include <mulib-ve'
        item = next(i for i in self.complete(include, "c") if i["label"] == "mulib-vec.h")
        doc = Document("file:///file.c", include, "c")
        start = doc.offset(item["textEdit"]["range"]["start"])
        self.assertEqual(include[:start] + item["textEdit"]["newText"], '#include <mulib-vec.h')
        self.assertIn("mulib_vec_init", {i["label"] for i in self.complete("mulib_vec_", "c")})
        self.assertIn("ASTNode", {i["label"] for i in self.complete("dsl::AS", "cpp")})
        self.assertIn("MuLL", {i["label"] for i in self.complete("MuL", "d")})
        self.assertIn("mulib_parsec_range", {i["label"] for i in self.complete('mulib P <- G:MulibWrap("Parsec")\nP:mulib_parsec_')})

    def test_shadowing_parameters_and_comments(self):
        source = ('val x <- 1;\n# val ghost <- 0;\nval s <- "var fake <- 0;";\n'
                  'val f <- fn(x) {\n val y <- x;\n { val x <- 2; x; }\n return x;\n};\nx;')
        doc = Document("file:///scope.mull", source)
        self.assertNotIn("ghost", {b.token.text for b in doc.bindings})
        self.assertNotIn("fake", {b.token.text for b in doc.bindings})
        parameter = source.index("fn(x)") + 3
        use = source.index("return x") + 7
        definition = self.server.definition(doc, use)
        self.assertEqual(definition["range"]["start"], doc.position(parameter))
        inner_use = source.index("x; }")
        self.assertEqual(self.server.definition(doc, inner_use)["range"]["start"], doc.position(source.index("val x <- 2") + 4))
        self.assertEqual(self.server.definition(doc, len(source) - 2)["range"]["start"], doc.position(4))
        self.assertNotIn("y", doc.visible(len(source)))
        self.assertEqual(self.server.definition(Document(doc.uri, 'val x <- 1; val r <- {x: 0}; r:x;'), 31), None)

    def test_packages_headers_and_hover(self):
        text = 'val P <- G:LoadLib("printf"); P:Println("hi");'
        doc = Document("file:///example.mull", text)
        location = self.server.definition(doc, text.index('"printf"') + 2)
        self.assertEqual(location["uri"], (MUDSL / "mull/stdlib/printf/Package.mull").as_uri())
        hover = self.server.hover(doc, text.index("Println"))
        self.assertIn("newline", hover["contents"]["value"])
        self.assertIsNone(self.server.hover(doc, text.index('"hi"') + 1))
        native = Document("file:///example.c", "mulib_vec_init(&values, sizeof(int));", "c")
        location = self.server.definition(native, 2)
        self.assertTrue(location["uri"].endswith("/include/mulib-vec.h"))
        self.assertGreater(location["range"]["start"]["line"], 0)
        self.assertIn("module manual", self.server.hover(native, 2)["contents"]["value"])

    def test_utf16_and_lexical_diagnostics(self):
        doc = Document("file:///unicode.mull", 'val s <- "😀é";\r\nG:Len(s);')
        offset = doc.text.index("é")
        self.assertEqual(doc.position(offset)["character"], offset + 1)
        self.assertEqual(doc.offset(doc.position(offset)), offset)
        self.assertEqual(doc.position(doc.text.index("G:")), {"line": 1, "character": 0})
        self.assertEqual(doc.issues, [])
        self.assertEqual(Document(doc.uri, 'val s <- "# // { }"; // }\n').issues, [])
        self.assertEqual(Document(doc.uri, 'val s <- "line\nline";').issues, [])
        for text, message in [('val s <- "oops', "unterminated string"),
                              ('val s <- "oops\\', "unterminated string"),
                              ('val s <- "\\q";', "invalid string escape"),
                              ('val x <- 1e+;', "invalid exponent"),
                              ('if true {', "unclosed '{'"), ('[1);', "unmatched ')'"),
                              ('val café <- 1;', "unexpected character")]:
            with self.subTest(text=text):
                self.assertIn(message, [d["message"] for d in Document(doc.uri, text).issues])

    def test_oversized_document_recovers_after_incremental_deletion(self):
        self.server.dispatch("initialize", {})
        uri = "file:///large.mull"
        with patch("server.MAX_SOURCE", 16):
            self.server.update(uri, "val x <- 1234567890;", "mull", 1)
            self.assertTrue(self.server.documents[uri].too_large)
            self.server.dispatch("textDocument/didChange", {
                "textDocument": {"uri": uri, "version": 2},
                "contentChanges": [{"range": {"start": {"line": 0, "character": 9},
                                              "end": {"line": 0, "character": 18}}, "text": ""}],
            })
            document = self.server.documents[uri]
            self.assertEqual(document.text, "val x <- 0;")
            self.assertFalse(document.too_large)
            self.assertEqual(document.bindings[0].token.text, "x")


class ProtocolTests(unittest.TestCase):
    def test_real_stdio_lifecycle_and_incremental_edits(self):
        uri = "file:///unicode%20test.mull"
        source = 'val s <- "😀";\nG:Len(s);'
        doc = Document(uri, source)
        emoji = source.index("😀")
        document = {"uri": uri}
        requests = [
            rpc("initialize", {"rootUri": MUDSL.as_uri()}, 1),
            rpc("initialized"),
            rpc("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "mull", "text": source, "version": 1}}),
            rpc("textDocument/hover", {"textDocument": document, "position": {"line": 1, "character": 3}}, 2),
            rpc("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [
                {"range": doc.range(emoji, emoji + 1), "text": "é"},
                {"range": {"start": {"line": 1, "character": 0}, "end": {"line": 1, "character": 9}}, "text": "G:Lo"},
            ]}),
            rpc("textDocument/completion", {"textDocument": document, "position": {"line": 1, "character": 4}}, 3),
            rpc("textDocument/documentSymbol", {"textDocument": document}, 4),
            rpc("textDocument/didChange", {"textDocument": {"uri": uri, "version": 1}, "contentChanges": [{"text": "stale"}]}),
            rpc("textDocument/definition", {"textDocument": document, "position": {"line": 0, "character": 4}}, 5),
            rpc("unknown/request", {}, 6),
            rpc("textDocument/didClose", {"textDocument": document}),
            rpc("shutdown", identifier=7), rpc("exit"),
        ]
        command = [sys.executable, str(ADDONS / "lsp/server.py")]
        if CHECKER:
            command += ["--checker", CHECKER]
        result = subprocess.run(command, input=b"".join(map(frame, requests)), capture_output=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        replies = messages(result.stdout)
        by_id = {r["id"]: r for r in replies if "id" in r}
        self.assertEqual(by_id[1]["result"]["capabilities"]["positionEncoding"], "utf-16")
        self.assertIn("Len", by_id[2]["result"]["contents"]["value"])
        self.assertEqual([i["label"] for i in by_id[3]["result"]], ["LoadLib"])
        self.assertEqual(by_id[4]["result"][0]["name"], "s")
        self.assertEqual(by_id[5]["result"]["range"]["start"], {"line": 0, "character": 4})
        self.assertEqual(by_id[6]["error"]["code"], -32601)
        self.assertIsNone(by_id[7]["result"])
        diagnostics = [r["params"] for r in replies if r.get("method") == "textDocument/publishDiagnostics"]
        self.assertEqual(diagnostics[0]["diagnostics"], [])
        self.assertEqual([d.get("version") for d in diagnostics], [1, 2, None])
        self.assertEqual(diagnostics[-1]["diagnostics"], [])

    def test_malformed_json_and_exit_status(self):
        command = [sys.executable, str(ADDONS / "lsp/server.py")]
        result = subprocess.run(command, input=b"Content-Length: 1\r\n\r\n{" + frame(rpc("exit")), capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(messages(result.stdout)[0]["error"]["code"], -32700)


class IntegrationTests(unittest.TestCase):
    def test_generated_assets_and_snippets(self):
        subprocess.run([sys.executable, str(ADDONS / "generate.py"), "--check"], check=True)
        for path in (ADDONS / "sublime").rglob("*.sublime-snippet"):
            snippet = ET.parse(path).getroot()
            self.assertIsNotNone(snippet.find("scope"))
            body = expand_snippet(snippet.findtext("content"))
            if "LoadLib" in body:
                self.assertIn("$E?", body)
                self.assertNotIn("\\$E?", body)
        templates = ET.parse(ADDONS / "jetbrains/templates/Mu.xml").getroot()
        imported = next(t for t in templates if t.attrib["name"] == "mull-loadlib")
        self.assertIn("$$E?", imported.attrib["value"])
        for path in [ADDONS / "sublime/MuLL.tmLanguage", ADDONS / "jetbrains/MuLL.tmbundle/Syntaxes/MuLL.tmLanguage"]:
            grammar = plistlib.loads(path.read_bytes())
            self.assertEqual(grammar["scopeName"], "source.mull")
            self.assertEqual(grammar["fileTypes"], ["mull"])

    def test_installation_in_paths_with_spaces(self):
        with tempfile.TemporaryDirectory(prefix="mu editors ") as temporary:
            root = Path(temporary)
            for editor in ("vim", "sublime", "jetbrains", "lsp"):
                destination = root / editor
                option = "--prefix" if editor == "lsp" else "--dest"
                subprocess.run(["sh", str(ADDONS / editor / "install.sh"), option, str(destination)], check=True, capture_output=True)
            self.assertTrue((root / "vim/ftdetect/mull.vim").is_file())
            self.assertTrue((root / "sublime/MuLL.tmLanguage").is_file())
            self.assertTrue((root / "jetbrains/templates/Mu.xml").is_file())
            requests = [rpc("initialize", identifier=1), rpc("shutdown", identifier=2), rpc("exit")]
            result = subprocess.run([sys.executable, str(root / "lsp/bin/mull-lsp")], input=b"".join(map(frame, requests)), cwd=root, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            inventory = json.loads((root / "lsp/share/mu-lsp/Inventory.json").read_text())
            self.assertIn("parsec", inventory["modules"])

    @unittest.skipUnless(shutil.which("vim"), "Vim not installed")
    def test_vim_filetype_highlighting_and_indent(self):
        with tempfile.TemporaryDirectory(prefix="mu-vim-") as temporary:
            script = Path(temporary) / "test.vim"
            errors = Path(temporary) / "errors"
            script.write_text(f"""
set nocompatible
execute 'set runtimepath^=' . fnameescape('{ADDONS / 'vim'}')
filetype plugin indent on
syntax enable
edit {temporary}/test.mull
call setline(1, ['val text <- "# {{ }}"; // comment', 'if true {{', 'G:Assert(true);', '}}'])
call assert_equal('mull', &filetype)
call assert_equal('# %s', &commentstring)
call assert_equal('mullStorage', synIDattr(synID(1, 1, 1), 'name'))
call assert_equal('mullString', synIDattr(synID(1, 14, 1), 'name'))
call assert_equal('mullComment', synIDattr(synID(1, 26, 1), 'name'))
setlocal shiftwidth=2 expandtab
normal! gg=G
call assert_equal(0, indent(2))
call assert_equal(2, indent(3))
call assert_equal(0, indent(4))
set virtualedit=onemore
call setline(5, 'G:Lo')
call cursor(5, 5)
call assert_equal(['LoadLib'], mull#Complete(0, 'Load'))
call writefile(v:errors, '{errors}')
if len(v:errors) | cquit | endif
qa!
""", encoding="utf-8")
            environment = {k: v for k, v in os.environ.items() if k not in ("VIM", "VIMRUNTIME")}
            result = subprocess.run(["vim", "-Nu", "NONE", "-i", "NONE", "-n", "-es", "-S", str(script)], env=environment, capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, errors.read_text() if errors.exists() else result.stderr.decode())


@unittest.skipUnless(CHECKER, "set MULL_CHECKER to test native parser diagnostics")
class CheckerTests(unittest.TestCase):
    def parse(self, source):
        result = subprocess.run([CHECKER], input=source.encode(), capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        return json.loads(result.stdout)

    def test_parse_never_executes_and_reports_real_syntax(self):
        self.assertIsNone(self.parse('G:DieWith("must not run"); G:LoadLib("missing");'))
        self.assertIn("expected ';'", self.parse('val x <- 1')["message"])
        self.assertIn("invalid exponent", self.parse('val x <- 1e;')["message"])
        self.assertIsNone(self.parse('mulib P <- G:MulibWrap("Parsec")\nreturn P;'))

    def test_native_diagnostic_byte_columns_become_utf16(self):
        output = io.BytesIO()
        server = Server(output, checker=CHECKER)
        text = 'val s <- "😀é"; @'
        document = Document("file:///unicode.mull", text, version=5)
        server.diagnostics(document)
        diagnostic = messages(output.getvalue())[0]["params"]["diagnostics"][0]
        self.assertEqual(diagnostic["range"]["start"], document.position(text.index("@")))

    def test_existing_success_fixtures_and_mull_snippets_parse(self):
        sources = [p.with_suffix(".mull") for p in (MUDSL / "tests/mull").rglob("*.expected")]
        sources += list((MUDSL / "mull/stdlib").glob("*/Package.mull"))
        for source in sources:
            with self.subTest(source=source):
                self.assertIsNone(self.parse(source.read_text()))
        snippets = json.loads((ADDONS / "shared/snippets.json").read_text())["mull"]
        for name, snippet in snippets.items():
            with self.subTest(snippet=name):
                self.assertIsNone(self.parse(expand_snippet(snippet["body"])))


if __name__ == "__main__":
    unittest.main(verbosity=2)
