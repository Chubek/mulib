"""Exercise real On1x C11 output and HTTP/TLS over loopback only."""
from pathlib import Path
import os
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import threading

HERE = Path(__file__).resolve().parent
ON1X = HERE.parents[2]
ROOT = ON1X.parent
sys.path.insert(0, str(ROOT / "tests"))
from test_tls_integration import certificate, root


def run(*args, **kwargs):
    return subprocess.run(args, timeout=30, check=True, **kwargs)


def serve(listener, wire, method, cert, abrupt, errors):
    try:
        raw, _ = listener.accept()
        with raw:
            raw.settimeout(4)
            if cert:
                ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_3
                ctx.set_alpn_protocols(["http/1.1"])
                ctx.set_ecdh_curve("X25519")
                ctx.load_cert_chain(*map(str, cert))
                with ctx.wrap_socket(raw, server_side=True) as tls:
                    assert tls.selected_alpn_protocol() == "http/1.1"
                    reply(tls, wire, method)
                    if not abrupt:
                        with tls.unwrap():  # send close_notify, then close TCP
                            pass
            else:
                reply(raw, wire, method)
    except (OSError, AssertionError) as exc:
        errors.append(exc)


def reply(stream, wire, method):
    request = b""
    while not request.endswith(b"\r\n\r\n"):
        chunk = stream.recv(4096)
        assert chunk and len(request) + len(chunk) <= 4096
        request += chunk
    assert request.startswith(method + b" /test?q=1 HTTP/1.1\r\n"), request
    if wire is None:
        # Send nothing; the approved wait must expire and close the socket.
        assert stream.recv(1) == b""
    else:
        stream.sendall(wire)


def parse(proc):
    assert proc.returncode == 0, proc
    lines = proc.stdout.split(b"\n")
    assert len(lines) == 9 and lines[8] == b"", proc
    return lines[:8]


def failure(proc, error):
    assert parse(proc) == [str(error).encode(), b"0", b"false", b"false",
                           b"", b"false", b"", b"-1"], proc


def success(proc, status, headers, body, byte1):
    assert parse(proc) == [
        b"0", str(status).encode(), b"true" if 200 <= status < 300 else b"false",
        b"true" if headers else b"false", b"Value:yes" if headers else b"",
        b"true" if headers else b"false", body, str(byte1).encode()], proc


