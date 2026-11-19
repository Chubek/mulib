#!/usr/bin/env python3
"""Independent HTTP/1.1 server for OpenCode V2 native socket integration."""
import argparse
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import subprocess
from threading import Thread
from urllib.parse import parse_qs, urlsplit


class Fixture(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def do_GET(self):
        assert self.headers["Authorization"] == "Bearer fixture"
        url = urlsplit(self.path)
        self.server.requests.append(url.path)
        if url.path == "/api/agent":
            assert self.headers["Accept"] == "application/json"
            assert parse_qs(url.query) == {"location[directory]": ["/workspace a&b"]}
            body = json.dumps([{"id": "build"}]).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif url.path == "/api/event":
            assert self.headers["Accept"] == "text/event-stream" and not url.query
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            body = b'id: 42\r\ndata: {"type":"server.connected"}\r\n\r\n'
            for offset in range(0, len(body), 5):
                chunk = body[offset:offset + 5]
                self.wfile.write(f"{len(chunk):x}\r\n".encode() + chunk + b"\r\n")
            self.wfile.write(b"0\r\n\r\n")
        else:
            self.send_error(404)
        self.close_connection = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", required=True)
    args = parser.parse_args()
    with HTTPServer(("127.0.0.1", 0), Fixture) as server:
        server.requests = []
        thread = Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            subprocess.run([args.client, f"http://127.0.0.1:{server.server_port}"],
                           timeout=10, check=True)
            assert server.requests == ["/api/agent", "/api/event"]
        finally:
            server.shutdown()
            thread.join()


if __name__ == "__main__":
    main()
