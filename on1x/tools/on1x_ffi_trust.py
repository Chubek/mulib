"""Verify independently signed native FFI review evidence, not DSL claims."""

import hashlib
from pathlib import Path
import shutil
import subprocess


def authorize(expected_subject, *, schema, key, review, signature, stage, codec, diagnostic):
    """Match exact signed bytes to a freshly computed, schema-specific subject.

    ``codec`` is the existing bounded, duplicate-rejecting JSON implementation.
    A candidate can be a callback to preserve the compatibility CLI's evaluation
    order. No caller may reuse an old subject in place of recomputing it.
    """
    def reject(reason):
        raise ValueError(diagnostic + reason)

    raw = codec.read(review, 1024 * 1024)
    public = codec.read(key, 1024)
    sig = codec.read(signature, 1024)
    if len(public) != 44 or not public.startswith(bytes.fromhex('302a300506032b6570032100')) or len(sig) != 64:
        reject('expected an Ed25519 DER public key and 64-byte detached signature')
    evidence = codec.loads(raw)
    if (not isinstance(evidence, dict) or set(evidence) != {'schema', 'reviewer', 'review_reference', 'subject'}
            or evidence['schema'] != schema or not isinstance(evidence['subject'], dict)
            or not all(isinstance(evidence[k], str) and 0 < len(evidence[k].strip()) <= 1024
                       for k in ('reviewer', 'review_reference'))):
        reject('invalid independent review schema')
    for name, data in [('review.ed25519.der', public), ('review.json', raw), ('review.sig', sig)]:
        with (stage / name).open('xb') as stream:
            stream.write(data)
    openssl = shutil.which('openssl')
    if not openssl:
        reject('OpenSSL verification tool unavailable')
    openssl = Path(openssl).resolve(strict=True)
    digest = codec.digest(openssl)
    verified = subprocess.run([openssl, 'pkeyutl', '-verify', '-pubin', '-keyform', 'DER',
                               '-inkey', stage / 'review.ed25519.der', '-rawin', '-in', stage / 'review.json',
                               '-sigfile', stage / 'review.sig'], capture_output=True, timeout=30)
    if verified.returncode or codec.digest(openssl) != digest:
        reject('independent review signature verification failed')
    if callable(expected_subject):
        expected_subject = expected_subject()
    if codec.dumps(evidence['subject']) != codec.dumps(expected_subject):
        reject('independent review does not match the current binding inputs')
    return dict(**evidence, signature_hex=sig.hex(), key_der_hex=public.hex(),
                review_sha256=hashlib.sha256(raw).hexdigest(), signed_review_hex=raw.hex(),
                verifier=str(openssl), verifier_sha256=digest)
