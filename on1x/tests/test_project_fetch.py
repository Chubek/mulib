#!/usr/bin/env python3
"""Exercise the real Git smart HTTPS transport against only loopback fixtures."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import random
import shutil
import ssl
import subprocess
import sys
import tempfile
from threading import Event, Thread
import unittest

ON1X = Path(__file__).resolve().parents[1]
OPTIONS = None
GIT = shutil.which('git', path='/usr/bin:/bin')
OPENSSL = shutil.which('openssl')


class GitHandler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def setup(self):
        super().setup()
        self.connection.settimeout(5)

    def handle(self):
        try:
            super().handle()
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            # Certificate rejection, process timeout, and pack size rejection
            # can deliberately close the TLS connection early.
            pass

    def dispatch(self):
        self.server.requests.append((self.command, self.path, self.headers.get('Authorization')))
        if self.path.startswith('/stall.git/'):
            self.server.release_stall.wait(5)
            return
        if self.path.startswith('/redirect.git/'):
            self.send_response(302)
            self.send_header('Location', self.path.replace('/redirect.git/', '/lib.git/'))
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        if self.path.startswith('/dumb.git/'):
            payload = (b'ref: refs/heads/main\n' if self.path.endswith('/HEAD')
                       else (self.server.head + '\trefs/heads/main\n').encode())
            self.send_response(200)
            self.send_header('Content-Type', 'text/plain')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        if self.path.startswith('/auth.git/'):
            self.send_response(401)
            self.send_header('WWW-Authenticate', 'Basic realm="on1x-test"')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        path, _, query = self.path.partition('?')
        if not path.startswith('/lib.git/'):
            self.send_error(404)
            return
        size = int(self.headers.get('Content-Length', '0'))
        env = dict(self.server.git_env, GIT_PROJECT_ROOT=str(self.server.repository_root),
                   GIT_HTTP_EXPORT_ALL='1', PATH_INFO=path, QUERY_STRING=query,
                   REQUEST_METHOD=self.command, CONTENT_TYPE=self.headers.get('Content-Type', ''),
                   CONTENT_LENGTH=str(size), HTTP_GIT_PROTOCOL=self.headers.get('Git-Protocol', ''))
        result = subprocess.run([self.server.backend], input=self.rfile.read(size),
                                capture_output=True, env=env, timeout=20)
        raw, body = result.stdout.split(b'\r\n\r\n', 1)
        status, headers = 200, []
        for line in raw.decode('ascii').splitlines():
            key, value = line.split(': ', 1)
            if key == 'Status':
                status = int(value.split()[0])
            else:
                headers.append((key, value))
        self.send_response(status)
        for key, value in headers:
            self.send_header(key, value)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    do_GET = do_POST = dispatch


@unittest.skipUnless(sys.platform.startswith('linux') and GIT and OPENSSL,
                     'requires Linux, system Git with HTTPS, and OpenSSL')
class FetchTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='on1x-fetch-test-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.project = self.root / 'project'
        (self.project / 'src').mkdir(parents=True)
        (self.project / 'src/On1xfile.m').write_text('on1xfile 1\nsource "main.on1x"\n')
        (self.project / 'src/main.on1x').write_text(
            'import toolkit.value as V;\nfn main() -> i64 { println(V.number()); return 0; }\n')
        self.git_env = dict(PATH='/usr/bin:/bin', HOME=str(self.root), LC_ALL='C',
                            GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL='/dev/null',
                            GIT_AUTHOR_NAME='Fixture', GIT_AUTHOR_EMAIL='fixture@localhost',
                            GIT_COMMITTER_NAME='Fixture', GIT_COMMITTER_EMAIL='fixture@localhost')
        git_exec = self.git('--exec-path').decode()
        backend = Path(git_exec) / 'git-http-backend'
        if not backend.is_file() or not (Path(git_exec) / 'git-remote-https').is_file():
            self.skipTest('requires git-http-backend and system Git HTTPS support')
        self.ca = self.root / 'ca.pem'
        key = self.root / 'key.pem'
        subprocess.run([OPENSSL, 'req', '-new', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-keyout', key, '-out', self.ca, '-days', '1', '-subj', '/CN=127.0.0.1',
                        '-addext', 'subjectAltName=IP:127.0.0.1'],
                       check=True, capture_output=True, timeout=10)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(self.ca, key)
        self.server = ThreadingHTTPServer(('127.0.0.1', 0), GitHandler)
        self.addCleanup(self.server.server_close)
        self.server.socket = context.wrap_socket(self.server.socket, server_side=True)
        self.server.git_env = self.git_env
        self.server.backend = backend
        self.server.repository_root = self.root
        self.server.requests = []
        self.server.release_stall = Event()
        worker = Thread(target=self.server.serve_forever, kwargs={'poll_interval': 0.01}, daemon=True)
        worker.start()
        self.addCleanup(self.server.shutdown)
        self.addCleanup(self.server.release_stall.set)
        self.source = self.root / 'lib.git'
        self.base = f'https://127.0.0.1:{self.server.server_port}'

    def git(self, *arguments):
        return subprocess.run([GIT, *arguments], env=self.git_env, capture_output=True,
                              check=True, timeout=20).stdout.strip()

    def library(self, *, algorithm='sha1', declared_name='toolkit', extra=None):
        lib = self.source / 'lib'
        lib.mkdir(parents=True)
        self.git('-C', self.source, 'init', '--quiet', '--template=', '--initial-branch=main',
                 '--object-format=' + algorithm)
        self.git('-C', self.source, 'config', 'uploadpack.allowReachableSHA1InWant', 'true')
        (self.source / 'On1xfile').write_text(
            f'on1xfile 1\nkind library\nname "{declared_name}"\ndirectory "lib"\n')
        (lib / 'On1xfile.m').write_text('on1xfile 1\nlibrary "value.on1x"\n')
        (lib / 'value.on1x').write_text('fn number() -> i64 { return 42; }\n')
        # Neither this filter nor the shell file may run on the client.
        (self.source / '.gitattributes').write_text('*.on1x filter=probe\n')
        (self.source / '.gitmodules').write_text(
            f'[submodule "ignored"]\npath = ignored\nurl = {self.base}/submodule.git\n')
        (self.source / 'build.sh').write_text('#!/bin/sh\nexit 99\n')
        if extra:
            extra(self.source)
        self.git('-C', self.source, 'add', '.')
        gitlink = 'a' * (64 if algorithm == 'sha256' else 40)
        self.git('-C', self.source, 'update-index', '--add', '--cacheinfo', f'160000,{gitlink},ignored')
        self.git('-C', self.source, 'commit', '-qm', 'first revision')
        self.git('-C', self.source, 'commit', '--allow-empty', '-qm', 'pinned revision')
        (lib / 'value.on1x').write_text('fn number() -> i64 { return 99; }\n')
        self.git('-C', self.source, 'commit', '-qam', 'new tip')
        pinned = self.git('-C', self.source, 'rev-parse', 'HEAD^').decode()
        self.server.head = self.git('-C', self.source, 'rev-parse', 'HEAD').decode()
        return pinned

    def manifest(self, pin, url=None, *, kind='git-remote'):
        (self.project / 'On1xfile').write_text(
            'on1xfile 1\nkind executable\nname "sample"\ndirectory "src"\n'
            'target "sample" "src/main.on1x"\n'
            f'dependency "toolkit" {kind} {json.dumps(url or self.base + "/lib.git")} "{pin}"\n')
        return self.project / '.on1x-deps/toolkit' / pin

    def selector_manifest(self, selector, value, url=None):
        (self.project / 'On1xfile').write_text(
            'on1xfile 1\nkind executable\nname "sample"\ndirectory "src"\n'
            'target "sample" "src/main.on1x"\n'
            f'dependency "toolkit" git-remote {json.dumps(url or self.base + "/lib.git")} '
            f'{selector} {json.dumps(value)}\n')

    def driver(self, *args, negative=None, env=None, prefix=None):
        result = subprocess.run([*(prefix or (sys.executable, OPTIONS.driver)), *args],
                                cwd=self.project, capture_output=True, text=True, env=env, timeout=100)
        if negative is None:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn(negative, result.stderr)
        return result.stdout.strip()

    def poison(self):
        home = self.root / 'ambient-home'
        home.mkdir()
        probe = home / 'must-not-run'
        script = home / 'probe'
        script.write_text(f'#!/bin/sh\ntouch "{probe}"\nexit 99\n')
        script.chmod(0o755)
        config = home / '.gitconfig'
        config.write_text(f'[credential]\nhelper = !{script}\n'
                          f'[core]\nhooksPath = {home}\n'
                          f'[filter "probe"]\nsmudge = {script}\nrequired = true\n'
                          f'[url "ext::{script}"]\ninsteadOf = {self.base}\n')
        self.git('-C', self.project, 'init', '--quiet', '--template=')
        (self.project / '.git/config').write_text('[invalid enclosing Git config\n')
        (home / '.netrc').write_text('machine 127.0.0.1 login secret password secret\n')
        (home / '.netrc').chmod(0o600)
        (home / 'post-fetch').symlink_to(script)
        hooks = home / 'hooks'
        hooks.mkdir()
        (hooks / 'reference-transaction').symlink_to(script)
        return dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home),
                    GIT_CONFIG_GLOBAL=str(config), GIT_CONFIG_SYSTEM=str(config),
                    GIT_CONFIG_COUNT='1', GIT_CONFIG_KEY_0='http.sslVerify', GIT_CONFIG_VALUE_0='false',
                    GIT_ASKPASS=str(script), GIT_TEMPLATE_DIR=str(home), NETRC=str(home / '.netrc'),
                    GIT_SSL_NO_VERIFY='1', GIT_ALLOW_PROTOCOL='ext', https_proxy='http://127.0.0.1:1',
                    GIT_EXEC_PATH=str(home), GIT_DIR=str(home / 'wrong-git')), probe

    def fetch(self, *extra, negative=None, env=None, ca=True, prefix=None):
        flags = ['--ca-file', self.ca] if ca else []
        return self.driver('fetch', 'toolkit', *flags, *extra,
                           negative=negative, env=env, prefix=prefix)

    def build(self, *flags, negative=None):
        return self.driver('build', '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc, *flags,
                           negative=negative)

    def test_fetch_validate_and_build_offline(self):
        pin = self.library()
        dest = self.manifest(pin)
        env, marker = self.poison()
        self.build(negative='run on1x-project.py fetch toolkit explicitly')
        bwrap = shutil.which('bwrap')
        if bwrap and not subprocess.run([bwrap, '--unshare-all', '--ro-bind', '/usr', '/usr',
                                        '--ro-bind', '/lib64', '/lib64', '/usr/bin/true'],
                                       capture_output=True, timeout=5).returncode:
            self.build('--sandbox', negative='run on1x-project.py fetch toolkit explicitly')
            sandbox = True
        else:
            sandbox = False
        self.assertFalse(self.server.requests)
        self.assertEqual(self.fetch(env=env), str(dest))
        self.assertFalse(marker.exists())
        self.assertEqual({p.name for p in dest.iterdir()}, {'objects'})
        self.assertTrue(list(dest.glob('objects/pack/*.pack')))
        request_count = len(self.server.requests)
        self.assertGreater(request_count, 0)
        self.assertTrue(all(p.startswith('/lib.git/') and auth is None for _, p, auth in self.server.requests))
        self.server.shutdown()
        self.server.server_close()
        self.assertEqual(self.fetch(ca=False), str(dest))  # Cached validation needs no endpoint.
        for flags in ((), ('--sandbox',)) if sandbox else ((),):
            output = Path(self.build(*flags))
            self.assertEqual(subprocess.check_output([output / 'sample'], text=True, timeout=5), '42\n')
            report = json.loads((output / '.on1x-build.json').read_text())
            self.assertEqual(report['dependencies']['toolkit'], {'kind': 'git-remote', 'commit': pin})
        pack, = dest.glob('objects/pack/*.pack')
        pack.write_bytes(b'damaged cached pack')
        self.fetch(ca=False, negative='Git pin')
        self.assertEqual(pack.read_bytes(), b'damaged cached pack')
        self.assertEqual(len(self.server.requests), request_count)

    def test_sha256_pinned_commit(self):
        pin = self.library(algorithm='sha256')
        self.assertEqual(len(pin), 64)
        dest = self.manifest(pin)
        self.assertEqual(self.driver('fetch', 'toolkit', self.project, '--ca-file', self.ca), str(dest))
        self.selector_manifest('branch', 'main')
        self.assertEqual(self.driver('lock', '--ca-file', self.ca),
                         str(self.project / 'On1xfile.lock'))
        entry = json.loads((self.project / 'On1xfile.lock').read_text())['dependencies']['toolkit']
        self.assertEqual(len(entry['commit']), 64)
        self.assertEqual(entry['commit'], self.server.head)

    def test_branch_and_annotated_tag_resolve_to_locked_commits(self):
        pin = self.library()
        self.git('-C', self.source, 'tag', '-am', 'release fixture', 'release-1', pin)
        self.selector_manifest('branch', 'main')
        self.build('--frozen', negative='lockfile is required')
        self.fetch('--offline', negative='selector requires a matching lockfile entry')
        self.assertFalse(self.server.requests)
        branch_store = Path(self.fetch())
        self.assertEqual(branch_store.name, self.server.head)
        lock = json.loads((self.project / 'On1xfile.lock').read_text())
        self.assertEqual(lock['dependencies']['toolkit']['selector'],
                         {'kind': 'branch', 'value': 'main'})
        self.assertEqual(lock['dependencies']['toolkit']['commit'], self.server.head)
        output = Path(self.build('--frozen'))
        self.assertEqual(subprocess.check_output([output / 'sample'], text=True, timeout=5), '99\n')

        self.selector_manifest('tag', 'release-1')
        self.fetch('--frozen', negative='stale entry')
        self.assertEqual(self.driver('lock', '--ca-file', self.ca),
                         str(self.project / 'On1xfile.lock'))
        tag_store = self.project / '.on1x-deps/toolkit' / pin
        self.assertEqual(tag_store.name, pin)
        lock = json.loads((self.project / 'On1xfile.lock').read_text())
        self.assertEqual(lock['dependencies']['toolkit']['selector'],
                         {'kind': 'tag', 'value': 'release-1'})
        self.assertEqual(lock['dependencies']['toolkit']['commit'], pin)
        output = Path(self.build('--frozen'))
        self.assertEqual(subprocess.check_output([output / 'sample'], text=True, timeout=5), '42\n')

        requests = len(self.server.requests)
        self.server.shutdown()
        self.server.server_close()
        self.assertEqual(self.fetch('--frozen', ca=False), str(tag_store))
        self.assertEqual(len(self.server.requests), requests)

    def test_concurrent_fetches_publish_one_valid_cache_generation(self):
        pin = self.library()
        destination = self.manifest(pin)
        command = [sys.executable, OPTIONS.driver, 'fetch', 'toolkit',
                   '--ca-file', str(self.ca)]
        children = [subprocess.Popen(command, cwd=self.project, text=True,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    for _ in range(2)]
        results = [child.communicate(timeout=100) for child in children]
        for child, (stdout, stderr) in zip(children, results):
            self.assertEqual(child.returncode, 0, stderr)
            self.assertEqual(stdout.strip(), str(destination))
        advertisements = [path for method, path, _ in self.server.requests
                          if method == 'GET' and 'info/refs?service=git-upload-pack' in path]
        self.assertEqual(len(advertisements), 1, self.server.requests)
        self.assertEqual({path.name for path in destination.iterdir()}, {'objects'})
        lock = json.loads((self.project / 'On1xfile.lock').read_text())
        self.assertEqual(lock['dependencies']['toolkit']['commit'], pin)

    def test_reject_unadmitted_transports_and_non_pins_before_network(self):
        for url in ('http://127.0.0.1/repo.git', 'file:///etc', 'git://127.0.0.1/repo.git',
                    'ssh://127.0.0.1/repo.git', 'git@localhost:repo', 'ext::anything', '../repo',
                    self.base + '/repo?token=secret', self.base + '/repo#x', self.base + '/../repo',
                    self.base + '/x%2f..', self.base.replace('://', '://secret@') + '/repo'):
            with self.subTest(url=url):
                self.manifest('0' * 40, url)
                self.fetch(negative='HTTPS repository URL')
        for pin in ('HEAD', 'abcdef', 'A' * 40):
            with self.subTest(pin=pin):
                self.manifest(pin)
                self.fetch(negative='full lowercase commit object ID')
        self.manifest('0' * 40, 'vendor/lib', kind='git')
        self.fetch(negative='not a declared git-remote dependency')
        self.assertFalse(self.server.requests)
        self.assertFalse((self.project / '.on1x-deps').exists())

    def test_transport_controls_and_failed_fetch_cleanup(self):
        pin = self.library()
        env, marker = self.poison()
        self.manifest(pin)
        self.fetch(ca=False, env=env, negative='certificate')
        self.assertFalse(self.server.requests)
        for endpoint, diagnostic in (('redirect.git', '302'), ('auth.git', 'terminal prompts disabled'),
                                     ('dumb.git', 'dumb http transport')):
            with self.subTest(endpoint=endpoint):
                dest = self.manifest(pin, f'{self.base}/{endpoint}')
                self.server.requests.clear()
                self.fetch(negative=diagnostic, env=env)
                self.assertTrue(self.server.requests)
                allowed = {f'/{endpoint}/info/refs?service=git-upload-pack'}
                if endpoint == 'dumb.git':
                    allowed.add(f'/{endpoint}/HEAD')
                self.assertTrue(all(p in allowed and auth is None for _, p, auth in self.server.requests),
                                self.server.requests)
                self.assertFalse(marker.exists())
                self.assertEqual(list(dest.parent.iterdir()), [])

    def test_missing_or_noncommit_pin_and_wrong_library_never_publish(self):
        pin = self.library(declared_name='different')
        for check_pin, diagnostic in ((pin, 'certified under the declared dependency name'),
                                      ('0' * 40, 'HTTPS Git fetch')):
            with self.subTest(pin=check_pin):
                dest = self.manifest(check_pin)
                self.fetch(negative=diagnostic)
                self.assertEqual(list(dest.parent.iterdir()), [])
        self.git('-C', self.source, 'tag', '-am', 'fixture', 'fixture', pin)
        dest = self.manifest(self.git('-C', self.source, 'rev-parse', 'refs/tags/fixture').decode())
        self.fetch(negative='must name a commit')
        self.assertEqual(list(dest.parent.iterdir()), [])
        self.selector_manifest('branch', 'does-not-exist')
        self.fetch(negative="branch 'does-not-exist' was not found")
        self.assertEqual(list(dest.parent.iterdir()), [])

    def test_invalid_roles_and_symlink_cache_fail_closed(self):
        def template(source):
            (source / 'lib/On1xfile.m').write_text('on1xfile 1\n{% error("not executed") %}\n')

        dest = self.manifest(self.library(extra=template))
        self.fetch(negative='dependency manifests must be plain declarative text')
        self.assertEqual(list(dest.parent.iterdir()), [])
        self.server.requests.clear()
        dest.symlink_to(self.source)
        self.fetch(negative='symlinks are not allowed')
        self.assertTrue(dest.is_symlink())
        dest.unlink()
        dest.mkdir()
        (dest / 'sentinel').touch()
        self.fetch(negative='expected a self-contained Git objects directory')
        self.assertTrue((dest / 'sentinel').exists())
        self.assertFalse(self.server.requests)

    def test_pack_file_budget_enforced_before_publication(self):
        def add_blob(source):
            (source / 'test-data').write_bytes(random.Random(33).randbytes(2 * 1024 * 1024))

        dest = self.manifest(self.library(extra=add_blob))
        # Lower the production per-file limit in-process to keep the fixture
        # small. The real HTTPS and Git subprocess path is still exercised.
        self.fetch(negative='HTTPS Git fetch', prefix=self.with_limit('MAX_PROCESS', 1024 * 1024))
        self.assertTrue(any(method == 'POST' for method, _, _ in self.server.requests))
        self.assertEqual(list(dest.parent.iterdir()), [])

    def test_shallow_remote_is_not_a_complete_object_store(self):
        self.library()
        (self.source / '.git/shallow').write_text(self.server.head + '\n')
        dest = self.manifest(self.server.head)
        self.fetch(negative='shallow')
        self.assertTrue(any(method == 'POST' for method, _, _ in self.server.requests))
        self.assertEqual(list(dest.parent.iterdir()), [])

    def test_network_timeout_leaves_no_store(self):
        dest = self.manifest('0' * 40, f'{self.base}/stall.git')
        self.fetch(negative='exceeded 1-second subprocess limit', prefix=self.with_limit('GIT_FETCH_TIMEOUT', 1))
        self.assertTrue(self.server.requests)
        self.assertEqual(list(dest.parent.iterdir()), [])

    def with_limit(self, key, value):
        code = ('import runpy, sys; d = runpy.run_path(sys.argv.pop(1)); '
                f'main = d["main"]; main.__globals__[{key!r}] = {value}; sys.exit(main())')
        return (sys.executable, '-c', code, OPTIONS.driver)


if __name__ == '__main__':
    cli = argparse.ArgumentParser()
    cli.add_argument('--on1xc', required=True, type=lambda value: Path(value).resolve())
    cli.add_argument('--cc', default='cc')
    cli.add_argument('--driver', default=str(ON1X / 'tools/on1x-project.py'), type=lambda value: Path(value).resolve())
    OPTIONS, remainder = cli.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remainder], verbosity=2)
