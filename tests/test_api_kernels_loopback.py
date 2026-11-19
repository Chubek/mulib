#!/usr/bin/env python3
"""Independent, bounded HTTP/1.1 wire fixtures for the real protocol kernels."""
import argparse
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import struct
import subprocess
from threading import Thread
from xml.etree import ElementTree


def grpc_frame(flags, body):
    return struct.pack("!BI", flags, len(body)) + body


class Peer(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def reply(self, status, media, body, chunked=False):
        self.send_response(status)
        self.send_header("Content-Type", media)
        if chunked:
            self.send_header("Transfer-Encoding", "chunked")
        else:
            self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        if chunked:
            for i in range(0, len(body), 5):
                part = body[i:i + 5]
                self.wfile.write(f"{len(part):x}\r\n".encode() + part + b"\r\n")
            self.wfile.write(b"0\r\n\r\n")
        else:
            self.wfile.write(body)
        self.close_connection = True

    def do_GET(self):
        self.server.observed.append(self.path)
        if self.path == "/rest":
            self.reply(200, "application/json", b'{"ok":true}')
        elif self.path == "/events":
            assert self.headers["Last-Event-ID"] == "2"
            self.reply(200, "text/event-stream; charset=UTF-8",
                       b": comment\r\nid: 3\r\nevent: sample\r\ndata: one\r\ndata: two\r\n\r\n",
                       chunked=True)
        else:
            self.send_error(404)

    def do_POST(self):
        self.server.observed.append(self.path)
        length = int(self.headers["Content-Length"])
        assert 0 < length <= 4096
        body = self.rfile.read(length)
        if self.path == "/graphql":
            assert json.loads(body) == {"query": "query Probe { value }", "operationName": "Probe"}
            self.reply(200, "application/json", b'{"data":{"value":42}}')
        elif self.path == "/rpc":
            assert json.loads(body) == {"jsonrpc": "2.0", "method": "demo.read", "id": "test-id"}
            self.reply(200, "application/json", b'{"jsonrpc":"2.0","id":"test-id","result":true}')
        elif self.path == "/example.Probe/Read":
            assert body == grpc_frame(0, b"\x00a")
            self.reply(200, "application/grpc-web+proto",
                       grpc_frame(0, b"\x00b") + grpc_frame(0x80, b"grpc-status: 0\r\n"),
                       chunked=True)
        elif self.path == "/soap":
            assert 'action="urn:read"' in self.headers["Content-Type"]
            xml = ElementTree.fromstring(body)
            ns = "{http://www.w3.org/2003/05/soap-envelope}"
            assert xml.tag == ns + "Envelope" and xml.find(ns + "Body/{urn:probe}Read") is not None
            self.reply(200, "application/soap+xml", b'<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"><s:Body/></s:Envelope>')
        else:
            self.send_error(404)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", required=True)
    args = parser.parse_args()
    with HTTPServer(("127.0.0.1", 0), Peer) as server:
        server.observed = []
        thread = Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            subprocess.run([args.client, f"http://127.0.0.1:{server.server_port}"],
                           timeout=12, check=True)
            assert server.observed == [
                "/rest", "/graphql", "/rpc", "/example.Probe/Read", "/soap", "/events"
            ]
        finally:
            server.shutdown()
            thread.join()


if __name__ == "__main__":
    main()
