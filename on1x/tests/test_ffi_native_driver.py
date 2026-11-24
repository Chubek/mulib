"""Native driver component tests. No generated catalogue or source admission claims."""

import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'on1x/tools'))
import on1x_ffi_native as ffi
from on1x_ffi_trust import authorize


def descriptor(provider, member, symbol, n, *, inline=False, atom='long'):
    """A protocol unit-test fixture, not a generated descriptor or approval."""
    module = 'mulib.' + provider.replace('-', '_')
    kind = 'u64' if inline else 'i64'
    primitive = ['integer', atom, 4, 64, 'unsigned' if inline else 'signed', 8, 8]
    linkage, definition = ('internal', 'defined') if inline else ('external', 'declaration')
    native = ['on1x.ffi-native/v2', ffi.TARGET, symbol, linkage, definition, [primitive] * n, primitive]
    fingerprint = hashlib.sha256(ffi.c.dumps(native).strip().encode('ascii')).hexdigest()
    return dict(id=fingerprint, module=module, provider=provider, header='mulib-' + provider + '.h',
                member=member, symbol=symbol, wrapper='on1x_test_' + member, target=list(ffi.TARGET),
                linkage=linkage, definition=definition, native_parameters=[primitive] * n,
                native_result=primitive, native_fingerprint=fingerprint, surface_parameters=[kind] * n,
                surface_result=kind, guards=[], effect='unknown')


def inspection(descriptors, dynamic=()):
    return dict(schema='on1x.ffi-imports/v1', sources=[], descriptors=descriptors,
                requests=[dict(module=module, unsafe=True,
                               **({'linkage': 'dynlib'} if module in dynamic else {}))
                          for module in sorted({d['module'] for d in descriptors})])


