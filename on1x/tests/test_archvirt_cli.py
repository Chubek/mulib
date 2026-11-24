"""Exercise the native ArchVirt runner, linked executable, and exact replay."""
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import sys
import tempfile


compiler = Path(sys.argv[1]).resolve()
enabled = sys.argv[2] == "1"


def expect(command, status, *, stdout=None, error=None, env=None, cwd=None):
    p = subprocess.run(command, capture_output=True, timeout=30, env=env, cwd=cwd)
    assert p.returncode == status, (command, p.returncode, p.stdout, p.stderr)
    if stdout is not None:
        assert p.stdout == stdout, (command, p.stdout, p.stderr)
    if error is not None:
        assert error in p.stderr, (command, p.stdout, p.stderr)
    return p


with tempfile.TemporaryDirectory(prefix="on1x-archvirt-") as work:
    root = Path(work)
    env = dict(os.environ, XDG_CACHE_HOME=str(root / "cache"), HOME=str(root),
               DOMVIRT_KERNEL="/no/such/on1x-kernel", ARCHVIRT_MACHINE="not-a-machine")
    source = root / "fixture.on1x"
    source.write_text("fn main() -> i64 { println(42); return 0; }\n")
    elf, always, opt_in = (root / name for name in ("guest.elf", "always", "opt in"))

    def on1xc(*args, status=0, **checks):
        return expect([compiler, *args], status, env=env, cwd=root, **checks)

    if not enabled:
        for args in [
            ("--vm-run", source), ("--vm-build", source, "-o", elf),
            ("--vm-rerun", "0" * 64), ("--vm-cache-clear",),
            ("--vm-launcher=always", source, "-o", always),
            ("--vm-run-elf", elf), ("--print-archvirt-build-inputs",),
        ]:
            on1xc(*args, status=2, stdout=b"", error=b"ArchVirt unavailable")
        assert not (root / "cache").exists() and not elf.exists() and not always.exists()
        sys.exit(0)

    inputs = on1xc("--print-archvirt-build-inputs").stdout.decode().splitlines()
    assert inputs == sorted(set(inputs)) and all(Path(p).is_absolute() and Path(p).is_file() for p in inputs)
    assert {"archvirt_boot.h", "on1x-archvirt-runtime.a", "on1x-archvirt-wrapper"} <= {
        Path(p).name for p in inputs
    }
    elf.write_bytes(b"existing executable")
    elf.chmod(0o755)
    on1xc("--vm-build", source, "-o", elf, stdout=b"")
    assert elf.stat().st_mode & 0o777 == 0o600
    assert not (root / "cache").exists(), "build alone touched the replay cache"
    image = elf.read_bytes()
    assert image[:6] == b"\x7fELF\x02\x01" and struct.unpack_from("<H", image, 16)[0] == 2
    assert struct.unpack_from("<H", image, 18)[0] == {
        "x86_64": 62, "aarch64": 183, "riscv64": 243,
    }[platform.machine()]
    hosted = root / "hosted.elf"
    hosted.write_bytes(Path("/bin/echo").read_bytes())
    on1xc("--vm-run-elf", hosted, status=125, stdout=b"", error=b"not an On1x DomIOS")
    older = root / "older.elf"
    older.write_bytes(image[:-1] + b"z")
    on1xc("--vm-run-elf", older, status=2, stdout=b"", error=b"does not match")
    assert not (root / "cache").exists(), "invalid host executable entered the guest cache"
    p = on1xc("--vm-run-elf", elf, stdout=b"42\n")
    key = re.search(rb"ArchVirt cache key: ([0-9a-f]{64})\b", p.stderr)
    assert key, p.stderr
    key = key[1].decode()
    p = on1xc("--vm-run", source, "-o", elf, stdout=b"42\n")
    assert key.encode() in p.stderr

    on1xc("--vm-launcher=always", source, "-o", always, stdout=b"")
    on1xc("--vm-launcher=opt-in", source, "-o", opt_in, stdout=b"")
    assert always.read_bytes()[:4] == b"\x7fELF" and opt_in.read_bytes()[:4] == b"\x7fELF"
    source.unlink()
    elf.unlink()
    expect([always], 0, stdout=b"42\n", env=env, cwd=root)
    expect([always, "--native"], 2, stdout=b"", error=b"guest arguments", env=env, cwd=root)
    expect([opt_in], 2, stdout=b"", error=b"no native payload", env=env, cwd=root)
    expect([opt_in, "--sandbox"], 0, stdout=b"42\n", env=env, cwd=root)
    on1xc("--vm-rerun", key, stdout=b"42\n")

    source.write_text("fn main() -> i64 { println(43); return 0; }\n")
    p = on1xc("--vm-run", source, stdout=b"43\n")
    assert key.encode() not in p.stderr
    on1xc("--vm-rerun", key, stdout=b"42\n")
    malformed = root / "malformed"
    malformed.write_bytes(always.read_bytes()[:-1])
    malformed.chmod(0o700)
    expect([malformed], 2, stdout=b"", error=b"invalid embedded guest", env=env, cwd=root)
    on1xc("--vm-rerun", "../missing", status=2, stdout=b"")
    unrelated = root / "cache" / "unrelated"
    unrelated.write_bytes(b"do not delete")
    on1xc("--vm-cache-clear", stdout=b"")
    assert unrelated.read_bytes() == b"do not delete"
    on1xc("--vm-rerun", key, status=2, stdout=b"")
    # Linked launchers embed the image; they do not depend on the replay cache.
    expect([always], 0, stdout=b"42\n", env=env, cwd=root)

    source.write_text("fn main() { println(true + 1); }\n")
    on1xc("--vm-build", source, "-o", always, status=1, stdout=b"")
    expect([always], 0, stdout=b"42\n", env=env, cwd=root)
    on1xc("--emit-c", "--vm-run", source, status=2, stdout=b"",
          error=b"cannot be combined")
