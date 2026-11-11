#!/usr/bin/env python3
"""Offline HTTP + two Weblets + a real MicroChan daemon; standard library only."""
import argparse
import ctypes
import http.server
import json
import os
from pathlib import Path
import socket
import signal
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_):
        pass

    def do_GET(self):
        if self.path == '/redirect':
            self.send_response(302)
            self.send_header('Location', '/ok')
            body = b''
        elif self.path == '/chunk':
            self.send_response(200)
            self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()
            self.wfile.write(b'3;foo="bar"\r\na\x00b\r\n0\r\nDigest: test\r\n\r\n')
            return
        else:
            self.send_response(200)
            body = b'{"answer":42}'
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        body = self.rfile.read(int(self.headers['Content-Length']))
        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_HEAD(self):
        self.send_response(200)
        self.send_header('Content-Length', '1000')
        self.end_headers()


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, timeout=15, **kwargs)
    assert result.returncode == 0, (command, result.returncode, result.stdout, result.stderr)
    return result.stdout


def ready(process, predicate):
    deadline = time.monotonic() + 10
    while not predicate():
        assert process.poll() is None, process.communicate()
        assert time.monotonic() < deadline, 'startup timeout'
        time.sleep(0.01)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--muw3', required=True, type=Path)
    parser.add_argument('--microchand', required=True, type=Path)
    args = parser.parse_args()
    muw3, daemon = str(args.muw3.resolve()), str(args.microchand.resolve())
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    processes = []
    background_pid = None
    reaper = None
    try:
        with tempfile.TemporaryDirectory(prefix='muw3-', dir='/tmp/opencode') as work:
            work = Path(work)
            base = f'http://localhost:{server.server_port}'
            assert json.loads(run([muw3, base + '/redirect'])) == {'answer': 42}
            assert run([muw3, base + '/chunk']) == b'a\0b'
            assert run([muw3, '-X', 'HEAD', base + '/ok']) == b''
            assert run([muw3, '-d', 'binary-safe C API', '-H', 'X-Example: yes', base + '/echo']) == b'binary-safe C API'
            out = work / 'body'
            run([muw3, '-o', str(out), base + '/ok'])
            assert json.loads(out.read_bytes()) == {'answer': 42}
            invalid = subprocess.run([muw3, '-H', 'Bad\r\nInjected: yes', base], capture_output=True, timeout=10)
            assert invalid.returncode != 0
            tls = subprocess.run([muw3, '--cacert', '/dev/null', 'https://localhost/'], capture_output=True, timeout=10)
            assert tls.returncode != 0 and b'transport failure' in tls.stderr
            bus = work / 'bus.sock'
            broker = subprocess.Popen([daemon, '--socket', str(bus), '--profile',
                                       str(ROOT / 'library/microchan/profiles/muw3.prof')],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append(broker)
            ready(broker, bus.exists)
            assert bus.stat().st_mode & 0o777 == 0o600
            collision = subprocess.run([daemon, '--socket', str(bus)], capture_output=True, timeout=10)
            assert collision.returncode != 0 and bus.exists()
            # An incomplete/malformed peer cannot block new subscribers.
            bad = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            bad.connect(str(bus))
            bad.sendall(b'not a frame')
            worker_script = work / 'worker.lua'
            marker = work / 'ready'
            worker_script.write_text("""
local w = require('lmuw3')
local bus = w.channel(arg[1])
bus.subscribe('muw3/jobs')
local ready = assert(io.open(arg[2], 'w')); ready:write('ready'); ready:close()
local job = assert(bus.receive(5000))
local r = w.get(job.body):expect(200)
bus.publish('muw3/results', w.json_encode({id=job.id, answer=r:json().answer}))
bus.close()
""")
            worker = subprocess.Popen([muw3, '--weblet', str(worker_script), str(bus), str(marker)],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append(worker)
            ready(worker, marker.exists)
            controller_script = work / 'controller.lua'
            controller_script.write_text("""
local w = require('lmuw3')
local bus = w.channel(arg[1])
bus.subscribe('muw3/results')
bus.publish('muw3/jobs', arg[2])
local event = assert(bus.receive(5000))
local result = w.json_decode(event.body)
assert(event.topic == 'muw3/results' and result.answer == 42)
assert(not pcall(bus.publish, 'forbidden/topic', 'no'))
bus.close()
assert(not pcall(bus.publish, 'muw3/jobs', 'closed'))
print('weblets passed')
""")
            assert b'weblets passed' in run([muw3, '--weblet', str(controller_script), str(bus), base + '/ok'])
            stdout, stderr = worker.communicate(timeout=10)
            assert worker.returncode == 0, (stdout, stderr)
            bad.close()
            broker.terminate()
            stdout, stderr = broker.communicate(timeout=10)
            assert broker.returncode == 0, (stdout, stderr)
            assert not bus.exists()
            bad_profile = work / 'bad.prof'
            bad_profile.write_text("return {max_clients='wrong type'}")
            invalid = subprocess.run([daemon, '--socket', str(bus), '--profile', str(bad_profile)],
                                     capture_output=True, timeout=10)
            assert invalid.returncode == 2 and not bus.exists()
            # Become a subreaper so the --daemon child remains waitable by this
            # test after its short-lived launcher exits (Linux test host only).
            if sys.platform == 'linux':
                libc = ctypes.CDLL(None, use_errno=True)
                old = ctypes.c_int()
                assert libc.prctl(37, ctypes.byref(old), 0, 0, 0) == 0
                assert libc.prctl(36, 1, 0, 0, 0) == 0
                reaper = libc, old.value
                background_pid = int(run([daemon, '--socket', str(bus), '--daemon']))
                deadline = time.monotonic() + 10
                while not bus.exists():
                    assert os.waitpid(background_pid, os.WNOHANG) == (0, 0), 'daemon initialization failed'
                    assert time.monotonic() < deadline
                    time.sleep(0.01)
                os.kill(background_pid, signal.SIGTERM)
                while True:
                    pid, status = os.waitpid(background_pid, os.WNOHANG)
                    if pid:
                        background_pid = None
                        assert os.waitstatus_to_exitcode(status) == 0 and not bus.exists()
                        break
                    assert time.monotonic() < deadline
                    time.sleep(0.01)
    finally:
        if background_pid is not None:
            try:
                os.kill(background_pid, signal.SIGKILL)
                os.waitpid(background_pid, 0)
            except ProcessLookupError:
                pass
        if reaper:
            reaper[0].prctl(36, reaper[1], 0, 0, 0)
        for process in processes:
            if process.poll() is None:
                process.terminate()
                try:
                    process.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.communicate()
        server.shutdown()
        server.server_close()
        thread.join()
    print('MuW3 integration: native HTTP/DNS, CLI, two Weblets, daemon/profile and cleanup passed')


if __name__ == '__main__':
    main()
