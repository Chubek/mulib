#!/usr/bin/env python3
"""Test On1xfile dotenv loading independently of the build engine."""
import importlib.util
import os
from pathlib import Path
import runpy
import tempfile
from types import MappingProxyType, SimpleNamespace
import unittest
from unittest.mock import patch


HELPER = Path(__file__).resolve().parents[1] / 'tools/on1x_dotenv.py'
SPEC = importlib.util.spec_from_file_location('_on1x_dotenv_test', HELPER)
DOTENV = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DOTENV)


class ProjectDotenv(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='on1x-dotenv-test-')
        self.addCleanup(temporary.cleanup)
        self.project = Path(temporary.name) / 'project'
        self.project.mkdir()
        self.file = self.project / '.env'

    def test_literal_assignments(self):
        self.file.write_bytes((
            '# a comment\r\n'
            ' \r\n'
            'export NAME\t=  café with spaces # notes\r\n'
            'LOWER_case123=value#literal\r\n'
            'EMPTY = # comment after equals\r\n'
            'SINGLE=\'  # $PATH " \\stuff  \' # comment\r\n'
            'DOUBLE="  # café \\" \\\\ \\n ${NAME}  "\r\n'
            'LAST=first\r\n'
            'LAST=second'
        ).encode('utf-8'))
        self.assertEqual(DOTENV.load(self.project, '.env', {}), {
            'NAME': 'café with spaces',
            'LOWER_case123': 'value#literal',
            'EMPTY': '',
            'SINGLE': '  # $PATH " \\stuff  ',
            'DOUBLE': '  # café " \\ \\n ${NAME}  ',
            'LAST': 'second',
        })

    def test_no_expansion_or_evaluation(self):
        marker = self.project / 'marker'
        value = f'~ $AMBIENT $KEY ${{AMBIENT:-fallback}} $(touch {marker}) `touch {marker}`; *'
        self.file.write_text(f'KEY={value}\n', encoding='utf-8')
        self.assertEqual(DOTENV.load(self.project, '.env', {'AMBIENT': 'expanded'})['KEY'], value)
        self.assertFalse(marker.exists())

    def test_precedence_does_not_mutate_base_or_global_environment(self):
        self.file.write_text('COLLISION=file\nEMPTY=file\nBLANK=\nLOCAL_ONLY=local\n', encoding='utf-8')
        base = {'COLLISION': 'ambient', 'EMPTY': '', 'BLANK': 'ambient', 'AMBIENT_ONLY': 'ambient'}
        ambient = MappingProxyType(base)
        before = dict(os.environ)
        for preserve, collision, empty, blank in ((False, 'file', 'file', ''),
                                                   (True, 'ambient', '', 'ambient')):
            with self.subTest(preserve=preserve):
                options = {'preserve': True} if preserve else {}
                self.assertEqual(DOTENV.load(self.project, '.env', ambient, **options), {
                    'COLLISION': collision, 'EMPTY': empty, 'BLANK': blank,
                    'LOCAL_ONLY': 'local', 'AMBIENT_ONLY': 'ambient',
                })
        result = DOTENV.load(self.project, '.env', os.environ)
        self.assertEqual(result['COLLISION'], 'file')
        self.assertEqual(base, {'COLLISION': 'ambient', 'EMPTY': '', 'BLANK': 'ambient',
                                'AMBIENT_ONLY': 'ambient'})
        self.assertEqual(dict(os.environ), before)

    def test_path_validation(self):
        folder = self.project / '.settings'
        folder.mkdir()
        (folder / 'local values.env').write_text('LOCAL=yes\n', encoding='utf-8')
        relative = DOTENV.local_path('.settings/local values.env')
        self.assertEqual(relative, Path('.settings/local values.env'))
        self.assertEqual(DOTENV.load(self.project, relative, {}), {'LOCAL': 'yes'})
        for value in ('', '.', '..', str(self.file), '../.env', 'nested/../../.env',
                      'nested/./.env', 'nested//.env', '.env/', './.env', r'nested\.env',
                      'C:/tmp/.env', 'nested/\t.env', 'nested/\x00.env'):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, 'project-relative path'):
                DOTENV.load(self.project, value, {})

    def test_symlink_is_never_followed(self):
        outside = self.project.with_name('outside')
        outside.mkdir()
        (outside / '.env').write_text('OUTSIDE=secret\n', encoding='utf-8')
        self.file.symlink_to(outside / '.env')
        (self.project / 'external').symlink_to(outside, target_is_directory=True)
        (self.project / 'dangling').symlink_to(outside / 'missing')
        (self.project / 'inside.env').write_text('INSIDE=secret\n', encoding='utf-8')
        (self.project / 'inside-link').symlink_to('inside.env')
        for relative in ('.env', 'external/.env', 'dangling', 'inside-link'):
            with self.subTest(relative=relative), self.assertRaisesRegex(ValueError, 'symlinks are not allowed'):
                DOTENV.load(self.project, relative, {})

    def test_missing_and_nonregular_inputs(self):
        with self.assertRaisesRegex(ValueError, 'declared file does not exist'):
            DOTENV.load(self.project, '.env', {})
        self.file.mkdir()
        with self.assertRaisesRegex(ValueError, 'expected a regular file'):
            DOTENV.load(self.project, '.env', {})
        self.file.rmdir()
        os.mkfifo(self.file)
        with self.assertRaisesRegex(ValueError, 'expected a regular file'):
            DOTENV.load(self.project, '.env', {})

    def test_bounded_size(self):
        payload = b'KEY=' + b'x' * (DOTENV.MAX_DOTENV - 4)
        self.file.write_bytes(payload)
        self.assertEqual(len(DOTENV.load(self.project, '.env', {})['KEY']), len(payload) - 4)
        self.file.write_bytes(b'KEY=x' + b' ' * (DOTENV.MAX_DOTENV - 5))
        self.assertEqual(DOTENV.load(self.project, '.env', {})['KEY'], 'x')
        self.file.write_bytes(payload + b'\n')
        with self.assertRaisesRegex(ValueError, 'file exceeds'):
            DOTENV.load(self.project, '.env', {})

    def test_invalid_contents_use_only_line_and_reason_in_diagnostic(self):
        for line, reason in ((b'PRIVATE_SENTINEL missing assignment', 'expected NAME=VALUE'),
                             (b'PRIVATE_SENTINEL-NAME=value', 'expected NAME=VALUE'),
                             (b'KEY="PRIVATE_SENTINEL', 'unterminated quoted value'),
                             (b'KEY="ok" PRIVATE_SENTINEL', 'unexpected text after quoted value'),
                             (b'KEY=PRIVATE_SENTINEL\x00', 'unexpected control character'),
                             (b'KEY=PRIVATE_SENTINEL\xff', 'invalid UTF-8')):
            with self.subTest(reason=reason):
                self.file.write_bytes(b'PREVIOUS=PRIVATE_SENTINEL\n' + line + b'\n')
                with self.assertRaisesRegex(ValueError, rf"\.env':2: {reason}") as caught:
                    DOTENV.load(self.project, '.env', {})
                self.assertNotIn('PRIVATE_SENTINEL', str(caught.exception))
                self.assertIsNone(caught.exception.__context__)

    def test_root_manifest_and_compiler_child_environment(self):
        engine = runpy.run_path(str(HELPER.with_name('on1x-project.py')))
        manifest = b'on1xfile 1\nkind library\nname "example"\ndotenv ".env"'
        self.file.write_text('PROJECT_FLAVOR=file\n', encoding='utf-8')
        args = SimpleNamespace(sandbox=False, locked=False, frozen=False, offline=False)
        with patch.dict(os.environ, {'PROJECT_FLAVOR': 'ambient', 'UNDECLARED': 'hidden'}):
            for suffix, expected in ((b'', 'file'), (b' preserve', 'ambient')):
                config = engine['root_manifest'](manifest + suffix + b'\n', self.project / 'On1xfile')
                environment = engine['dotenv_environment'](self.project, config, args)
                self.assertEqual(environment['PROJECT_FLAVOR'], expected)
                self.assertNotIn('UNDECLARED', environment)
                self.assertEqual(os.environ['PROJECT_FLAVOR'], 'ambient')
        args.sandbox = True
        with self.assertRaisesRegex(ValueError, 'unavailable with --sandbox'):
            engine['dotenv_environment'](self.project, config, args)


if __name__ == '__main__':
    unittest.main()
