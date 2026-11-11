#!/usr/bin/env python3
"""Offline TCP interoperability: MuLIB client/server and Python's HTTP client."""
import argparse
import http.client
import json
import os
from pathlib import Path
import re
import selectors
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='mulib-openai-', dir='/tmp/opencode') as temp:
        work = Path(temp)
        for name in ('client', 'server'):
            subprocess.run([args.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                            '-I' + str(ROOT / 'include'), str(ROOT / f'examples/modules/openai-{name}.c'),
                            '-lm', '-o', str(work / name)], check=True, timeout=60)
        env = dict(os.environ, OPENAI_API_KEY='synthetic-key')
        server = subprocess.Popen([str(work / 'server'), '--listen', '0'], env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(server.stdout, selectors.EVENT_READ)
                if not selector.select(10):
                    raise RuntimeError('server did not start')
                line = server.stdout.readline()
            match = re.fullmatch(r'Listening on http://127.0.0.1:(\d+)/v1\n', line)
            if not match:
                raise RuntimeError('unexpected server startup: ' + line)
            port = int(match[1])
            env['OPENAI_BASE_URL'] = f'http://127.0.0.1:{port}/v1'
            result = subprocess.run([str(work / 'client'), 'synthetic-model', 'Hello'],
                                    env=env, text=True, capture_output=True, timeout=15, check=True)
            assert result.stdout == 'Hello from MuLIB\n'

            def request(method, target, body=None, headers=None):
                connection = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
                try:
                    connection.request(method, target, body, headers or {})
                    response = connection.getresponse()
                    return response.status, dict(response.getheaders()), response.read()
                finally:
                    connection.close()

            status, headers, body = request('GET', '/v1/models')
            assert status == 401 and 'error' in json.loads(body)
            assert headers['WWW-Authenticate'] == 'Bearer'
            auth = {'Authorization': 'Bearer synthetic-key'}
            status, _, body = request('GET', '/v1/models', headers=auth)
            assert status == 200 and json.loads(body)['data'][0]['id'] == 'synthetic-model'
            status, headers, _ = request('PUT', '/v1/models', headers=auth)
            assert status == 405 and headers['Allow'] == 'GET'
            auth['Content-Type'] = 'application/json'
            status, headers, body = request('POST', '/v1/responses',
                json.dumps({'model': 'synthetic-model', 'input': 'hello', 'stream': True}), auth)
            assert status == 200 and headers['Transfer-Encoding'] == 'chunked'
            assert headers['Content-Type'] == 'text/event-stream'
            events = [json.loads(line[6:]) for line in body.decode().splitlines() if line.startswith('data: ')]
            assert [e['type'] for e in events] == ['response.output_text.delta', 'response.completed']
            assert events[-1]['response']['status'] == 'completed'
            print('OpenAI loopback TCP, auth, JSON, binary HTTP framing and SSE interoperability passed')
        finally:
            server.terminate()
            try:
                server.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.communicate(timeout=5)


if __name__ == '__main__':
    main()