def main(compiler, cc, *flags):
    runtime = ON1X / "runtime"
    sources = [HERE.parent / "runtime.c", HERE.parent / "kernels.c", HERE / "host.c"]
    cflags = ["-std=c11", "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra",
              "-Werror", "-I", ROOT / "kernels", "-I", runtime,
              *(f.removeprefix("--cflag=") for f in flags)]
    fields = b"X-Mixed: Value:yes\r\nx-mixed: ignored\r\nX-Empty:\r\n"
    chunked = (b"HTTP/1.1 404 Not Found\r\n" + fields +
               b"Transfer-Encoding: chunked\r\n\r\n3;x=z\r\nA\x00\xff\r\n0\r\nY-Trailer: not-mixed\r\n\r\n")
    head_response = b"HTTP/1.1 200 OK\r\n" + fields + b"Content-Length: 999999\r\n\r\n"
    with tempfile.TemporaryDirectory(prefix="on1x-http-") as tmp:
        work = Path(tmp)
        generated, exe, alloc_exe = work / "client.c", work / "client", work / "oom"
        pure, pure_exe = work / "pure.c", work / "pure"
        run(compiler, "--emit-c", HERE / "pure.on1x", "-o", pure)
        run(cc, *cflags, pure, runtime / "on1x_runtime.c", "-o", pure_exe)
        run(pure_exe)
        run(compiler, "--emit-c", HERE / "client.on1x", "-o", generated)
        link = [cc, *cflags, generated, runtime / "on1x_runtime.c", *sources]
        run(*link, "-o", exe)
        # Any native result allocations must be gone before runtime snapshots.
        run(*link, "-include", HERE / "alloc.h", "-DON1X_DATA_MALLOC=on1x_http_test_alloc",
            "-o", alloc_exe)
        if sys.platform.startswith("linux"):
            missing = subprocess.run(
                [*link[:-1], "-o", work / "missing"], capture_output=True, timeout=30)
            assert missing.returncode != 0 and b"on1x_http_authorize_v1" in missing.stderr, missing

        env = dict(os.environ)
        for var in ("ON1X_TEST_PORT", "ON1X_TEST_CA"):
            env.pop(var, None)

        def request(url, **kw):
            return subprocess.run([kw.pop("binary", exe)], input=url.encode(),
                                  env={**env, **kw}, capture_output=True, timeout=8)

        for url in ("", "file:///path", "http://localhost/test", "http://127.1/",
                    "http://[::1]/", "http://127.0.0.1@127.0.0.1/", "http://127.0.0.1/#x",
                    "http://127.0.0.1:0/", "http://127.0.0.1:65536/",
                    "http://127.0.0.1/" + "a" * 2048):
            invalid = request(url)
            failure(invalid, -1)
            assert not invalid.stderr, invalid

        # A listening endpoint proves denial and invalid grants do not dial.
        with socket.create_server(("127.0.0.1", 0)) as listener:
            port = listener.getsockname()[1]
            for address, path, authority in (
                    ("127.0.0.1", "/test", {}),
                    ("127.0.0.1", "/deny", {"ON1X_TEST_PORT": str(port)}),
                    ("127.0.0.1", "/bad-grant", {"ON1X_TEST_PORT": str(port)}),
                    ("127.0.0.2", "/test", {"ON1X_TEST_PORT": str(port)})):
                denied = request(f"http://{address}:{port}{path}", **authority)
                failure(denied, -5)
                assert denied.stderr == f"auth 0 0 {port}\n".encode(), denied
            no_trust = request(f"https://127.0.0.1:{port}/test", ON1X_TEST_PORT=str(port))
            failure(no_trust, -5)
            assert no_trust.stderr == f"auth 1 0 {port}\n".encode(), no_trust
            listener.setblocking(False)
            try:
                conn, _ = listener.accept()
            except BlockingIOError:
                pass
            else:
                conn.close()
                raise AssertionError("a denied request connected")

        def transact(wire, *, head=False, cert=None, ca=None, abrupt=False,
                     failing_tls=False, binary=exe):
            errors = []
            with socket.create_server(("127.0.0.1", 0)) as listener:
                listener.settimeout(5)
                port = listener.getsockname()[1]
                server = threading.Thread(target=serve, args=(
                    listener, wire, b"HEAD" if head else b"GET", cert, abrupt, errors))
                server.start()
                try:
                    url = ("H" if head else "") + ("https" if cert else "http")
                    call_env = {"ON1X_TEST_PORT": str(port)}
                    if ca:
                        call_env["ON1X_TEST_CA"] = str(ca)
                    result = request(url + f"://127.0.0.1:{port}/test?q=1", binary=binary,
                                     **call_env)
                finally:
                    server.join(6)
                assert not server.is_alive()
                if not failing_tls and cert and not abrupt:
                    # The existing TLS client accepts close_notify but may then
                    # close the descriptor without reciprocating it.
                    errors = [e for e in errors if not isinstance(e, ssl.SSLEOFError)]
                if not failing_tls:
                    assert not errors, errors
                elif errors:
                    assert all(isinstance(e, (ssl.SSLError, ConnectionError)) for e in errors), errors
                line = f"auth {int(bool(cert))} {int(head)} {port}\n".encode()
                assert result.stderr.startswith(line) and result.stderr.count(b"auth ") == 1, result
                return result

        success(transact(chunked), 404, True, b"4100ff", 0)
        success(transact(head_response, head=True), 200, True, b"", -1)
        success(transact(b"HTTP/1.1 302 Found\r\nLocation: /other\r\nContent-Length: 0\r\n\r\n"),
                302, False, b"", -1)
        success(transact(b"HTTP/1.1 103 Early Hints\r\n\r\n" +
                         b"HTTP/1.1 200 OK\r\n\r\n\xff\x80"),
                200, False, b"ff80", 128)
        failure(transact(b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nx"), -1)
        failure(transact(b"HTTP/1.1 200 OK\r\nContent-Length: 32769\r\n\r\n" + b"a" * 32769), -2)
        failure(transact(b"HTTP/1.1 200 OK\r\nX-Large: " + b"x" * 8192 + b"\r\n\r\n"), -2)
        # The raw HTTP byte budget counts informational responses too, before parsing.
        failure(transact(b"x" * 65537), -2)
        failure(transact(None), -4)
        exhausted = transact(chunked, binary=alloc_exe)
        assert (exhausted.returncode == 73 and b"text allocation failure" in exhausted.stderr
                and b"LeakSanitizer" not in exhausted.stderr), exhausted

        if not ssl.HAS_TLSv1_3 or not shutil.which("openssl"):
            print("loopback HTTPS oracle unavailable (need Python TLS 1.3 and openssl)",
                  file=sys.stderr)
            return 77
        trusted = root(work, "http-root")
        server = certificate(work, "http-server", trusted)
        wrong_host = certificate(work, "wrong-host", trusted, san="DNS:localhost")
        other_root = root(work, "other-root")
        success(transact(chunked, cert=(server[0], server[1]), ca=trusted[0]), 404, True, b"4100ff", 0)
        failure(transact(chunked, cert=(server[0], server[1]), ca=trusted[0], abrupt=True), -4)
        failure(transact(chunked, cert=(server[0], server[1]), ca=other_root[0], failing_tls=True), -4)
        failure(transact(chunked, cert=(wrong_host[0], wrong_host[1]), ca=trusted[0], failing_tls=True), -4)


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
