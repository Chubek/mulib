#!/usr/bin/env python3
"""Exercise Mune Weblets against an independent HTTP/1.1 loopback fixture."""
from argparse import ArgumentParser
from collections import Counter
from contextlib import ExitStack
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
from pathlib import Path
import subprocess
from threading import Thread
from urllib.parse import parse_qsl, urlsplit
from xml.etree import ElementTree


class Peer(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def setup(self):
        super().setup()
        self.connection.settimeout(4)

    def log_message(self, *_args):
        pass

    def reply(self, status, body=b"", media="text/plain", **headers):
        self.send_response(status)
        self.send_header("Connection", "close")
        if status != 204:
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Content-Type", media)
        else:
            assert not body
        for key, value in headers.items():
            self.send_header(key.replace("_", "-"), value)
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def document(self, payload, media="application/json", **headers):
        self.reply(200, json.dumps(payload).encode(), media, **headers)

    def dispatch(self):
        parts = urlsplit(self.path)
        route = parts.path
        self.server.seen[(self.command, route)] += 1
        length = int(self.headers.get("Content-Length", "0"))
        assert 0 <= length <= 4096
        body = self.rfile.read(length)
        if route in ("/redirect", "/api/redirect"):
            if self.command == "POST":
                assert body == b"test-only"
                assert self.headers["X-Fixture-Key"] == "test-only"
                assert parse_qsl(parts.query) == [("key", "test-only")]
            self.reply(307, b"test-only (fixture body)",
                       Location=self.server.redirect_target)
        elif route == "/redirect-same":
            self.reply(302, Location="/landed")
        elif route == "/landed":
            self.reply(200, b"host policy retained")
        elif route == "/api/rest":
            assert self.command == "GET" and not body
            assert self.headers.get_all("X-Fixture") == ["override"]
            assert parse_qsl(parts.query) == [("name", "a & b"), ("name", "%/+!")]
            self.document({"ok": True}, X_Answer="42")
        elif route == "/api/info":
            assert self.command == "GET" and not body
            self.document({"version": "2.0-fixture"})
        elif route in ("/not-found", "/api/not-found"):
            self.reply(404, b"not found")
        elif route == "/binary":
            assert self.command == "POST" and body == b"a\x00b"
            self.reply(200, b"b\x00a", "application/octet-stream")
        elif route == "/graphql":
            assert self.command == "POST"
            request = json.loads(body)
            assert request["query"] == "query Read { answer }"
            assert request["operationName"] == "Read"
            self.document({"data": {"answer": 42}}, "application/graphql-response+json")
        elif route == "/rpc":
            assert self.command == "POST"
            request = json.loads(body)
            if isinstance(request, list):
                first, notice, second = request
                assert [p["jsonrpc"] for p in request] == ["2.0"] * 3
                assert [p["method"] for p in request] == [
                    "example.first", "example.notice", "example.second"
                ]
                assert "id" not in notice and first["id"] != second["id"]
                self.document([
                    {"jsonrpc": "2.0", "id": second["id"],
                     "error": {"code": -32601, "message": "Method not found"}},
                    {"jsonrpc": "2.0", "id": first["id"], "result": first["method"]},
                ])
            elif request["method"] == "example.notice":
                assert request["jsonrpc"] == "2.0" and "id" not in request
                self.reply(204)
            else:
                assert request["method"] == "example.read" and request["jsonrpc"] == "2.0"
                assert request["params"] == {"name": "Mune"}
                self.document({"jsonrpc": "2.0", "id": request["id"], "result": {"answer": 42}})
        elif route in ("/soap", "/soap12"):
            assert self.command == "POST"
            root = ElementTree.fromstring(body)
            if route == "/soap":
                ns = "{http://schemas.xmlsoap.org/soap/envelope/}"
                media = "text/xml"
                assert self.headers["SOAPAction"] == '"urn:example"'
            else:
                ns = "{http://www.w3.org/2003/05/soap-envelope}"
                media = "application/soap+xml"
                assert self.headers["SOAPAction"] is None
                assert self.headers.get_param("action") == "urn:example"
            assert root.tag == ns + "Envelope"
            assert root.find(ns + "Body/{urn:example}Read") is not None
            assert self.headers.get_content_type() == media
            self.reply(200, b'<s:Envelope xmlns:s="' + ns[1:-1].encode() +
                       b'"><s:Body><Reply xmlns="urn:reply"/></s:Body></s:Envelope>',
                       media + "; charset=utf-8")
        elif route == "/events":
            assert self.command == "GET" and self.headers["Accept"] == "text/event-stream"
            assert self.headers["Accept-Encoding"] == "identity"
            self.reply(200, b"\xef\xbb\xbfid: 7\r\ndata: one\r\ndata: two\r\n\r\n"
                       b"data: three\r\rdata: incomplete\r", "text/event-stream; charset=UTF-8")
        else:
            self.reply(422)

    do_GET = do_POST = dispatch


class ResultSink(Peer):
    def dispatch(self):
        self.server.seen[self.command] += 1
        self.reply(204)

    do_GET = do_POST = dispatch


def start(stack, handler):
    server = stack.enter_context(HTTPServer(("127.0.0.1", 0), handler))
    server.seen = Counter()
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    stack.callback(thread.join)
    stack.callback(server.shutdown)
    return server, f"http://127.0.0.1:{server.server_port}"


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--muw3", required=True, type=Path)
    args = parser.parse_args()
    test_file = Path(__file__).with_suffix(".lua").resolve()
    library = str(test_file.parents[2])
    with ExitStack() as stack:
        sink, target = start(stack, ResultSink)
        server, base = start(stack, Peer)
        server.redirect_target = target + "/unexpected"
        command = [str(args.muw3.resolve()), "--timeout", "3000"]
        options = ["--weblet", str(test_file), library, base]
        try:
            subprocess.run(command + options, check=True, timeout=15)
            subprocess.run(command + ["--redirects", "0"] + options + ["host-disabled"],
                           check=True, timeout=10)
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
            raise RuntimeError(f"Weblet failed after these loopback routes: {server.seen}") from exc
        assert not sink.seen, "a disabled redirect left the origin"
        assert server.seen == Counter({
            ("GET", "/api/rest"): 1, ("GET", "/api/info"): 1, ("POST", "/binary"): 1,
            ("GET", "/not-found"): 1, ("GET", "/api/not-found"): 1,
            ("POST", "/graphql"): 1, ("POST", "/rpc"): 3, ("POST", "/soap"): 1,
            ("POST", "/soap12"): 1,
            ("GET", "/events"): 1, ("GET", "/redirect"): 2,
            ("GET", "/redirect-same"): 1, ("GET", "/landed"): 1,
            ("POST", "/redirect"): 1, ("POST", "/api/redirect"): 1,
        }), server.seen


if __name__ == "__main__":
    main()
