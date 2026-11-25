#!/usr/bin/env python3
"""Check direct compiler proxy inheritance without making network requests."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


COMPILER = Path(sys.argv.pop(1)).resolve()
KEYS = ('http_proxy', 'https_proxy', 'any_proxy', 'no_proxy', 'all_proxy')


class ProxyTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='on1x-direct-proxy-')
        self.addCleanup(temporary.cleanup)
        self.project = Path(temporary.name)
        source_root = self.project / 'src'
        source_root.mkdir()
        self.source = source_root / 'main.on1x'
        self.source.write_text('fn main() -> i64 { return 0; }\n')
        self.ppx = self.project / 'ppx'
        self.ppx.write_text(
            f'#!{sys.executable}\n'
            'import json, os, sys\n'
            f'keys = {KEYS!r}\n'
            'with open("observed.json", "w") as output:\n'
            '    json.dump({k: v for k, v in os.environ.items() if k.lower() in keys}, output)\n'
            'sys.stdout.buffer.write(sys.stdin.buffer.read())\n')
        self.ppx.chmod(0o700)
        self.env = {k: v for k, v in os.environ.items() if k.lower() not in KEYS}

    def manifest(self, rows, directory=None, filename='On1xfile'):
        path = (directory or self.project) / filename
        path.write_text('on1xfile 1\nkind executable\nname "sample"\n'
                        'directory "src"\ntarget "sample" "src/main.on1x"\n' + rows)

    def compile(self, cwd=None):
        return subprocess.run([COMPILER, '--check', '--ppx', 'inspect', self.ppx, self.source],
                              cwd=cwd or self.project, env=self.env, text=True,
                              capture_output=True, timeout=10)

    def assert_routes(self, expected, cwd=None):
        result = self.compile(cwd)
        self.assertEqual(result.returncode, 0, result.stderr)
        observed = json.loads(((cwd or self.project) / 'observed.json').read_text())
        self.assertEqual(observed, {spelling: value for key, value in expected.items()
                                    for spelling in (key, key.upper())})

    def test_case_insensitive_environment(self):
        self.env.update(HtTp_PrOxY='http://mixed.invalid',
                        HTTP_PROXY='http://uppercase.invalid',
                        http_proxy='http://lowercase.invalid',
                        aNy_pRoXy='http://fallback.invalid',
                        No_PrOxY='example.org,.test')
        self.assert_routes(dict(http_proxy='http://lowercase.invalid',
                                https_proxy='http://fallback.invalid',
                                any_proxy='http://fallback.invalid',
                                all_proxy='http://fallback.invalid',
                                no_proxy='example.org,.test'))
        del self.env['http_proxy']
        del self.env['HTTP_PROXY']
        self.assert_routes(dict(http_proxy='http://mixed.invalid',
                                https_proxy='http://fallback.invalid',
                                any_proxy='http://fallback.invalid',
                                all_proxy='http://fallback.invalid',
                                no_proxy='example.org,.test'))

    def test_project_scope_precedence_and_literal_fields(self):
        self.env.update(https_proxy='http://ambient.invalid', no_proxy='local.invalid')
        self.manifest('proxy-none\n', self.source.parent)
        self.manifest('proxy-none\n', filename='On1xfile.m')
        self.manifest('proxy-any\n "http:\\/\\/generic.invalid" # continuation\n'
                      'proxy-http \\\n "http://specific.invalid"\n')
        self.assert_routes(dict(http_proxy='http://specific.invalid',
                                https_proxy='http://generic.invalid',
                                any_proxy='http://generic.invalid',
                                all_proxy='http://generic.invalid',
                                no_proxy='local.invalid'))
        other = self.project / 'other'
        other.mkdir()
        self.assert_routes(dict(https_proxy='http://ambient.invalid', no_proxy='local.invalid'),
                           cwd=other)

    def test_proxy_none_suppresses_all_case_variants(self):
        self.env.update(HtTp_PrOxY='http://mixed.invalid',
                        https_proxy='http://secure.invalid',
                        ANY_PROXY='http://fallback.invalid',
                        ALL_PROXY='http://unspecified.invalid',
                        NO_PROXY='local.invalid')
        self.manifest('proxy-none # disable inherited routes\n')
        self.assert_routes(dict(no_proxy='*'))

    def test_https_uses_literal_value(self):
        self.env.update(HtTp_PrOxY='http://plain.invalid', HTTPS_PROXY='http://ambient.invalid')
        literal = 'http://user:$(false)@proxy.invalid'
        self.manifest(f'proxy-https "{literal}"\n')
        self.assert_routes(dict(http_proxy='http://plain.invalid', https_proxy=literal))

    def test_bad_policy_does_not_echo_value(self):
        self.manifest('proxy-https "http://user:fixture-secret@proxy.invalid" extra\n')
        result = self.compile()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('proxy-https', result.stderr)
        self.assertNotIn('fixture-secret', result.stderr)
        self.assertFalse((self.project / 'observed.json').exists())

    def test_root_manifest_never_follows_symlinks(self):
        other = self.project / 'other'
        other.mkdir()
        self.manifest('proxy-none\n', directory=other)
        (self.project / 'On1xfile').symlink_to(other / 'On1xfile')
        result = self.compile()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('root On1xfile', result.stderr)
        self.assertFalse((self.project / 'observed.json').exists())


if __name__ == '__main__':
    unittest.main()
