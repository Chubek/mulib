#!/usr/bin/env python3
"""Optional independent aioquic server on loopback; no Python in the C client."""
import argparse
import asyncio
import pathlib
import tempfile

from aioquic.asyncio import QuicConnectionProtocol, serve
from aioquic.h3.connection import H3_ALPN, H3Connection
from aioquic.h3.events import HeadersReceived
from aioquic.quic.configuration import QuicConfiguration
from aioquic.quic.events import ProtocolNegotiated
from aioquic.tls import CipherSuite

from test_tls_integration import certificate, root


BODY = b"MuLIB HTTP/3\x00" + bytes(range(256)) * 160


class Server(QuicConnectionProtocol):
    def quic_event_received(self, event):
        if isinstance(event, ProtocolNegotiated):
            self.h3 = H3Connection(self._quic)
        if not hasattr(self, "h3"):
            return
        for h3_event in self.h3.handle_event(event):
            if not isinstance(h3_event, HeadersReceived):
                continue
            stream = h3_event.stream_id
            headers = dict(h3_event.headers)
            assert headers[b":method"] == b"GET"
            assert headers[b":scheme"] == b"https"
            assert headers[b":authority"].startswith(b"127.0.0.1:")
            path = headers[b":path"]
            assert path in (b"/objects?q=one%20two", b"/bad-length", b"/not-modified", b"/")
            if path == b"/not-modified":
                self.h3.send_headers(stream, [(b":status", b"304"), (b"content-length", b"42")], end_stream=True)
                continue
            self.h3.send_headers(stream, [(b":status", b"103"), (b"link", b"</style.css>; rel=preload")])
            declared = len(BODY) + (path == b"/bad-length")
            self.h3.send_headers(stream, [(b":status", b"200"), (b"content-type", b"application/json"),
                                          (b"server", b"interop-oracle"),
                                          (b"content-length", str(declared).encode())])
            for offset in range(0, len(BODY), 3000):
                self.h3.send_data(stream, BODY[offset:offset + 3000], end_stream=False)
            self.h3.send_headers(stream, [(b"x-check", b"completed normally")], end_stream=True)


class Lossy(asyncio.DatagramProtocol):
    """UDP forwarder: drop an Initial, then delay and reorder one server packet."""
    def __init__(self, target):
        self.target = target

    def connection_made(self, transport):
        self.transport = transport
        self.client = None
        self.dropped_client = False
        self.delayed_server = False
        self.dropped_request = False
        self.request_forwarded = False
        self.tampered = False

    def datagram_received(self, data, addr):
        if addr == self.target:
            if not self.delayed_server and len(data) > 1200:
                self.delayed_server = True
                asyncio.get_running_loop().call_later(0.07, self.transport.sendto, data, self.client)
            elif self.request_forwarded and not self.tampered and len(data) > 100 and not data[0] & 0x80:
                self.tampered = True
                self.transport.sendto(data[:-1] + bytes([data[-1] ^ 1]), self.client)
            else:
                self.transport.sendto(data, self.client)
        else:
            self.client = addr
            if not self.dropped_client:
                self.dropped_client = True
            elif not self.dropped_request and len(data) > 80 and not data[0] & 0x80:
                self.dropped_request = True
            else:
                if self.dropped_request and len(data) > 80 and not data[0] & 0x80:
                    self.request_forwarded = True
                self.transport.sendto(data, self.target)


async def run(client, directory):
    ca = root(directory, "root")
    other = root(directory, "untrusted")
    cert = certificate(directory, "server", ca, san="IP:127.0.0.1")
    wrong = certificate(directory, "wrong-host", ca, san="IP:127.0.0.2")
    loop = asyncio.get_running_loop()
    for retry in (False, True):
        config = QuicConfiguration(is_client=False, alpn_protocols=H3_ALPN,
                                   cipher_suites=[CipherSuite.AES_128_GCM_SHA256], max_datagram_size=1350)
        config.load_cert_chain(*map(str, cert))
        server = await serve("127.0.0.1", 0, configuration=config, create_protocol=Server, retry=retry)
        address = server._transport.get_extra_info("sockname")
        proxy = None
        try:
            if retry:
                proxy, forwarder = await loop.create_datagram_endpoint(
                    lambda: Lossy(address), local_addr=("127.0.0.1", 0))
            port = (proxy.get_extra_info("sockname") if proxy else address)[1]

            async def get(path, trust=ca[0], limit=None, target_port=port):
                url = f"https://127.0.0.1:{target_port}{path}"
                args = [client, url, str(trust)]
                if limit is not None:
                    args.append(str(limit))
                proc = await asyncio.create_subprocess_exec(
                    *args, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
                try:
                    out, err = await asyncio.wait_for(proc.communicate(), 8)
                except TimeoutError:
                    proc.kill()
                    await proc.wait()
                    raise
                return proc.returncode, out, err

            result = await get("/objects?q=one%20two#not-on-wire")
            assert result == (0, b"200\n" + BODY, b""), (result[0], result[2])
            if retry:
                assert forwarder.dropped_client and forwarder.delayed_server and forwarder.dropped_request
                assert forwarder.tampered
            else:
                for path, options, expected in [
                    ("/bad-length", {}, -6),
                    ("/", {"trust": other[0]}, -5),
                    ("/", {"limit": 2048}, -2),
                ]:
                    rc, out, err = await get(path, **options)
                    assert rc != 0 and not out and f"HTTP/3 error {expected}\n".encode() == err, (path, rc, err)
                assert await get("/not-modified") == (0, b"304\n", b"")
                config.load_cert_chain(*map(str, wrong))
                wrong_server = await serve("127.0.0.1", 0, configuration=config, create_protocol=Server)
                try:
                    wrong_port = wrong_server._transport.get_extra_info("sockname")[1]
                    assert await get("/", target_port=wrong_port) == (1, b"", b"HTTP/3 error -5\n")
                finally:
                    wrong_server.close()
        finally:
            if proxy:
                proxy.close()
            server.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--client", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mulib-h3-") as temp:
        asyncio.run(run(str(pathlib.Path(args.client).resolve()), pathlib.Path(temp)))
