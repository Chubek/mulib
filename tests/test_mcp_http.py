#!/usr/bin/env python3
"""Exercise the socket-free C binding on TCP loopback with independent HTTP/JSON fixtures."""
import argparse
import base64
import json
import socket
import socketserver
import subprocess
import threading


VERSION = "2026-07-28"


class Endpoint(socketserver.StreamRequestHandler):
    def handle(self):
        # Test driver only. Limit reads and launch the C binding with the raw
        # request, not a reserialized Python view that might hide duplicates.
        self.connection.settimeout(5)
        lines = []
        for _ in range(80):
            line = self.rfile.readline(8192)
            assert line.endswith(b"\r\n"), line
            lines.append(line)
            if line == b"\r\n":
                break
        assert lines[-1] == b"\r\n"
        n = next((int(x.split(b":", 1)[1]) for x in lines
                  if x.lower().startswith(b"content-length:")), 0)
        assert 0 <= n <= 65536
        body = self.rfile.read(n)
        assert len(body) == n
        if self.server.fixture:
            result = subprocess.run([self.server.fixture, "--respond"],
                                    input=b"".join(lines) + body, capture_output=True,
                                    timeout=5, check=True).stdout
        else:
            request = json.loads(body)
            assert request["method"] == "prompts/get" and request["params"]["name"] == "café"
            assert b"Mcp-Name: =?base64?Y2Fmw6k=?=\r\n" in lines
            response = json.dumps({"jsonrpc": "2.0", "id": request["id"],
                                  "result": {"resultType": "complete", "messages": [
                                      {"role": "user", "content": {"type": "text", "text": "fixture"}}
                                  ]}}).encode()
            sse = b":keep-alive\r\n\r\ndata: " + response + b"\r\n\r\n"
            result = b"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nTransfer-Encoding: chunked\r\n\r\n"
            for i in range(0, len(sse), 13):
                chunk = sse[i:i + 13]
                result += f"{len(chunk):x}\r\n".encode() + chunk + b"\r\n"
            result += b"0\r\n\r\n"
        self.wfile.write(result)


def exchange(server, wire):
    with socket.create_connection(server.server_address, timeout=5) as peer:
        peer.sendall(wire)
        peer.shutdown(socket.SHUT_WR)
        parts = []
        while part := peer.recv(8192):
            parts.append(part)
    return b"".join(parts)


def req(method, fields=None, *, omit=(), extra=(), variant=VERSION, headers_method=None,
        name=None, verb="POST"):
    frame = {"jsonrpc": "2.0", "method": method, "id": "req-7", "params": {
        "_meta": {"io.modelcontextprotocol/protocolVersion": variant,
                  "io.modelcontextprotocol/clientCapabilities": {}},
        **(fields or {})}}
    body = json.dumps(frame, ensure_ascii=False, indent=2).encode()
    headers = [("Host", "localhost"), ("Accept", "application/json, text/event-stream"),
               ("Content-Type", "application/json; charset=UTF-8"),
               ("Authorization", "Bearer fixture"), ("Mcp-Method", headers_method or method),
               ("MCP-Protocol-Version", variant), ("Content-Length", len(body))]
    if name is not None:
        headers.append(("Mcp-Name", name))
    return (f"{verb} /mcp HTTP/1.1\r\n" +
            "".join(f"{k}: {v}\r\n" for k, v in headers if k not in omit) +
            "".join(f"{k}: {v}\r\n" for k, v in extra) + "\r\n").encode() + body


def check(server, wire, status, code=None):
    head, body = exchange(server, wire).split(b"\r\n\r\n", 1)
    assert int(head.split(b" ", 2)[1]) == status, (head, body)
    if code is not None:
        assert json.loads(body)["error"]["code"] == code, (head, body)
    return head, body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", required=True)
    fixture = parser.parse_args().fixture
    with socketserver.TCPServer(("127.0.0.1", 0), Endpoint) as server:
        server.fixture = fixture
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            encoded = "=?base64?" + base64.b64encode("café".encode()).decode() + "?="
            approved = req("prompts/get", {"name": "café"}, name=encoded,
                           extra=(("Origin", "https://ui.example"),))
            head, body = check(server, approved, 200)
            assert b"Content-Type: text/event-stream" in head
            assert b"X-Accel-Buffering: no" in head
            assert json.loads(body.removeprefix(b"data: "))["result"]["messages"][0]["content"]["text"] == "Welcome"
            check(server, req("server/discover", extra=(("Origin", "https://unlisted.example"),)), 403)
            check(server, req("server/discover", extra=(("Origin", "null"),)), 403)
            check(server, req("server/discover", extra=(("Origin", "https://ui.example"), ("origin", "https://ui.example"))), 403)
            challenge, _ = check(server, req("server/discover", omit=("Authorization",)), 401)
            assert b'WWW-Authenticate: Bearer realm="fixture"' in challenge
            check(server, req("server/discover", verb="GET"), 405)
            check(server, req("server/discover", omit=("Accept",), extra=(("Accept", "application/json, text/event-stream;q=0"),)), 406)
            check(server, req("server/discover", omit=("Content-Type",), extra=(("Content-Type", "text/plain"),)), 415)
            check(server, req("server/discover", omit=("MCP-Protocol-Version",)), 400, -32020)
            check(server, req("server/discover", extra=(("mcp-method", "server/discover"),)), 400, -32020)
            check(server, req("server/discover", headers_method="tools/list"), 400, -32020)
            check(server, req("prompts/get", {"name": "café"}, name="not-the-name"), 400, -32020)
            check(server, req("prompts/get", {"name": "café"}, name="=?base64?Y2Fmw6k!?="), 400, -32020)
            check(server, req("resources/subscribe"), 404, -32601)
            _, old = check(server, req("server/discover", variant="2025-11-25"), 400, -32022)
            assert json.loads(old)["error"]["data"]["supported"] == [VERSION]
            # Independent client- and server-side fixtures, with actual TCP round trips.
            encoded_c = subprocess.check_output([fixture, "--client-request"], timeout=5)
            response = exchange(server, encoded_c)
            subprocess.run([fixture, "--client-response"], input=response, timeout=5, check=True)
            server.fixture = None
            response = exchange(server, encoded_c)
            subprocess.run([fixture, "--client-response"], input=response, timeout=5, check=True)
        finally:
            server.shutdown()
            thread.join(timeout=5)


if __name__ == "__main__":
    main()
