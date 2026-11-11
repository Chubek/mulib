#!/usr/bin/env python3
"""Independent OpenSSL/Python TLS oracle. Runtime binaries link only MuLIB.

All traffic stays on loopback; certificates and private test keys are ephemeral.
"""
import argparse
import base64
import pathlib
import socket
import ssl
import subprocess
import tempfile
import threading


def command(*args):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def key(directory, name, algorithm):
    path = directory / (name + '.key')
    if algorithm == 'rsa':
        command('openssl', 'genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048', '-out', str(path))
    else:
        command('openssl', 'genpkey', '-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:' + algorithm, '-out', str(path))
    return path


def root(directory, name, algorithm='rsa', constraints='CA:TRUE,pathlen:1'):
    private = key(directory, name, algorithm)
    cert = directory / (name + '.pem')
    command('openssl', 'req', '-new', '-x509', '-key', str(private), '-out', str(cert), '-days', '2',
            '-sha384' if algorithm == 'secp384r1' else '-sha256', '-subj', '/CN=' + name,
            '-addext', 'basicConstraints=critical,' + constraints,
            '-addext', 'keyUsage=critical,keyCertSign,cRLSign')
    return cert, private


def certificate(directory, name, issuer, algorithm='rsa', san='DNS:localhost,IP:127.0.0.1',
                days='1', eku='serverAuth', ca=False, pss=False, extra=''):
    private = key(directory, name, algorithm)
    csr, cert, extensions = [directory / (name + suffix) for suffix in ('.csr', '.pem', '.ext')]
    command('openssl', 'req', '-new', '-key', str(private), '-out', str(csr), '-subj', '/CN=ignored.example')
    extensions.write_text('basicConstraints=critical,' + ('CA:TRUE,pathlen:0' if ca else 'CA:FALSE') + '\n'
                          + 'keyUsage=critical,' + ('keyCertSign,cRLSign' if ca else 'digitalSignature') + '\n'
                          + 'extendedKeyUsage=' + eku + '\nsubjectAltName=' + san + '\n' + extra)
    args = ['openssl', 'x509', '-req', '-in', str(csr), '-CA', str(issuer[0]), '-CAkey', str(issuer[1]),
            '-set_serial', str(len(list(directory.iterdir())) + 100), '-days', days, '-sha384' if algorithm == 'secp384r1' else '-sha256',
            '-extfile', str(extensions), '-out', str(cert)]
    if pss:
        args += ['-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:digest']
    if days == '-1':
        database, serial, config = [directory / (name + suffix) for suffix in ('.db', '.serial', '.cfg')]
        database.write_text('')
        serial.write_text('01\n')
        config.write_text(f'[ca]\ndefault_ca=issuer\n[issuer]\ndatabase={database}\nserial={serial}\n'
                          f'new_certs_dir={directory}\ncertificate={issuer[0]}\nprivate_key={issuer[1]}\n'
                          'default_md=sha256\npolicy=policy\n[policy]\ncommonName=supplied\n')
        extensions.write_text('[cert]\n' + extensions.read_text())
        args = ['openssl', 'ca', '-batch', '-config', str(config), '-in', str(csr), '-out', str(cert),
                '-startdate', '20000101000000Z', '-enddate', '20010101000000Z', '-notext',
                '-extfile', str(extensions), '-extensions', 'cert']
    command(*args)
    return cert, private


