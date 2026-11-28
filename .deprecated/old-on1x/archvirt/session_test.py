"""Host-protocol unit tests. The fixture explicitly substitutes for ArchVirt."""

import argparse
import os
from pathlib import Path
import pty
import secrets
import select
import socket
import subprocess
import sys
import tempfile
import time


FIXTURE = """#!/usr/bin/env python3
import os
from pathlib import Path
import sys
import time

if len(sys.argv) != 3 or sys.argv[1] != "--interactive":
    raise SystemExit(2)
print(f"boot pid={os.getpid()}", flush=True)
for line in sys.stdin:
    # The private staging path may already be unlinked when the guest reads it.
    if Path(sys.argv[2]).read_bytes() != b"pinned image":
        raise SystemExit(3)
    value = int(line)
    print(f"received {value} pid={os.getpid()}", flush=True)
    print("\\\\fixture\\x1b[31m", file=sys.stderr, flush=True)
    if value == 4242:
        raise SystemExit(17)
    if value == 4343:
        sys.stdout.buffer.write(b"x" * (2 * 1024 * 1024))
        sys.stdout.flush()
"""


def require(test, message):
    if not test:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repl", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    script = Path(__file__).resolve().with_name("session.py")
    tool = args.repl.resolve()
    ids = [secrets.token_hex(32) for _ in range(5)]
    base = Path("/tmp") / f"on1x-archvirt-{os.geteuid()}"
    with tempfile.TemporaryDirectory(dir=args.work_dir) as scratch:
        root = Path(scratch)
        runner = root / "fixture_runner"
        runner.write_text(FIXTURE)
        runner.chmod(0o700)
        image = root / "guest image"
        image.write_bytes(b"pinned image")

        def command(*options, stage=None, exe=runner):
            prefix = [sys.executable, "-IB", str(script), "--runner", str(exe), "--repl", str(tool)]
            if stage is not None:
                prefix += ["--image", str(stage)]
            return prefix + list(options)

        def call(*options, stage=None, exe=runner, error=None, data=None):
            proc = subprocess.run(command(*options, stage=stage, exe=exe),
                                  input=data, capture_output=True, text=True, timeout=10)
            if error is None:
                require(proc.returncode == 0, f"{options}: {proc.stderr}")
            else:
                require(proc.returncode != 0 and error in proc.stderr,
                        f"{options}: expected {error!r} but got {proc.stderr!r}")
            return proc

        def console(session):
            master, slave = pty.openpty()
            proc = subprocess.Popen(command("connect", session), stdin=slave, stdout=slave, stderr=slave)
            os.close(slave)
            return master, proc

        def until(fd, marker, timeout=5):
            output = b""
            deadline = time.monotonic() + timeout
            while marker not in output:
                delay = deadline - time.monotonic()
                if delay <= 0 or not select.select([fd], [], [], delay)[0]:
                    raise AssertionError(f"console did not show {marker!r}: {output[-1024:]!r}")
                try:
                    data = os.read(fd, 4096)
                except OSError as err:
                    raise AssertionError(f"console closed before {marker!r}: {output[-1024:]!r}") from err
                if not data:
                    raise AssertionError(f"console ended before {marker!r}: {output[-1024:]!r}")
                output += data
            return output

        sessions = []
        processes = []
        try:
            live, other, corrupt, failed, noisy = ids
            call("connect", live, "--detached", error="no live session")
            call("connect", "../not-a-key", "--detached", error="64 lowercase")
            call("connect", failed, "--detached", stage=image, exe=root / "absent",
                 error="cannot start interactive")
            require(not (base / failed).exists(), "failed startup left discovery state")
            call("connect", live, "--detached", "--param", "9223372036854775808", stage=image,
                 error="signed decimal i64")
            require(not (base / live).exists(), "invalid arguments started a runner")

            call("connect", live, "--detached", "--param", "-9223372036854775808", stage=image)
            sessions.append(live)
            image.unlink()
            call("connect", live, "--detached", "--param", "+9223372036854775807")
            require((base.stat().st_mode & 0o777) == 0o700, "namespace is not private")
            require(((base / live).stat().st_mode & 0o777) == 0o700, "socket is not private")
            master, repl = console(live)
            processes.append((master, repl))
            history = until(master, b"vmm> ")
            if b"received 9223372036854775807" not in history:
                history += until(master, b"received 9223372036854775807")
            require(b"received -9223372036854775808" in history, "lost initial i64")
            require(history.count(b"boot pid=") == 1, "detached send booted another runner")
            require(b"\x1b[31m" not in history, "untrusted terminal escape reached the console")
            os.write(master, b"send \r")
            until(master, b"expected send VALUE")
            os.write(master, b"send 7x\x7f\r")
            last = until(master, b"received 7")
            require(b"\x1b[31m" not in last, "untrusted terminal escape reached the live console")
            os.write(master, b":quit\r")
            require(repl.wait(timeout=5) == 0, ":quit did not detach cleanly")

            # Two callers requesting the same fresh ID must join one launch.
            image.write_bytes(b"pinned image")
            sessions.append(other)
            launches = [subprocess.Popen(command("connect", other, "--detached", "--param", str(n),
                                                 stage=image), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                        for n in (81, 82)]
            for proc in launches:
                _, error = proc.communicate(timeout=10)
                require(proc.returncode == 0, f"concurrent attach: {error!r}")
            master2, repl2 = console(other)
            processes.append((master2, repl2))
            seen = until(master2, b"vmm> ")
            remaining = {b"received 81", b"received 82"} - {m for m in
                         (b"received 81", b"received 82") if m in seen}
            while remaining:
                seen += until(master2, next(iter(remaining)))
                remaining = {marker for marker in remaining if marker not in seen}
            require(seen.count(b"boot pid=") == 1, "concurrent starts booted different runners")
            call("stop", other)
            require(repl2.wait(timeout=5) != 0, "stopping the session left the editor waiting")
            require(not (base / other).exists(), "explicit stop left stale socket")

            # Overlong, malformed and invalid-number packets never reach the runner.
            for payload, response in ((b"S" + b"2" * 4097, b"Einvalid session packet"),
                                      (b"S1\n\x00", b"Einvalid or oversized parameter batch"),
                                      (b"S9223372036854775808\n", b"Eexpected a signed decimal i64")):
                with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as client:
                    client.settimeout(4)
                    client.connect(str(base / live))
                    client.sendall(payload)
                    require(client.recv(4096) == response, f"unexpected response for {payload[:32]!r}")

            # Do not replace a dead session when the original guest bytes are still cached.
            call("connect", live, "--detached", "--param", "4242", stage=image)
            deadline = time.monotonic() + 5
            while True:
                proc = subprocess.run(command("connect", live, "--detached", stage=image),
                                      capture_output=True, text=True, timeout=5)
                if proc.returncode and "session has ended" in proc.stderr:
                    break
                require(time.monotonic() < deadline, f"session never transitioned to stale: {proc.stderr}")
                time.sleep(0.025)
            call("connect", live, "--detached", "--param", "5", stage=image, error="session has ended")
            call("stop", live)
            call("connect", live, "--detached", "--param", "9", stage=image)
            call("stop", live)

            sessions.append(noisy)
            call("connect", noisy, "--detached", "--param", "4343", stage=image)
            deadline = time.monotonic() + 5
            while True:
                proc = subprocess.run(command("connect", noisy, "--detached"),
                                      capture_output=True, text=True, timeout=5)
                if proc.returncode and "session has ended" in proc.stderr:
                    break
                require(time.monotonic() < deadline, f"noisy runner exceeded lifetime: {proc.stderr}")
                time.sleep(0.025)
            call("stop", noisy)

            outside = root / "outside"
            outside.write_text("not a socket")
            (base / corrupt).symlink_to(outside)
            try:
                call("connect", corrupt, "--detached", stage=image, error="not an owner-only socket")
                call("stop", corrupt, error="not an owner-only socket")
                require(outside.read_text() == "not a socket", "followed session endpoint symlink")
            finally:
                (base / corrupt).unlink()
        finally:
            for fd, proc in processes:
                os.close(fd)
                if proc.poll() is None:
                    proc.terminate()
                    proc.wait(timeout=5)
            for session in sessions:
                subprocess.run(command("stop", session), capture_output=True, timeout=10)
    print("ArchVirt host session tests passed (explicit protocol fixture)")


if __name__ == "__main__":
    main()
