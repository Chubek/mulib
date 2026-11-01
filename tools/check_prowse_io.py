#!/usr/bin/env python3
"""Exercise native loopback HTTP/WebSocket transport and piped/pseudo-TTY REPL I/O."""
import argparse
import base64
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import select
import socket
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent


def exact(connection, count):
    value = b''
    while len(value) < count:
        part = connection.recv(count - len(value))
        if not part:
            raise RuntimeError('unexpected peer EOF')
        value += part
    return value


def headers(connection):
    data = b''
    while not data.endswith(b'\r\n\r\n'):
        if len(data) >= 32768:
            raise RuntimeError('oversized request headers')
        data += exact(connection, 1)
    lines = data.decode('ascii').split('\r\n')
    return lines[0], {key.lower(): value for key, value in (line.split(': ', 1) for line in lines[1:] if line)}


def websocket_frame(connection):
    head = exact(connection, 2)
    assert head[1] & 128
    length = head[1] & 127
    if length == 126:
        length = struct.unpack('!H', exact(connection, 2))[0]
    elif length == 127:
        length = struct.unpack('!Q', exact(connection, 8))[0]
    assert length < 65536
    mask = exact(connection, 4)
    return head[0] & 15, bytes(ch ^ mask[i % 4] for i, ch in enumerate(exact(connection, length)))


def serve(listener, kind):
    with listener.accept()[0] as connection:
        connection.settimeout(10)
        first, fields = headers(connection)
        if kind == '--http':
            assert first == 'GET /wd/hub/status HTTP/1.1'
            connection.sendall(b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n')
            for chunk in [b'{"value":', b'{"ready":true}}']:
                connection.sendall(f'{len(chunk):x}\r\n'.encode() + chunk + b'\r\n')
            connection.sendall(b'0\r\n\r\n')
        else:
            assert first == 'GET /devtools/page/test HTTP/1.1'
            key = fields['sec-websocket-key']
            accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
            connection.sendall(b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n'
                               b'Connection: keep-alive, Upgrade\r\nSec-WebSocket-Accept: ' + accept + b'\r\n\r\n')
            opcode, payload = websocket_frame(connection)
            request = json.loads(payload)
            assert opcode == 1 and request['method'] == 'Browser.getVersion'
            result = json.dumps(dict(id=request['id'], result=dict(ready=True))).encode()
            connection.sendall(bytes([1, 5]) + result[:5] + b'\x89\x04ping')
            opcode, payload = websocket_frame(connection)
            assert opcode == 10 and payload == b'ping'
            connection.sendall(bytes([128, len(result) - 5]) + result[5:])


def network(runner):
    for kind, path in [('--http', '/wd/hub'), ('--cdp', '/devtools/page/test')]:
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen(1)
            listener.settimeout(10)
            with concurrent.futures.ThreadPoolExecutor(max_workers=1) as executor:
                future = executor.submit(serve, listener, kind)
                scheme = 'ws' if kind == '--cdp' else 'http'
                subprocess.run([str(runner), kind, f'{scheme}://127.0.0.1:{listener.getsockname()[1]}{path}'], check=True, timeout=20)
                future.result(timeout=15)


def repl(runner):
    source = ':begin\nfunction twice(x)\nreturn x*2\nend\n:end\n=twice(21)\n' \
             f':profile {ROOT}/library/librl/lua.mldir\n:lua-example print("PROFILE_OK")\n' \
             '=browser.json(browser.array())\n:quit\n'
    result = subprocess.run([str(runner), '--repl'], input=source, text=True, capture_output=True, timeout=20, check=True)
    assert '42\n' in result.stdout and '[]\n' in result.stdout and 'PROFILE_OK\n' in result.stdout, result.stdout
    if os.name != 'posix':
        return
    import fcntl
    import pty
    import termios
    master, slave = pty.openpty()
    try:
        original = termios.tcgetattr(slave)
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 80, 0, 0))
        child = subprocess.Popen([str(runner), '--repl'], stdin=slave, stdout=slave, stderr=slave, close_fds=True)
        output = b''

        def expect(fragment):
            nonlocal output
            deadline = time.monotonic() + 15
            while fragment not in output:
                assert child.poll() is None, output
                remaining = deadline - time.monotonic()
                if remaining <= 0 or not select.select([master], [], [], remaining)[0]:
                    raise AssertionError((fragment, output))
                output += os.read(master, 65536)
            output = output[output.index(fragment) + len(fragment):]

        try:
            expect(b'prowse> ')
            os.write(master, b'=6*8\x1b[D\x1b[3~7\r')
            expect(b'42\r\n')
            os.write(master, b'\x1b[A\r')
            expect(b'42\r\n')
            os.write(master, b'=browser.stat\t().active\r')
            expect(b'false\r\n')
            os.write(master, '=\x1b[200~"caf\u00e9"\x1b[201~\r'.encode())
            expect('caf\u00e9\r\n'.encode())
            os.write(master, b'ignored\x03')
            expect(b'prowse> ')
            os.write(master, b':quit\r')
            assert child.wait(timeout=15) == 0
            assert termios.tcgetattr(slave) == original, 'terminal attributes were not restored'
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
    finally:
        os.close(master)
        os.close(slave)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--protocol', required=True, type=Path)
    parser.add_argument('--repl', required=True, type=Path)
    args = parser.parse_args()
    network(args.protocol.resolve())
    repl(args.repl.resolve())
    print('Prowse native HTTP/WebSocket and pipe/PTY REPL checks passed')


if __name__ == '__main__':
    main()