class Driver(unittest.TestCase):
    def setUp(self):
        self.addCleanup((directory := tempfile.TemporaryDirectory()).cleanup)
        self.work = Path(directory.name)
        self.cc = os.environ.get('TEST_CC', 'cc')
        self.clamp = descriptor('clamp', 'add_i64', 'mulib_clamp_add_i64', 2)
        self.inline = descriptor('container-core', 'counter_add', 'mulib_ct_counter_add', 2,
                                 inline=True, atom='unsigned-long')

    def compile(self, *args, ok=True):
        result = subprocess.run([self.cc, *ffi.STRICT, '-I', ROOT / 'include', *args], cwd=self.work,
                                capture_output=True, text=True, timeout=45)
        self.assertEqual(result.returncode == 0, ok, result.stderr)
        return result.stderr

    def test_combined_and_partitioned_real_manifest(self):
        doc = inspection([self.clamp, self.inline])
        plans = ffi.plan(ROOT, doc)
        self.assertEqual(set(plans), {'static'})
        self.assertEqual(plans['static']['implementation_macros'], ['MULIB_IMPL_CLAMP'])
        doc['requests'].append(dict(module='mulib.clamp', unsafe=False))
        self.assertEqual(ffi.plan(ROOT, doc)['static']['implementation_macros'], ['MULIB_IMPL_CLAMP'])
        doc['requests'][-1]['linkage'] = 'dynlib'
        with self.assertRaisesRegex(ValueError, 'conflicting'):
            ffi.plan(ROOT, doc)
        split = ffi.plan(ROOT, inspection([self.clamp, self.inline], {'mulib.clamp'}))
        self.assertEqual(split['static']['implementation_macros'], [])
        self.assertEqual(split['dynlib']['implementation_macros'], ['MULIB_IMPL_CLAMP'])
        with self.assertRaisesRegex(ValueError, 'static-inline placement'):
            ffi.plan(ROOT, inspection([self.inline], {'mulib.container_core'}))

    def test_provider_or_target_drift(self):
        for changed, diagnostic in ((dict(target=['aarch64-linux-gnu', 'LP64', 'AAPCS64', 'aapcs64']),
                                     'target/ABI'),
                                    (dict(provider='statsproc'), 'canonical module'),
                                    (dict(native_fingerprint='0' * 64), 'fingerprint'),
                                    (dict(native_result=['enum', 'mulib_priceopt_demand_kind_t']),
                                     'unsupported native type')):
            with self.subTest(changed=changed), self.assertRaisesRegex(ValueError, diagnostic):
                ffi.plan(ROOT, inspection([dict(self.clamp, **changed)]))

    def test_resource_cannot_use_generated_scalar_profile(self):
        fake = descriptor('ipc-framed', 'init', 'mulib_ipc_framed_init', 2)
        for descriptors in ([], [fake]):
            doc = inspection(descriptors)
            doc['requests'] = [dict(module='mulib.ipc_framed', unsafe=True)]
            with self.assertRaisesRegex(ValueError, 'no generated resource ABI'):
                ffi.plan(ROOT, doc)
            doc['requests'][0]['unsafe'] = False
            with self.assertRaisesRegex(ValueError, 'unsafe-only'):
                ffi.plan(ROOT, doc)

    def test_abi_checks_identity_not_just_equal_layout(self):
        abi = self.work / 'abi.c'
        abi.write_text(ffi.abi_source([self.clamp, self.inline]))
        self.compile('-c', abi, '-o', 'probe.o')
        wrong = descriptor('clamp', 'add_i64', 'mulib_clamp_add_i64', 2, atom='long-long')
        abi.write_text(ffi.abi_source([wrong]))
        diagnostic = self.compile('-c', abi, '-o', 'wrong.o', ok=False)
        self.assertIn('FFI prototype: mulib_clamp_add_i64', diagnostic)
        self.assertNotIn('FFI primitive layout', diagnostic)
        self.assertFalse((self.work / 'wrong.o').exists())

    def test_actual_static_boundary_and_missing_symbol(self):
        doc = inspection([self.clamp, self.inline])
        recipe = ffi.plan(ROOT, doc)['static']
        source = self.work / 'implementation.c'
        source.write_text(ffi.house.implementation(ROOT, recipe))
        self.compile('-c', source, '-o', 'implementation.o')
        probe, count = ffi.probe_source(doc['descriptors'])
        self.assertEqual(count, 657)
        (self.work / 'probe.c').write_text(probe)
        (self.work / 'abi.c').write_text(ffi.abi_source(doc['descriptors']))
        # Independent C bridge fixture, not substituted for production lowering.
        (self.work / 'boundary.c').write_text('''
#include <mulib-clamp.h>
#include <mulib-container-core.h>
int64_t on1x_test_add_i64(int64_t a, int64_t b) { return mulib_clamp_add_i64(a, b); }
uint64_t on1x_test_counter_add(uint64_t a, uint64_t b) { return mulib_ct_counter_add(a, b); }
''')
        self.compile('probe.c', 'abi.c', 'boundary.c', 'implementation.o', '-o', 'probe')
        subprocess.run([self.work / 'probe'], check=True, timeout=30)
        diagnostic = self.compile('probe.c', 'abi.c', 'boundary.c', '-o', 'missing', ok=False)
        self.assertIn('mulib_clamp_add_i64', diagnostic)

    def test_review_signature_and_exact_subject(self):
        def openssl(*args):
            subprocess.run(['openssl', *args], cwd=self.work, check=True, capture_output=True, timeout=30)
        for name in ('first', 'other'):
            openssl('genpkey', '-algorithm', 'ED25519', '-out', name + '.pem')
            openssl('pkey', '-in', name + '.pem', '-pubout', '-outform', 'DER', '-out', name + '.der')
        subject = {'schema': 'on1x.ffi-review-subject/v1', 'target': ffi.TARGET, 'artifacts': {'object.o': 'a' * 64}}
        document = dict(schema='on1x.ffi-review/v1', reviewer='test-only ephemeral key',
                        review_reference='native driver unit test', subject=subject)
        (self.work / 'review').write_text(ffi.c.dumps(document))
        openssl('pkeyutl', '-sign', '-inkey', 'first.pem', '-rawin', '-in', 'review', '-out', 'signature')

        def verify(candidate, name, schema='on1x.ffi-review/v1'):
            with tempfile.TemporaryDirectory(dir=self.work) as stage:
                return authorize(candidate, schema=schema, key=self.work / (name + '.der'),
                                 review=self.work / 'review', signature=self.work / 'signature',
                                 stage=Path(stage), codec=ffi.c, diagnostic='FFI: ')

        self.assertEqual(verify(subject, 'first')['subject'], subject)
        with self.assertRaisesRegex(ValueError, 'signature verification failed'):
            verify(subject, 'other')
        with self.assertRaisesRegex(ValueError, 'does not match'):
            verify(dict(subject, artifacts={'object.o': 'b' * 64}), 'first')
        with self.assertRaisesRegex(ValueError, 'invalid independent review schema'):
            verify(subject, 'first', 'on1x.kernel-review/v1')
        (self.work / 'review').write_text(ffi.c.dumps(dict(document, reviewer='tampered')))
        with self.assertRaisesRegex(ValueError, 'signature verification failed'):
            verify(subject, 'first')

    def test_publish_checks_pinned_bytes(self):
        build = ffi.Build.__new__(ffi.Build)
        build.stage = self.work
        build.inputs = ffi.Inputs()
        artifact = self.work / 'object'
        artifact.write_bytes(b'qualified')
        build.inputs.add(artifact)
        output = self.work / 'out'
        copy2 = ffi.shutil.copy2

        def changed_copy(source, destination):
            source.write_bytes(b'changed during copy')
            return copy2(source, destination)

        with patch.object(ffi.shutil, 'copy2', changed_copy):
            with self.assertRaisesRegex(ValueError, 'artifact drift during publication'):
                build.publish(output, ['object'])
        self.assertFalse(output.exists())
        artifact.write_bytes(b'qualified')
        build.publish(output, ['object'])
        self.assertEqual(output.read_bytes(), b'qualified')


if __name__ == '__main__':
    unittest.main()