def serve(listener, cert, errors, abrupt=False, tls12=False, fragment=False, tamper=False, client_auth=False):
    """Memory BIO allows fragmenting/corrupting records without changing the oracle."""
    try:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2 if tls12 else ssl.TLSVersion.TLSv1_3
        ctx.load_cert_chain(*map(str, cert))
        ctx.set_alpn_protocols(['http/1.1'])
        ctx.set_ecdh_curve('X25519')
        if client_auth:
            ctx.verify_mode = ssl.CERT_OPTIONAL
        raw, _ = listener.accept()
        with raw:
            raw.settimeout(8)
            incoming, outgoing = ssl.MemoryBIO(), ssl.MemoryBIO()
            tls = ctx.wrap_bio(incoming, outgoing, server_side=True)
            mutated = False

            def flush():
                nonlocal mutated
                wire = outgoing.read()
                if tamper and not mutated:
                    pos = 0
                    while pos + 5 <= len(wire):
                        size = int.from_bytes(wire[pos + 3:pos + 5], 'big') + 5
                        if wire[pos] == 23:
                            data = bytearray(wire)
                            data[pos + size - 1] ^= 1
                            wire = bytes(data)
                            mutated = True
                            break
                        pos += size
                step = 7 if fragment else max(1, len(wire))
                for offset in range(0, len(wire), step):
                    raw.sendall(wire[offset:offset + step])

            def drive(fn):
                while True:
                    try:
                        result = fn()
                        flush()
                        return result
                    except ssl.SSLWantReadError:
                        flush()
                        data = raw.recv(65536)
                        if not data:
                            raise EOFError()
                        incoming.write(data)

            drive(tls.do_handshake)
            request = b''
            while b'\r\n\r\n' not in request:
                request += drive(lambda: tls.read(16384))
            assert tls12 or tls.cipher()[0] == 'TLS_AES_128_GCM_SHA256'
            assert tls.selected_alpn_protocol() == 'http/1.1'
            body = b'MuLIB HTTPS\x00' + bytes(range(256)) * 140
            reply = b'HTTP/1.1 200 OK\r\nContent-Length: ' + str(len(body)).encode() + b'\r\nConnection: close\r\n\r\n' + body
            for off in range(0, len(reply), 3000):
                drive(lambda off=off: tls.write(reply[off:off + 3000]))
            if not abrupt:
                drive(tls.unwrap)
    except (ssl.SSLError, ConnectionError, EOFError, TimeoutError, OSError) as exc:
        errors.append(exc)
    except BaseException as exc:
        errors.append(exc)
    finally:
        listener.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--muw3', default='build/muw3')
    parser.add_argument('--x509', default='build/test-x509')
    parser.add_argument('--signature', default='build/test-signature')
    args = parser.parse_args()
    muw3, x509 = str(pathlib.Path(args.muw3).resolve()), str(pathlib.Path(args.x509).resolve())
    verifier = str(pathlib.Path(args.signature).resolve())
    with tempfile.TemporaryDirectory(prefix='mulib-tls-', dir='/tmp/opencode' if pathlib.Path('/tmp/opencode').is_dir() else None) as temp:
        directory = pathlib.Path(temp)
        rsa = root(directory, 'rsa-root')
        ec = root(directory, 'ec-root', 'prime256v1')
        ec384 = root(directory, 'ec384-root', 'secp384r1')
        valid = certificate(directory, 'valid', rsa)
        weblet = directory / 'https.lua'
        weblet.write_text("local w = require('lmuw3'); local r = w.get(arg[1]); r:expect(200); io.write(r.body)\n")
        altered_der = bytearray(ssl.PEM_cert_to_DER_cert(valid[0].read_text()))
        altered_der[-1] ^= 1
        altered = directory / 'altered.pem'
        altered.write_text('-----BEGIN CERTIFICATE-----\n' + base64.encodebytes(altered_der).decode()
                           + '-----END CERTIFICATE-----\n')
        message = directory / 'message'
        message.write_bytes(b'MuLIB signature oracle')
        for cert, scheme, digest, pss in [(rsa, '0401', 'sha256', False), (rsa, '0501', 'sha384', False),
                                         (rsa, '0601', 'sha512', False), (rsa, '0804', 'sha256', True),
                                         (rsa, '0805', 'sha384', True), (rsa, '0806', 'sha512', True),
                                         (ec, '0403', 'sha256', False), (ec384, '0503', 'sha384', False)]:
            signature = directory / 'signature'
            sign = ['openssl', 'dgst', '-' + digest, '-sign', str(cert[1]), '-out', str(signature)]
            if pss:
                sign += ['-sigopt', 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:digest']
            command(*(sign + [str(message)]))
            command(verifier, str(cert[0]), str(signature), scheme)
            if pss:
                sign[-1] = 'rsa_pss_saltlen:0'
                command(*(sign + [str(message)]))
                assert subprocess.run([verifier, str(cert[0]), str(signature), scheme], capture_output=True).returncode != 0
        print('RSA PKCS1/PSS SHA256/384/512 and ECDSA signature oracles: passed')
        cases = [
            ('RSA', valid, rsa, True, {}),
            ('HTTPS-Weblet', valid, rsa, True, {'weblet': True}),
            ('IP-SAN', valid, rsa, True, {'ip_host': True}),
            ('fragmented', valid, rsa, True, {'fragment': True}),
            ('optional-client-auth', valid, rsa, True, {'client_auth': True}),
            ('ECDSA-P256', certificate(directory, 'ec', ec, 'prime256v1'), ec, True, {}),
            ('ECDSA-P384', certificate(directory, 'ec384', ec384, 'secp384r1'), ec384, True, {}),
            ('RSA-PSS-cert', certificate(directory, 'pss', rsa, pss=True), rsa, True, {}),
            ('wrong-host', certificate(directory, 'wrong', rsa, san='DNS:other.example'), rsa, False, {}),
            ('expired', certificate(directory, 'expired', rsa, days='-1'), rsa, False, {}),
            ('wrong-EKU', certificate(directory, 'eku', rsa, eku='clientAuth'), rsa, False, {}),
            ('unknown-critical', certificate(directory, 'critical', rsa, extra='1.2.3.4=critical,DER:05:00\n'), rsa, False, {}),
            ('untrusted', valid, ec, False, {}),
            ('bad-certificate-signature', (altered, valid[1]), rsa, False, {}),
            ('truncated-close', valid, rsa, False, {'abrupt': True}),
            ('tampered-record', valid, rsa, False, {'tamper': True}),
            ('TLS1.2', valid, rsa, False, {'tls12': True}),
        ]
        intermediate = certificate(directory, 'intermediate', rsa, 'prime256v1', ca=True)
        chained = certificate(directory, 'chained', intermediate, 'prime256v1')
        chainfile = directory / 'chain.pem'
        chainfile.write_bytes(chained[0].read_bytes() + intermediate[0].read_bytes())
        cases.append(('intermediate', (chainfile, chained[1]), rsa, True, {}))
        constrained = root(directory, 'constrained', constraints='CA:TRUE,pathlen:0')
        badinter = certificate(directory, 'badinter', constrained, ca=True)
        badleaf = certificate(directory, 'badleaf', badinter)
        badchain = directory / 'badchain.pem'
        badchain.write_bytes(badleaf[0].read_bytes() + badinter[0].read_bytes())
        cases.append(('path-length', (badchain, badleaf[1]), constrained, False, {}))
        for name, cert, trust, success, options in cases:
            if not options:
                result = subprocess.run([x509, str(trust[0]), str(cert[0]), 'localhost'], capture_output=True, timeout=20)
                assert (result.returncode == 0) == success, (name, 'X509', result.returncode, result.stderr)
            listener = socket.socket()
            listener.bind(('127.0.0.1', 0))
            listener.listen()
            listener.settimeout(10)
            port = listener.getsockname()[1]
            errors = []
            options = options.copy()
            use_weblet, ip_host = options.pop('weblet', False), options.pop('ip_host', False)
            thread = threading.Thread(target=serve, args=(listener, cert, errors), kwargs=options)
            thread.start()
            client_args = [muw3, '--cacert', str(trust[0]), '--timeout', '3000']
            if use_weblet:
                client_args += ['--weblet', str(weblet)]
            client_args += [f'https://{"127.0.0.1" if ip_host else "localhost"}:{port}/']
            result = subprocess.run(client_args, capture_output=True, timeout=20)
            thread.join(12)
            assert not thread.is_alive(), name
            assert (result.returncode == 0) == success, (name, result.returncode, result.stderr, errors)
            if success:
                assert not errors, (name, errors)
                assert result.stdout == b'MuLIB HTTPS\x00' + bytes(range(256)) * 140, name
            else:
                assert not result.stdout, (name, 'released unauthenticated response')
            print(name + ': passed')
        # SAN wildcard and IP matching are tested independently of DNS routing.
        wildcard = certificate(directory, 'wildcard', rsa, san='DNS:*.example.test,IP:127.0.0.1')
        for host, expected in [('www.example.test', True), ('WWW.EXAMPLE.TEST', True),
                               ('example.test', False), ('a.b.example.test', False),
                               ('ignored.example', False), ('127.0.0.1', True), ('127.0.0.2', False)]:
            result = subprocess.run([x509, str(rsa[0]), str(wildcard[0]), host], capture_output=True, timeout=20)
            assert (result.returncode == 0) == expected, host
    print('TLS 1.3 / HTTPS independent interoperability and rejection tests passed')


if __name__ == '__main__':
    main()
