#!/usr/bin/env python3
"""Black-box stdio tests; pass --on1xc to also run the real compiler smoke."""

import argparse
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
import unittest

SERVER = Path(__file__).resolve().with_name("on1x-lsp.py")
REAL_COMPILER = None
REAL_SYREN = True
REAL_FOREIGNLIB = None


def frame(body):
    return f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body


class Wire:
    def __init__(self, case, compiler, work, env=None, foreignlib=None):
        args = ["--on1xc", str(compiler)] if compiler else []
        if foreignlib:
            args.extend(("--foreignlib-path", str(foreignlib)))
        self.child = subprocess.Popen([sys.executable, str(SERVER), *args], cwd=work,
                                      env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE, bufsize=0)
        case.addCleanup(self.close)
        self.case = case
        self.pending = b""
        self.counter = 0

    def close(self):
        if self.child.poll() is None:
            self.child.terminate()
            try:
                self.child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.child.kill()
                self.child.wait(timeout=3)
        for stream in (self.child.stdin, self.child.stdout, self.child.stderr):
            stream.close()

    def write(self, data):
        view = memoryview(data)
        while view:
            view = view[self.child.stdin.write(view):]

    def send(self, **message):
        data = json.dumps({"jsonrpc": "2.0", **message}, ensure_ascii=False).encode("utf-8")
        self.write(frame(data))

    def receive(self):
        deadline = time.monotonic() + 9

        def read():
            wait = max(0, deadline - time.monotonic())
            self.case.assertTrue(select.select([self.child.stdout], [], [], wait)[0],
                                 "timed out waiting for an LSP message")
            chunk = os.read(self.child.stdout.fileno(), 65536)
            self.case.assertTrue(chunk, "unexpected server EOF")
            self.pending += chunk

        while b"\r\n\r\n" not in self.pending:
            read()
        header, self.pending = self.pending.split(b"\r\n\r\n", 1)
        name, count = header.split(b":")
        self.case.assertEqual(name, b"Content-Length", header)
        length = int(count)
        while len(self.pending) < length:
            read()
        payload, self.pending = self.pending[:length], self.pending[length:]
        packet = json.loads(payload)
        self.case.assertEqual(packet.pop("jsonrpc"), "2.0")
        return packet

    def request(self, method, params=None):
        self.counter += 1
        ident = f"id-{self.counter}"
        self.send(id=ident, method=method, params=params)
        response = self.receive()
        self.case.assertEqual(response.pop("id", None), ident, response)
        return response

    def initialize(self, root=None, encodings=None):
        capabilities = {"general": {"positionEncodings": encodings}} if encodings else {}
        response = self.request("initialize", {"rootUri": root, "capabilities": capabilities})
        self.send(method="initialized", params={})
        return response["result"]["capabilities"]

    def document(self, suffix, uri, **other):
        self.send(method=f"textDocument/{suffix}", params={"textDocument": {"uri": uri}, **other})

    def open(self, uri, text, version=1):
        self.send(method="textDocument/didOpen", params={
            "textDocument": {"uri": uri, "languageId": "on1x", "text": text, "version": version}})

    def change(self, uri, version, changes):
        self.send(method="textDocument/didChange", params={
            "textDocument": {"uri": uri, "version": version}, "contentChanges": changes})

    def format(self, uri):
        return self.request("textDocument/formatting", {
            "textDocument": {"uri": uri}, "options": {"tabSize": 4, "insertSpaces": True}})

    def operator(self, method, uri, line, character):
        return self.request(f"textDocument/{method}", {
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character}})

    def actions(self, uri, only=None):
        context = {"diagnostics": []}
        if only is not None:
            context["only"] = only
        return self.request("textDocument/codeAction", {
            "textDocument": {"uri": uri},
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": 100, "character": 0},
            },
            "context": context,
        })

    def diagnostics(self, uri, version):
        packet = self.receive()
        self.case.assertEqual(packet.get("method"), "textDocument/publishDiagnostics", packet)
        self.case.assertEqual(packet["params"]["uri"], uri)
        self.case.assertEqual(packet["params"]["version"], version)
        return packet["params"]["diagnostics"]

    def log(self):
        packet = self.receive()
        self.case.assertEqual(packet.get("method"), "window/logMessage", packet)
        return packet["params"]["message"]

    def finish(self):
        self.case.assertEqual(self.request("shutdown"), {"result": None})
        self.send(method="exit")
        self.case.assertEqual(self.child.wait(timeout=3), 0)
        self.case.assertEqual(self.child.stderr.read(), b"")


