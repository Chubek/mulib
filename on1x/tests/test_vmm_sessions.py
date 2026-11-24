"""End-to-end session test, running a real x86-64 ArchVirt KVM guest."""
import os
from pathlib import Path
import re
import secrets
import select
import subprocess
import sys
import tempfile
import time


compiler = Path(sys.argv[1]).resolve()


def receive(fd, marker, timeout=8):
    seen = b""
    deadline = time.monotonic() + timeout
    while marker not in seen:
        remaining = deadline - time.monotonic()
        assert remaining > 0 and select.select([fd], [], [], remaining)[0], (marker, seen)
        data = os.read(fd, 8192)
        assert data, (marker, seen)
        seen += data
    return seen


with tempfile.TemporaryDirectory(prefix="on1x-vmm-kvm-") as tmp:
    root = Path(tmp)
    env = dict(os.environ, HOME=tmp, XDG_CACHE_HOME=str(root / "cache"))
    source = root / "session.on1x"
    boot = "session boot " + secrets.token_hex(8)
    source.write_text("""\
extern fn on1x_archvirt_read_i64() -> i64;
fn main() {
    println("session boot");
    let x = on1x_archvirt_read_i64();
    println(x);
    let y = on1x_archvirt_read_i64();
    println(x + y);
    let z = on1x_archvirt_read_i64();
    println(x + y + z);
    let keep_alive = on1x_archvirt_read_i64();
    println(keep_alive);
}
""".replace("session boot", boot))

    def call(*args, okay=True):
        result = subprocess.run([compiler, *args], env=env, cwd=root, capture_output=True, timeout=15)
        assert (result.returncode == 0) == okay, (args, result.returncode, result.stdout, result.stderr)
        return result

    elf = root / "session.elf"
    emitted = call("--vm-build", source, "-o", elf)
    tag = re.search(rb"ArchVirt VM ID: ([0-9a-f]{64})\b", emitted.stderr)
    assert tag and not emitted.stdout, emitted
    vm = tag[1].decode()
    assert call("vmm", "connect", "../bad", okay=False).stderr.find(b"64-hex VM ID") >= 0
    assert b"signed decimal i64" in call("vmm", "connect", vm, "--detached", "--param",
                                        "9223372036854775808", okay=False).stderr
    # All later operations must be independent of any source, named image or compiler scratch directory.
    source.unlink()
    elf.unlink()
    console = None
    try:
        call("vmm", "connect", vm, "--detached", "--param", "9")
        call("--vm-cache-clear")
        call("vmm", "connect", vm, "--detached", "--param", "5")
        console = subprocess.Popen([compiler, "vmm", "connect", vm], env=env, cwd=root,
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        boot_line = boot.encode() + b"\n"
        history = receive(console.stdout.fileno(), boot_line + b"9\n14\n")
        assert history.count(boot_line) == 1, history
        console.stdin.write(b"send 7\n")
        console.stdin.flush()
        assert receive(console.stdout.fileno(), b"21\n") == b"21\n"
        console.stdin.write(b":quit\n")
        console.stdin.flush()
        output, error = console.communicate(timeout=8)
        assert console.returncode == 0 and not output and not error, (console.returncode, output, error)
        call("vmm", "connect", vm, "--detached")
        call("vmm", "stop", vm)
        assert b"no live session" in call("vmm", "connect", vm, "--detached", okay=False).stderr
    finally:
        if console is not None and console.poll() is None:
            console.stdin.close()
        subprocess.run([compiler, "vmm", "stop", vm], env=env, cwd=root, capture_output=True, timeout=10)
        if console is not None and console.poll() is None:
            console.kill()
            console.wait(timeout=5)
    print("ArchVirt VM ID, detached sends, and live MuLIB console passed on KVM")
