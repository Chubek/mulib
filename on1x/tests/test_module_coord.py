"""Isolated Linux POSIX IPC acceptance for the module work-lane coordinator."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import ctypes
import errno
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import uuid


ROLES = (
    "grammar", "modules", "imports", "visibility", "signatures",
    "asdl", "ppx", "stdlib", "integration",
)


def test(coord, work):
    env = {**os.environ, "ON1X_MOD_AUDIT": str(work / "coord.log")}
    session = "coordtest_" + uuid.uuid4().hex
    partial = session + "_partial"

    def invoke(*args, expected=0, prefix=session, config=env):
        p = subprocess.run([coord, prefix, *args], text=True, capture_output=True,
                           timeout=8, env=config)
        assert p.returncode == expected, (args, p.returncode, p.stdout, p.stderr)
        return p.stdout

    libc = ctypes.CDLL(None, use_errno=True)
    libc.mq_close.argtypes = [ctypes.c_int]
    libc.mq_unlink.argtypes = [ctypes.c_char_p]
    existing = f"/on1x_{partial}_visibility".encode()
    held = -1
    try:
        invoke("init")
        invoke("init", expected=1)
        invoke("cleanup", "extra", expected=2)
        assert not invoke("snapshot")
        invoke("contract", "r1: keep legacy wire")
        with ThreadPoolExecutor(len(ROLES)) as executor:
            list(executor.map(lambda role: invoke("state", role, "started"), ROLES))
        assert invoke("snapshot").splitlines() == [
            "contract r1: keep legacy wire", *(role + " started" for role in ROLES),
        ]
        invoke("contract", "invalid\npublish", expected=2)
        invoke("send", "integration", "grammar", "two words", "invalid", expected=2)
        invoke("send", "integration", "grammar", "test", "a" * 640, expected=2)
        invoke("send", "integration", "grammar", "test", "invalid\rtext", expected=2)
        invoke("send", "integration", "grammar", "test", "invalid", "-1", expected=2)
        invoke("send", "integration", "grammar", "test", "invalid", "18446744073709551616", expected=2)
        invoke("send", "integration", "grammar", "test", "invalid", expected=2,
               config={**env, "ON1X_MOD_AUDIT": ""})
        invoke("recv", "grammar", "1ms", expected=2)
        assert not invoke("recv", "grammar", "0")

        ids = [int(invoke("send", "integration", "grammar", "test", f"message {i}", str(i)))
               for i in range(10)]
        then = time.monotonic()
        assert not invoke("send", "integration", "grammar", "test", "overflow", expected=3)
        assert 0.6 < time.monotonic() - then < 5

        # The full role queue doesn't block another receiver or shared-state updates.
        other_id = int(invoke("send", "integration", "imports", "test", "unaffected"))
        assert invoke("recv", "imports", "0") == f"{other_id} 0 integration test unaffected\n"
        invoke("state", "integration", "draining")
        assert invoke("recv", "grammar", "0").splitlines() == [
            f"{message_id} {i} integration test message {i}" for i, message_id in enumerate(ids)
        ]
        assert not invoke("recv", "grammar", "0")
        exact = "x" * 639
        next_id = int(invoke("send", "imports", "grammar", "recovered", exact, str(ids[0])))
        assert invoke("recv", "grammar", "0") == f"{next_id} {ids[0]} imports recovered {exact}\n"

        records = [line.split(maxsplit=7) for line in Path(env["ON1X_MOD_AUDIT"]).read_text().splitlines()]
        sends = {int(row[2]): row[3:] for row in records if row[1] == "send"}
        receives = {int(row[2]): row[3:] for row in records if row[1] == "recv"}
        timeouts = [row for row in records if row[1] == "timeout"]
        assert sends == receives and len(sends) == 12
        assert len(timeouts) == 1 and timeouts[0][4:] == ["integration", "grammar", "test", "overflow"]
        assert int(timeouts[0][2]) not in sends

        # Force init to collide after creating earlier role queues. Rollback
        # must remove only resources created by that init, not the existing queue.
        held = libc.mq_open(ctypes.c_char_p(existing), os.O_RDONLY | os.O_CREAT | os.O_EXCL,
                            ctypes.c_uint(0o600), ctypes.c_void_p())
        assert held >= 0, os.strerror(ctypes.get_errno())
        invoke("init", prefix=partial, expected=1)
        fd = libc.mq_open(ctypes.c_char_p(existing), os.O_RDONLY)
        assert fd >= 0, os.strerror(ctypes.get_errno())
        assert libc.mq_close(fd) == 0
        assert libc.mq_unlink(existing) == 0
        invoke("init", prefix=partial)
        invoke("cleanup", prefix=partial)
        invoke("snapshot", expected=1, prefix=partial)

        invoke("cleanup")
        invoke("cleanup")
        invoke("snapshot", expected=1)
        for role in ROLES:
            fd = libc.mq_open(ctypes.c_char_p(f"/on1x_{session}_{role}".encode()), os.O_RDONLY)
            assert fd == -1 and ctypes.get_errno() == errno.ENOENT, role
        invoke("init")
        assert not invoke("snapshot")
    finally:
        if held >= 0:
            libc.mq_close(held)
        invoke("cleanup", prefix=partial)
        invoke("cleanup")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("coordinator", type=lambda value: str(Path(value).resolve()))
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("test requires Linux POSIX message queues and shared memory")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="coord-", dir=args.work_dir) as directory:
        test(args.coordinator, Path(directory))
    print("Module coordination: audit, queue saturation/recovery, init rollback, cleanup: PASS")


if __name__ == "__main__":
    main()