# This is only a subprocess-boundary fixture. Real diagnostics/formatting are
# covered independently below.
FAKE_COMPILER = r'''
import json
import os
from pathlib import Path
import sys
import time

mode, *args, name = sys.argv[1:]
path = Path(name)
assert path.is_absolute()
assert set(os.environ).isdisjoint(("ON1X_PPX_PATH", "ON1X_STDLIB", "LD_PRELOAD"))
assert os.environ.get("MULIB_FOREIGNLIB_PATH") != "/ambient-ignored"
assert Path.cwd() == Path(os.environ["TMPDIR"])
text = path.read_bytes()
if mode == "fmt":
    assert args == ["--isolated", "--target", "plain"]
    assert path.parent == Path.cwd()
    assert Path(".on1xfmt").read_bytes() == b""
    if b"hang" in text:
        time.sleep(30)
    if b"flood" in text:
        sys.stdout.buffer.write(b"x" * (5 * 1024 * 1024))
    elif b"error" in text:
        print("formatter rejected input", file=sys.stderr)
        sys.exit(1)
    else:
        sys.stdout.buffer.write(text.replace(b"  ", b" "))
elif mode == "--deps":
    assert args == ["--diagnostics", "json", "--color", "never"]
    if b"no-json" in text:
        print("JSON unavailable", file=sys.stderr)
        sys.exit(2)
    if b"race" in text:
        path.write_bytes(b"raced")
    if b"bad-json" in text:
        print("{", file=sys.stderr)
        sys.exit(1)
    if b"diagnose" not in text:
        sys.exit(0)
    offset = text.index(b"diagnose")
    records = [{
        "id": "TEST001", "severity": "warning", "message": "test diagnostic",
        "location": {"file": name, "offset": offset, "line": text[:offset].count(b"\n") + 1,
                      "byteColumn": offset - text.rfind(b"\n", 0, offset)}}
    ]
    if b"bad-repair" in text:
        records[0]["repairs"] = [
            {"id": "malformed", "status": "validated", "reason": "",
             "edits": [{"begin": len(text) + 1, "end": len(text) + 2,
                        "replacement": "outside"}]},
        ]
    elif b"repair" in text:
        records[0]["repairs"] = [
            {"id": "validated-repair", "status": "validated", "reason": "",
             "edits": [{"begin": offset, "end": offset + len(b"diagnose"),
                        "replacement": "fixed"}]},
            {"id": "candidate-repair", "status": "candidate", "reason": "",
             "edits": [{"begin": offset, "end": offset,
                        "replacement": "maybe "}]},
        ]
    else:
        records[0]["repairs"] = []
    if b"elsewhere" in text:
        records[0]["location"]["file"] = str(path.parent / "dependency.on1x")
    if b"bogus" in text:
        records[0]["location"]["byteColumn"] += 1
    print(json.dumps({"schema": "on1x.diagnostics/v1", "dropped": 0, "diagnostics": records}),
          file=sys.stderr)
    sys.exit(1)
elif mode == "--operator-info":
    assert args == ["--source-only"]
    declaration = path.with_name("dependency % 1.on1x")
    offset = text.index(b"<+>")
    if b"race" in text:
        path.write_bytes(b"raced")
    print(json.dumps({
        "version": 1, "fixity": [],
        "declarations": [{
            "lexeme": "<+>", "lexeme2": "", "arity": 2, "position": "infix",
            "precedence": 5, "tightness": 1, "binds": "left",
            "signature": "fn(a: i64, b: i64) -> i64",
            "file": str(declaration), "line": 1, "column": 1, "offset": 0,
        }],
        "uses": [{
            "declaration": 99 if b"bad-index" in text else 0, "length": 3,
            "file": name, "line": text.count(b"\n", 0, offset) + 1,
            "column": offset - text.rfind(b"\n", 0, offset), "offset": offset,
        }],
    }))
elif mode == "aux":
    assert args == ["format"]
    assert path.name in ("On1xfile", "On1xfile.m", "On1xfile.i")
    assert path.parent == Path.cwd()
    assert "MULIB_FOREIGNLIB_PATH" not in os.environ
    if b"bad-policy" in text:
        print(f"on1x-project: {path}:2-3: invalid manifest policy", file=sys.stderr)
        sys.exit(1)
    sys.stdout.buffer.write(text.replace(b"  ", b" "))
else:
    sys.exit("unexpected compiler mode")
'''


