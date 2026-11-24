#!/usr/bin/env python3
"""Generated FFI build-driver/project smoke checks with a real compiled registry."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
OPTIONS = None
EXPECTED = '42\n9223372036854775807\n-9223372036854775808\n9223372036854775807\n-9223372036854775808\n6\n0\n10\n4\n-7\n'


class Workflows(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix='on1x-native-workflows-')
        self.addCleanup(directory.cleanup)
        self.work = Path(directory.name).resolve()

    def run_cmd(self, *args, rejected=None):
        result = subprocess.run(args, cwd=self.work, text=True, capture_output=True, timeout=180)
        if rejected is None:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn(rejected, result.stderr)
        return result.stdout

    def native(self, source, output, *args, implementation='host', adapter='host', rejected=None):
        result = self.run_cmd(
            sys.executable, '-B', OPTIONS.tools / 'on1x-build.py', source, '-o', output,
            '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc, '--mulib-root', ROOT,
            '--no-prelude', '--ffi-provider', implementation, '--ffi-adapter-provider', adapter,
            *args, rejected=rejected)
        if rejected is not None:
            self.assertFalse(output.exists())
        return result

    def test_clamp_host(self):
        self.clamp('host', 'host')

    def test_clamp_cephyr_adapter(self):
        self.clamp('host', 'cephyr')

    def test_clamp_cephyr_implementation(self):
        self.clamp('cephyr', 'host')

    def test_clamp_cephyr_both(self):
        self.clamp('cephyr', 'cephyr')

    def clamp(self, implementation, adapter):
        if 'cephyr' in (implementation, adapter) and not OPTIONS.cephyr:
            self.skipTest('embedded Cephyr was not requested')
        source = ROOT / 'on1x/examples/kernel/clamp.on1x'
        out = self.work / 'clamp'
        self.native(source, out, implementation=implementation, adapter=adapter)
        self.assertEqual(self.run_cmd(out), EXPECTED)

    def test_lexical_unsafe_and_staging_modes(self):
        source = self.work / 'scope.on1x'
        source.write_text('unsafe import foreign stdffi.clamp as C;\n'
                          'fn main() { println(C.add_i64(40, 2)); }\n')
        self.native(source, self.work / 'invalid', rejected='unsafe')
        self.run_cmd(OPTIONS.on1xc, '--emit-c', '--kernel-c11', '--ffi-stage', '--kernel-stage',
                     '--no-prelude', source, rejected='excludes --kernel-stage')
        source.write_text('unsafe import foreign stdffi.clamp as C;\n'
                          'fn main() { println(unsafe { C.add_i64(40, 2) }); }\n')
        out = self.work / 'scalar'
        self.native(source, out)
        self.assertEqual(self.run_cmd(out), '42\n')

        source.write_text('import foreign stdffi.ipc_framed;\nfn main(){}\n')
        self.native(source, self.work / 'safe-resource', rejected='unsafe-only')
        source.write_text('unsafe import foreign stdffi.ipc_framed;\nfn main(){}\n')
        self.native(source, self.work / 'raw-resource', rejected='no generated resource ABI')

    def test_exact_dso_and_independent_inline(self):
        clamp = (ROOT / 'on1x/examples/kernel/clamp.on1x').read_text()
        source = self.work / 'dyn.on1x'
        source.write_text(clamp.replace('import kernel', 'import kernel[dynlib]', 1))
        out = self.work / 'bundle'
        self.native(source, out)
        self.assertEqual(self.run_cmd(out / 'program'), EXPECTED)
        library = out / 'libon1x_kernels.so'
        report = json.loads((out / 'admission.json').read_text())
        self.assertEqual(report['admission'], 'unsafe')
        self.assertEqual(report['probe'], {'comparisons': 1335, 'passed': True})
        self.assertEqual(report['artifacts'][library.name], hashlib.sha256(library.read_bytes()).hexdigest())
        self.assertIn('DYN (Shared object file)', self.run_cmd('readelf', '-h', library))
        self.assertIn(library.name, self.run_cmd('readelf', '-d', out / 'program'))
        if OPTIONS.cephyr:
            self.native(source, self.work / 'unsupported', implementation='cephyr', adapter='cephyr',
                        rejected='embedded Cephyr DSO provider is not qualified')
        moved = self.work / 'moved'
        shutil.move(out, moved)
        self.assertEqual(self.run_cmd(moved / 'program'), EXPECTED)
        (moved / library.name).unlink()
        self.run_cmd(moved / 'program', rejected=library.name)
        inline = self.work / 'inline.on1x'
        inline.write_text('unsafe import kernel mulib.container_core as C;\n'
                          'fn main() { unsafe { println(C.counter_add(40u64, 2u64));'
                          ' println(C.counter_add(18446744073709551615u64, 1u64)); } }\n')
        plan = self.work / 'plan.json'
        self.native(inline, plan, '--inspect-ffi')
        partition = json.loads(plan.read_text())['partitions']['static']
        self.assertEqual(partition['implementation_macros'], [])
        program = self.work / 'inline'
        self.native(inline, program)
        self.assertEqual(self.run_cmd(program), '42\n18446744073709551615\n')

    def test_review_fresh_subject_and_signed_artifact_drift(self):
        source = ROOT / 'on1x/examples/kernel/clamp_safe.on1x'
        self.native(source, self.work / 'unsigned', rejected='safe import requires independent signed review')
        dynamic = self.work / 'safe-dynamic.on1x'
        dynamic.write_text(source.read_text().replace('import kernel', 'import kernel[dynlib]', 1))
        self.native(dynamic, self.work / 'dynamic-review', '--ffi-review-candidate',
                    rejected='generated safe DSO load policy is not qualified')
        candidate = self.work / 'candidate.json'
        self.native(source, candidate, '--ffi-review-candidate')
        subject = json.loads(candidate.read_text())
        key, public, other, signature, review = [self.work / p for p in
            ('first.pem', 'first.der', 'other.der', 'review.sig', 'review.json')]
        for private, der in ((key, public), (self.work / 'other.pem', other)):
            self.run_cmd('openssl', 'genpkey', '-algorithm', 'ED25519', '-out', private)
            self.run_cmd('openssl', 'pkey', '-in', private, '-pubout', '-outform', 'DER', '-out', der)

        def sign():
            review.write_text(json.dumps(dict(schema='on1x.ffi-review/v1', reviewer='ephemeral test only',
                                             review_reference='native workflows', subject=subject)))
            self.run_cmd('openssl', 'pkeyutl', '-sign', '-inkey', key, '-rawin', '-in', review, '-out', signature)

        def reviewed(name, trust=public, rejected=None):
            out = self.work / name
            self.native(source, out, '--ffi-review', review, '--ffi-review-key', trust,
                        '--ffi-review-signature', signature, rejected=rejected)
            return out

        sign()
        reviewed('wrong-key', other, 'signature verification failed')
        result = reviewed('approved')
        self.assertEqual(self.run_cmd(result / 'program'), EXPECTED)
        for name, digest in subject['artifacts'].items():
            if (result / name).exists():
                self.assertEqual(hashlib.sha256((result / name).read_bytes()).hexdigest(), digest)
        subject['artifacts']['native-abi.o'] = '0' * 64
        sign()
        reviewed('different-artifact', rejected='does not match the current binding inputs')

    def test_project_frozen_transaction(self):
        project = self.work / 'project'
        src = project / 'src'
        src.mkdir(parents=True)
        (project / 'On1xfile').write_text('on1xfile 1\nkind executable\nname "clamp"\n'
                                        'directory "src"\ntarget "clamp" "src/main.on1x"\n')
        (src / 'On1xfile.m').write_text('on1xfile 1\nsource "main.on1x"\n')
        unsafe = ROOT / 'on1x/examples/kernel/clamp.on1x'
        shutil.copyfile(unsafe, src / 'main.on1x')
        command = [sys.executable, '-B', OPTIONS.tools / 'on1x-project.py']
        self.run_cmd(*command, 'lock', project)
        flags = ['--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc, '--mulib-root', ROOT,
                 '--ffi-provider', 'host', '--frozen', *(['--sandbox'] if OPTIONS.sandbox else [])]
        plan = json.loads(self.run_cmd(*command, 'plan', project, *flags))
        self.assertIn('clamp', plan['backend']['ffi_plans'])
        out = Path(self.run_cmd(*command, 'build', project, *flags).strip())
        self.assertEqual(self.run_cmd(out / 'clamp'), EXPECTED)
        build = (out / '.on1x-build.json').read_bytes()
        self.assertEqual(json.loads(build)['ffi_provider'], 'host')
        self.assertIn('clamp.ffi.json', json.loads(build)['outputs'])
        shutil.copyfile(ROOT / 'on1x/examples/kernel/clamp_safe.on1x', src / 'main.on1x')
        self.run_cmd(*command, 'build', project, *flags, rejected='only explicit unsafe static imports')
        self.assertEqual((out / '.on1x-build.json').read_bytes(), build)
        self.assertEqual(self.run_cmd(out / 'clamp'), EXPECTED)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--on1xc', type=Path, required=True)
    parser.add_argument('--tools', type=Path, default=ROOT / 'on1x/tools')
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--cephyr', action='store_true')
    parser.add_argument('--sandbox', action='store_true')
    OPTIONS, remaining = parser.parse_known_args()
    OPTIONS.on1xc = OPTIONS.on1xc.resolve(strict=True)
    OPTIONS.tools = OPTIONS.tools.resolve(strict=True)
    unittest.main(argv=[sys.argv[0], *remaining])