class Base(unittest.TestCase):
    def tempdir(self, prefix):
        temporary = tempfile.TemporaryDirectory(prefix=prefix)
        self.addCleanup(temporary.cleanup)
        return Path(temporary.name)

    def setUp(self):
        self.work = self.tempdir("on1x-lsp-tests-")
        self.path = self.work / "literal % 🙂 name.on1x"
        self.uri = self.path.as_uri()

    def write(self, text):
        self.path.write_bytes(text.encode())


class Protocol(Base):
    def setUp(self):
        super().setUp()
        self.compiler = self.work / "fake compiler"
        self.compiler.write_text(f"#!{sys.executable}\n" + FAKE_COMPILER, encoding="utf-8")
        self.compiler.chmod(0o700)

    def launch(self):
        return Wire(self, self.compiler, self.work,
                    {**os.environ, "ON1X_PPX_PATH": "/invalid", "ON1X_STDLIB": "/invalid",
                     "MULIB_FOREIGNLIB_PATH": "/ambient-ignored"})

    def test_manifest_diagnostics_and_buffer_format(self):
        wire = self.launch()
        wire.initialize()
        path = self.work / "On1xfile"
        uri = path.as_uri()
        text = 'on1xfile 1\r\n# bad-policy\r\nname "🙂"\r\n'
        path.write_bytes(text.encode())
        wire.open(uri, text)
        wire.document("didSave", uri)
        diagnostic, = wire.diagnostics(uri, 1)
        self.assertEqual(diagnostic, {
            "source": "on1x-project", "severity": 1, "code": "ON1XFILE", "message": "invalid manifest policy",
            "range": {"start": {"line": 1, "character": 0}, "end": {"line": 2, "character": 9}},
        })
        self.assertEqual(wire.actions(uri), {"result": []})
        wire.change(uri, 2, [{"text": 'on1xfile  1\nname "test"\n'}])
        self.assertEqual(wire.diagnostics(uri, 2), [])
        self.assertEqual(wire.format(uri)["result"][0]["newText"], 'on1xfile 1\nname "test"\n')
        self.assertEqual(path.read_bytes(), text.encode())
        qmr = self.work / "On1xfile.m"
        qmr.write_text("{{ never_render_qmr() }}")
        wire.open(qmr.as_uri(), qmr.read_text())
        wire.document("didSave", qmr.as_uri())
        self.assertIn("QMR manifests are not inspected", wire.log())
        self.assertEqual(wire.diagnostics(qmr.as_uri(), 1), [])
        wire.finish()

    def test_framing_and_lifecycle(self):
        wire = self.launch()
        self.assertEqual(wire.request("textDocument/formatting")["error"]["code"], -32002)
        wire.write(frame(b"{"))
        self.assertEqual(wire.receive()["error"]["code"], -32700)
        body = b'{"jsonrpc":"2.0","id":17,"method":"initialize"}'
        wire.write(f"content-length: {len(body)}\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n".encode() + body)
        initialized = wire.receive()
        self.assertEqual(initialized["id"], 17)
        capabilities = initialized["result"]["capabilities"]
        self.assertEqual(capabilities, {"positionEncoding": "utf-16",
                                        "textDocumentSync": {"openClose": True, "change": 1,
                                                             "save": {"includeText": False}},
                                        "documentFormattingProvider": True,
                                        "hoverProvider": True,
                                        "definitionProvider": True,
                                        "codeActionProvider": {
                                            "codeActionKinds": ["quickfix"],
                                            "resolveProvider": False}})
        for method in ("textDocument/definition", "textDocument/hover"):
            self.assertEqual(wire.request(method), {"result": None})
        for method in ("textDocument/completion",
                       "textDocument/rename", "textDocument/references"):
            self.assertEqual(wire.request(method)["error"]["code"], -32601)
        self.assertEqual(wire.request("initialize")["error"]["code"], -32600)
        self.assertEqual(wire.request("shutdown"), {"result": None})
        self.assertEqual(wire.request("textDocument/formatting")["error"]["code"], -32600)
        wire.send(method="exit")
        self.assertEqual(wire.child.wait(timeout=3), 0)

    def test_operator_navigation_snapshot_and_encoding(self):
        declaration = self.work / "dependency % 1.on1x"
        declaration.write_text('operator (lexeme: "<+>") {}')
        prefix = 'fn main() { println("🙂"); println(4 '
        text = "# lead\n" + prefix + "<+> 2); }\n"
        for encoding, column in (("utf-8", len(prefix.encode())), ("utf-16", len(prefix) + 1),
                                 ("utf-32", len(prefix))):
            with self.subTest(encoding=encoding):
                self.write(text.replace("\n", "\r\n"))
                wire = self.launch()
                wire.initialize(encodings=[encoding])
                wire.open(self.uri, text)
                report = wire.operator("hover", self.uri, 1, column + 1)["result"]
                self.assertEqual(report["contents"]["kind"], "plaintext")
                self.assertIn('operator "<+>" (binary infix)', report["contents"]["value"])
                self.assertIn("fn(a: i64, b: i64)", report["contents"]["value"])
                self.assertEqual(report["range"], {
                    "start": {"line": 1, "character": column},
                    "end": {"line": 1, "character": column + 3},
                })
                self.assertEqual(wire.operator("definition", self.uri, 1, column)["result"], {
                    "uri": declaration.as_uri(), "range": {
                        "start": {"line": 0, "character": 0},
                        "end": {"line": 0, "character": 8},
                    },
                })
                wire.open(declaration.as_uri(), "unsaved")
                self.assertEqual(wire.operator("definition", self.uri, 1, column), {"result": None})
                wire.change(self.uri, 2, [{"text": text + "# unsaved"}])
                self.assertEqual(wire.operator("hover", self.uri, 1, column), {"result": None})
                for version, trailer in enumerate(("# bad-index\n", "# race\n"), start=3):
                    updated = text + trailer
                    self.write(updated)
                    wire.change(self.uri, version, [{"text": updated}])
                    self.assertEqual(wire.operator("hover", self.uri, 1, column), {"result": None})
                wire.finish()

    def test_bad_frames_and_exit_status(self):
        for header in (b"Content-Length: 16777217\r\n\r\n",
                       b"Content-Length: 2\r\ncontent-length: 2\r\n\r\n",
                       b"Content-Length: 2\r\n\r\n{",
                       b"X:" + (b"x" * 8191)):
            with self.subTest(header=header[:50]):
                wire = self.launch()
                wire.write(header)
                wire.child.stdin.close()
                self.assertEqual(wire.child.wait(timeout=3), 1)
                self.assertEqual(wire.child.stdout.read(), b"")
                self.assertIn(b"on1x-lsp:", wire.child.stderr.read())
        wire = self.launch()
        wire.initialize()
        wire.send(method="exit")
        self.assertEqual(wire.child.wait(timeout=3), 1)

    def test_save_snapshot_change_and_close(self):
        wire = self.launch()
        wire.initialize(self.work.as_uri())
        text = "# header\r\n🙂 diagnose"
        self.write(text)
        wire.open(self.uri, text, 5)
        # No diagnostics on open. The next frame must be this response.
        self.assertEqual(wire.format(self.uri), {"result": []})
        wire.change(self.uri, 4, [{"text": "obsolete"}])
        wire.document("didSave", self.uri, text="ignore the optional save payload")
        point = {"line": 1, "character": 3}
        self.assertEqual(wire.diagnostics(self.uri, 5), [{
            "range": {"start": point, "end": point}, "message": "test diagnostic",
            "severity": 2, "code": "TEST001", "source": "on1xc"}])
        wire.change(self.uri, 6, [{"text": "first"}, {"text": "new buffer  🙂"}])
        self.assertEqual(wire.diagnostics(self.uri, 6), [])
        wire.document("didSave", self.uri)
        self.assertIn("does not match", wire.log())
        self.assertEqual(wire.diagnostics(self.uri, 6), [])
        result = wire.format(self.uri)["result"]
        self.assertEqual(result[0]["newText"], "new buffer 🙂")
        self.assertEqual(result[0]["range"]["end"], {"line": 0, "character": 14})
        self.assertEqual(self.path.read_bytes(), text.encode())
        wire.change(self.uri, 7, [{"text": text}])
        wire.document("didSave", self.uri)
        self.assertTrue(wire.diagnostics(self.uri, 7))
        wire.document("didClose", self.uri)
        self.assertEqual(wire.diagnostics(self.uri, 7), [])
        self.assertEqual(wire.format(self.uri)["error"]["code"], -32602)
        wire.finish()

    def test_position_encodings_and_line_endings(self):
        for offered, canonical, width in ((["utf-8", "utf-16"], "utf-8", 7),
                                           (["utf-32"], "utf-32", 3),
                                           (["unknown"], "utf-16", 4)):
            with self.subTest(encoding=canonical):
                wire = self.launch()
                capabilities = wire.initialize(encodings=offered)
                self.assertEqual(capabilities["positionEncoding"], canonical)
                text = "first\r\nsecond\r🙂é diagnose  "
                self.write(text)
                wire.open(self.uri, text.replace("\r\n", "\n").replace("\r", "\n"))
                wire.document("didSave", self.uri)
                point = wire.diagnostics(self.uri, 1)[0]["range"]["start"]
                self.assertEqual(point, {"line": 2, "character": width})
                wire.change(self.uri, 2, [{"text": "first\r\nsecond\r🙂é  \r\n"}])
                self.assertEqual(wire.diagnostics(self.uri, 2), [])
                end = wire.format(self.uri)["result"][0]["range"]["end"]
                self.assertEqual(end, {"line": 3, "character": 0})
                wire.finish()

    def test_save_requires_final_newline_agreement(self):
        wire = self.launch()
        wire.initialize()
        self.write("diagnose\r\n")
        wire.open(self.uri, "diagnose")
        wire.document("didSave", self.uri)
        self.assertIn("does not match", wire.log())
        self.assertEqual(wire.diagnostics(self.uri, 1), [])
        wire.finish()

    def test_uri_and_text_limits(self):
        wire = self.launch()
        wire.initialize()
        for uri in ("untitled:buffer.on1x", "file://host/tmp/x.on1x", "file:///tmp/x%ZZ.on1x",
                    "file:///tmp/x%00.on1x", "file:///tmp/x%FF.on1x", "file:///tmp/x.on1x#part",
                    "file:///tmp/x.on1xi"):
            wire.open(uri, "text")
            self.assertEqual(wire.format(uri)["error"]["code"], -32602, uri)
        local = self.uri.replace("file://", "file://localhost", 1)
        wire.open(local, "x" * (2 * 1024 * 1024 + 1))
        self.assertEqual(wire.format(local)["error"]["code"], -32602)
        wire.change(local, 2, [{"text": "short  text"}])
        wire.change(local, 3, [{"text": "incremental", "range": {}}])
        self.assertEqual(wire.format(local)["error"]["code"], -32602)
        wire.change(local, 4, [{"text": "short  text"}])
        self.assertEqual(wire.format(local)["result"][0]["newText"], "short text")
        wire.finish()

    def test_bad_reports_and_save_race(self):
        wire = self.launch()
        wire.initialize()
        wire.open(self.uri, "")
        for version, (text, warning) in enumerate((
                ("no-json", "exit 2"), ("bad-json", "diagnostic JSON"),
                ("diagnose elsewhere", "dependency.on1x"), ("diagnose bogus", "findings omitted"),
                ("diagnose race", "changed during")), start=2):
            self.write(text)
            wire.change(self.uri, version, [{"text": text}])
            wire.document("didSave", self.uri)
            self.assertIn(warning, wire.log())
            self.assertEqual(wire.diagnostics(self.uri, version), [])
        wire.finish()

    def test_versioned_validated_and_disabled_candidate_actions(self):
        wire = self.launch()
        wire.initialize()
        text = "diagnose repair"
        self.write(text)
        wire.open(self.uri, text, 7)
        wire.document("didSave", self.uri)
        diagnostics = wire.diagnostics(self.uri, 7)
        self.assertEqual(
            diagnostics[0]["data"]["on1xRepairActions"], [0, 1])
        actions = wire.actions(self.uri)["result"]
        self.assertEqual(len(actions), 2)
        validated, candidate = actions
        self.assertTrue(validated["isPreferred"])
        change = validated["edit"]["documentChanges"][0]
        self.assertEqual(
            change["textDocument"], {"uri": self.uri, "version": 7})
        self.assertEqual(change["edits"][0]["newText"], "fixed")
        self.assertEqual(
            validated["data"]["on1xRepairStatus"], "validated")
        self.assertNotIn("edit", candidate)
        self.assertIn("unverified", candidate["disabled"]["reason"])
        self.assertEqual(wire.actions(self.uri, ["refactor"])["result"], [])
        wire.change(self.uri, 8, [{"text": text + " changed"}])
        self.assertEqual(wire.diagnostics(self.uri, 8), [])
        self.assertEqual(wire.actions(self.uri)["result"], [])
        wire.finish()

    def test_malformed_repair_is_not_an_action(self):
        wire = self.launch()
        wire.initialize()
        text = "diagnose bad-repair"
        self.write(text)
        wire.open(self.uri, text)
        wire.document("didSave", self.uri)
        self.assertIn("findings omitted", wire.log())
        diagnostic, = wire.diagnostics(self.uri, 1)
        self.assertNotIn("data", diagnostic)
        self.assertEqual(wire.actions(self.uri)["result"], [])
        wire.finish()

    def test_format_failure_output_limit_and_timeout(self):
        wire = self.launch()
        wire.initialize()
        wire.open(self.uri, "")
        for version, (text, message) in enumerate((
                ("error", "formatter rejected input"), ("flood", "output limit"),
                ("hang", "timed out"), ("okay", None)), start=2):
            wire.change(self.uri, version, [{"text": text}])
            response = wire.format(self.uri)
            if message:
                self.assertEqual(response["error"]["code"], -32803)
                self.assertIn(message, response["error"]["message"])
            else:
                self.assertEqual(response, {"result": []})
        wire.finish()

    def test_workspace_path_candidates_not_executed(self):
        # Default PATH search rejects both a workspace executable and a trusted
        # directory symlink whose ultimate target is inside the workspace.
        bindir = self.tempdir("on1x-path-")
        trip = self.work / "on1xc"
        marker = self.work / "marker"
        trip.write_text(f"#!{sys.executable}\nfrom pathlib import Path\nPath({str(marker)!r}).touch()\n")
        trip.chmod(0o700)
        (bindir / "on1xc").symlink_to(trip)
        wire = Wire(self, None, self.work, {**os.environ, "PATH": f".:{self.work}:{bindir}"})
        wire.initialize(self.work.as_uri())
        wire.open(self.uri, "fn main(){}")
        self.assertIn("trusted absolute PATH", wire.format(self.uri)["error"]["message"])
        self.assertFalse(marker.exists())
        wire.finish()
        # A real executable on an absolute allowed PATH is selected.
        (bindir / "on1xc").unlink()
        (bindir / "on1xc").write_bytes(self.compiler.read_bytes())
        (bindir / "on1xc").chmod(0o700)
        wire = Wire(self, None, self.work, {**os.environ, "PATH": str(bindir)})
        wire.initialize(self.work.as_uri())
        wire.open(self.uri, "okay")
        self.assertEqual(wire.format(self.uri), {"result": []})
        wire.finish()


class RealCompiler(Base):
    def setUp(self):
        if REAL_COMPILER is None:
            self.skipTest("pass --on1xc PATH to run the real compiler smoke")
        super().setUp()
        self.wire = Wire(self, REAL_COMPILER, self.work,
                         {**os.environ, "ON1X_STDLIB": "/not-a-real-stdlib",
                          "MULIB_FOREIGNLIB_PATH": "/ambient-ignored"}, REAL_FOREIGNLIB)
        self.wire.initialize(self.work.as_uri())

    def test_untouched_source_format(self):
        text = 'fn main(){ println("🙂"); let x = ;}\n'
        self.wire.open(self.uri, text)
        self.write(text)
        self.assertEqual(self.wire.format(self.uri)["error"]["code"], -32803)

        # Malformed workspace formatter settings must not affect buffer format.
        (self.work / ".on1xfmt").write_text("invalid\n")
        good = '# on1xfmt: target = latex\nfn main(){println("🙂");}'
        self.wire.change(self.uri, 2, [{"text": good}])
        edits = self.wire.format(self.uri)["result"]
        expected = '# on1xfmt: target = latex\nfn main() {\n  println("🙂");\n}\n'
        self.assertEqual(edits[0]["newText"], expected)
        self.assertEqual(self.path.read_bytes(), text.encode())
        self.wire.change(self.uri, 3, [{"text": expected}])
        self.assertEqual(self.wire.format(self.uri), {"result": []})
        self.wire.finish()

    def test_diagnostics(self):
        text = '# CRLF file\nfn main(){ println("🙂"); let x = ;}\n'
        self.write(text.replace("\n", "\r\n"))
        self.wire.open(self.uri, text)
        self.wire.document("didSave", self.uri)
        if REAL_SYREN:
            prefix = text.splitlines()[1].split("let x = ;")[0] + "let x = "
            point = {"line": 1, "character": len(prefix.encode("utf-16-le")) // 2}
            diagnostic, = self.wire.diagnostics(self.uri, 1)
            self.assertEqual(diagnostic["range"], {"start": point, "end": point})
            self.assertEqual(diagnostic["severity"], 1)
            self.assertEqual(diagnostic["code"], "ON1X-SOURCE")
            self.assertTrue(diagnostic["message"])
            good = 'fn main() {}\n'
            self.wire.change(self.uri, 2, [{"text": good}])
            self.assertEqual(self.wire.diagnostics(self.uri, 2), [])
            self.write(good)
            self.wire.document("didSave", self.uri)
            self.assertEqual(self.wire.diagnostics(self.uri, 2), [])
        else:
            self.assertIn("ON1X_WITH_SYREN=ON", self.wire.log())
            self.assertEqual(self.wire.diagnostics(self.uri, 1), [])
        self.wire.finish()

    def test_real_import_paths_and_no_ppx(self):
        if not REAL_SYREN:
            self.skipTest("import diagnostics smoke requires Syren")
        tripwire = self.work / "never run"
        marker = self.work / "ran"
        tripwire.write_text(f"#!{sys.executable}\nfrom pathlib import Path\nPath({str(marker)!r}).touch()\n")
        tripwire.chmod(0o700)
        dep = self.work / "dependency.on1x"
        source = 'import "dependency.on1x";\nfn main(){ let value:i64 = "not an i64"; }\n'
        self.write(source)
        dep.write_text('fn value()->i64{ return ;}\nfn invalid( {\n')
        self.wire.open(self.uri, source)
        self.wire.document("didSave", self.uri)
        self.assertIn("dependency.on1x", self.wire.log())
        self.assertEqual(self.wire.diagnostics(self.uri, 1), [])
        dep.write_text('@ppx example "./never run";\n'
                       '@enable Protocols\n@extension Protocols {\n'
                       '  protocol Eq<Self> { fn eq(a:Self,b:Self)->bool; }\n'
                       '}\nfn value()->i64{ return 42; }\n')
        self.wire.document("didSave", self.uri)
        self.assertEqual(self.wire.diagnostics(self.uri, 1), [])
        self.assertFalse(marker.exists())
        dep.unlink()
        self.wire.document("didSave", self.uri)
        self.assertIn("cannot resolve import", self.wire.diagnostics(self.uri, 1)[0]["message"])
        self.wire.finish()

    def test_local_operator_format_and_saved_operator_import(self):
        header = ('operator(lexeme:"<+>",arity:"binary",precedence:5,'
                  'formals:a:i64,b:i64)->i64{return a+b;}\n')
        local = header + 'fn main(){println(40<+>2);}\n'
        self.wire.open(self.uri, local)
        edit, = self.wire.format(self.uri)["result"]
        self.assertIn("operator (lexeme:", edit["newText"])
        self.assertIn("println(40 <+> 2);", edit["newText"])
        self.assertFalse(self.path.exists())
        self.wire.change(self.uri, 2, [{"text": edit["newText"]}])
        self.assertEqual(self.wire.format(self.uri), {"result": []})

        # Private buffer formatting may not follow workspace imports, even if
        # their paths are absolute or the editor has a saved file at this URI.
        dep = self.work / "arithmetic.on1x"
        dep.write_text(header)
        unsaved = f'import "{dep}";\nfn main(){{println(40<+>2);}}\n'
        self.wire.change(self.uri, 3, [{"text": unsaved}])
        self.assertEqual(self.wire.format(self.uri)["error"]["code"], -32803)
        self.assertFalse(self.path.exists())

        if REAL_SYREN:
            prefix = 'fn main() { println("🙂"); println(40 '
            import_line = 'from .arithmetic import (operator("<+>"));\n'
            good = import_line + prefix + '<+> 2); }\n'
            self.write(good.replace("\n", "\r\n"))
            self.wire.change(self.uri, 4, [{"text": good}])
            self.wire.document("didSave", self.uri)
            self.assertEqual(self.wire.diagnostics(self.uri, 4), [])
            bad = import_line + prefix + '<~> 2); }\n'
            self.write(bad.replace("\n", "\r\n"))
            self.wire.change(self.uri, 5, [{"text": bad}])
            self.wire.document("didSave", self.uri)
            report, = self.wire.diagnostics(self.uri, 5)
            point = {"line": 1, "character": len(prefix.encode("utf-16-le")) // 2}
            self.assertEqual(report["range"], {"start": point, "end": point})
            self.assertEqual(report["code"], "ON1X-OPERATOR-UNBOUND")
            self.assertIn("<~>", report["message"])
            self.assertIn("select it explicitly in an import", report["message"])
            self.assertEqual(self.wire.actions(self.uri)["result"], [])
        self.wire.finish()

    def test_real_operator_navigation_without_ppx_execution(self):
        declaration = self.work / "arithmetic.on1x"
        declaration.write_text('operator (lexeme: "<+>", arity: "binary", precedence: 5,'
                               ' formals: a: i64, b: i64) -> i64 { return a + b; }\n')
        prefix = 'fn main() { println("🙂"); println(40 '
        import_line = 'from .arithmetic import (operator("<+>"));\n'
        statement = prefix + '<+> 2); let f = operator("<+>"); println(f(40, 2)); }\n'
        source = import_line + statement
        self.write(source)
        self.wire.open(self.uri, source)
        column = len(prefix.encode("utf-16-le")) // 2
        report = self.wire.operator("hover", self.uri, 1, column)["result"]
        self.assertIn('operator "<+>" (binary infix)', report["contents"]["value"])
        self.assertIn("i64", report["contents"]["value"])
        found = self.wire.operator("definition", self.uri, 1, column)["result"]
        self.assertEqual(found["uri"], declaration.as_uri())
        self.assertEqual(found["range"]["start"], {"line": 0, "character": 0})
        literal_prefix = statement[:statement.rindex("<+>")]
        literal_column = len(literal_prefix.encode("utf-16-le")) // 2
        self.assertEqual(self.wire.operator("definition", self.uri, 1, literal_column)["result"], found)

        tripwire = self.work / "never-run"
        marker = self.work / "ran"
        tripwire.write_text(f"#!{sys.executable}\nfrom pathlib import Path\nPath({str(marker)!r}).touch()\n")
        tripwire.chmod(0o700)
        risky = '@ppx unexpected "./never-run";\n' + source
        self.write(risky)
        self.wire.change(self.uri, 2, [{"text": risky}])
        self.assertEqual(self.wire.operator("hover", self.uri, 2, column), {"result": None})
        self.assertFalse(marker.exists())
        self.wire.finish()

    def test_plain_manifest_driver(self):
        supports_aux = subprocess.run([REAL_COMPILER, "aux", "format", "--help"],
                                      capture_output=True, timeout=5)
        if supports_aux.returncode:
            self.skipTest("requires on1xc aux format")
        path = self.work / "On1xfile"
        uri = path.as_uri()
        text = ('on1xfile 1\nkind  executable\nname "hello"\ndirectory "src"\n'
                'target "hello" "src/main.on1x"\nbackend "release" mir-vm\n'
                'optimization-profile debug\noptimization "release" "integer_add_zero_right" on\n')
        self.wire.open(uri, text)
        self.assertEqual(self.wire.format(uri)["result"][0]["newText"], text.replace("  ", " "))
        invalid = text.replace("optimization-profile debug", "optimization-profile nope")
        self.wire.change(uri, 2, [{"text": invalid}])
        path.write_text(invalid)
        self.wire.document("didSave", uri)
        report, = self.wire.diagnostics(uri, 2)
        self.assertEqual(report["range"]["start"]["line"], 6)
        self.assertIn("optimization", report["message"])
        self.wire.finish()

    def test_foreign_and_kernel_save(self):
        if not REAL_SYREN or REAL_FOREIGNLIB is None:
            self.skipTest("requires Syren and --foreignlib-path with the generated clamp.ffi")
        portable = 'import foreign stdffi.clamp as Clamp;\nfn main() { println(Clamp.add_i64(40, 2)); }\n'
        self.write(portable)
        self.wire.open(self.uri, portable)
        self.wire.document("didSave", self.uri)
        self.assertEqual(self.wire.diagnostics(self.uri, 1), [])
        invalid = portable.replace("add_i64", "missing")
        self.wire.change(self.uri, 2, [{"text": invalid}])
        self.write(invalid)
        self.wire.document("didSave", self.uri)
        report, = self.wire.diagnostics(self.uri, 2)
        self.assertEqual(report["code"], "ON1X-STDFFI-MEMBER")
        native = 'unsafe import kernel[dynlib] mulib.clamp as Clamp;\nfn main() {}\n'
        self.wire.change(self.uri, 3, [{"text": native}])
        self.assertEqual(self.wire.diagnostics(self.uri, 3), [])
        self.write(native)
        self.wire.document("didSave", self.uri)
        report, = self.wire.diagnostics(self.uri, 3)
        self.assertEqual(report["severity"], 3)
        self.assertIn("not checked", report["message"])
        self.wire.finish()


if __name__ == "__main__":
    opts = argparse.ArgumentParser()
    opts.add_argument("--on1xc", type=Path)
    opts.add_argument("--foreignlib-path", type=Path, help="generated stdffi directory with clamp.ffi")
    opts.add_argument("--no-syren", action="store_true", help="expect a compiler without diagnostic JSON")
    args, unittest_args = opts.parse_known_args()
    REAL_COMPILER = args.on1xc.resolve(strict=True) if args.on1xc else None
    REAL_FOREIGNLIB = args.foreignlib_path.resolve(strict=True) if args.foreignlib_path else None
    REAL_SYREN = not args.no_syren
    unittest.main(argv=[sys.argv[0], *unittest_args])
