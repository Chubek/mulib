#!/usr/bin/env python3
"""Local stdio MCP integration against real ArchVirt/KVM, not a VM substitute."""
import argparse
import json
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile
import time

from test_chatgpt import fixture

SERVER = Path(__file__).resolve().with_name("server.py")
SOURCE = """\
extern fn on1x_archvirt_read_i64() -> i64;
fn main() {
    println("ready");
    let a = on1x_archvirt_read_i64();
    println(a);
    let b = on1x_archvirt_read_i64();
    println(a + b);
    let c = on1x_archvirt_read_i64();
    println(c);
}
"""


class Client:
    def __init__(self, args):
        self.process = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        self.id = 0

    def request(self, method, **params):
        self.id += 1
        req = {"jsonrpc": "2.0", "id": self.id, "method": method, "params": params}
        self.process.stdin.write(json.dumps(req).encode() + b"\n")
        self.process.stdin.flush()
        assert select.select([self.process.stdout], [], [], 35)[0], method
        line = self.process.stdout.readline()
        assert line, self.process.poll()
        result = json.loads(line)
        assert result["id"] == self.id, result
        return result

    def init(self):
        assert "result" in self.request(
            "initialize", protocolVersion="2025-11-25", capabilities={}, clientInfo={"name": "test"})

    def call(self, name, code=None, **arguments):
        response = self.request("tools/call", name=name, arguments=arguments)
        result = response["result"]
        if code:
            assert result["isError"], response
            assert json.loads(result["content"][0]["text"])["error"] == code, response
            return json.loads(result["content"][0]["text"])["message"]
        assert not result["isError"], response
        return result["structuredContent"]

    def output(self, identifier, wait_for):
        text = ""
        deadline = time.monotonic() + 5
        while wait_for not in text:
            row = self.call("vm_read", id=identifier)
            assert not row["stderr"], row
            text += row["stdout"]
            assert time.monotonic() < deadline, text
            if wait_for not in text:
                time.sleep(0.02)
        return text

    def close(self, sig=None):
        if self.process.poll() is None:
            if sig:
                self.process.send_signal(sig)
            else:
                self.process.stdin.close()
            self.process.wait(timeout=12)
        if not self.process.stdin.closed:
            self.process.stdin.close()
        error = self.process.stderr.read()
        self.process.stderr.close()
        self.process.stdout.close()
        assert self.process.returncode == (0 if sig != signal.SIGKILL else -signal.SIGKILL), error


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--on1xc", required=True, type=Path)
    p.add_argument("--runner", required=True, type=Path)
    p.add_argument("--exercise-idle", action="store_true",
                   help="also verify a managed guest after the ordinary runner's 60-second idle limit")
    args = p.parse_args()
    with tempfile.TemporaryDirectory(prefix="on1x-mcp-vmm-") as scratch:
        root = Path(scratch)
        fixture(root)
        runtime = root / "runtime"
        runtime.mkdir(mode=0o700)
        command = [sys.executable, "-B", SERVER, "--root", root, "--on1xc",
                   args.on1xc.resolve(), "--archvirt-runner", args.runner.resolve(), "--vmm-root", runtime]
        rejected = subprocess.run(command, capture_output=True, timeout=10)
        assert rejected.returncode == 2 and b"private Linux --stdio" in rejected.stderr, rejected
        client = Client(command + ["--stdio"])
        other = Client(command + ["--stdio"])
        dying = None
        try:
            assert client.request("tools/call", name="vm_list")["error"]["code"] == -32002
            client.init()
            other.init()
            assert {"vm_compile", "vm_send", "vm_read", "vm_kill", "vm_delete", "vm_list",
                    "vm_invalidate", "list_headers"} <= {
                t["name"] for t in client.request("tools/list")["result"]["tools"]}
            assert client.call("list_headers", limit=1)["matches"] == 2
            assert "cannot import" in client.call("vm_compile", "BUILD_FAILED",
                source='import "/etc/passwd"; fn main() {}')
            assert "guest i64 input extern" in client.call("vm_compile", "BUILD_FAILED",
                source="extern fn getpid() -> i64; fn main() { println(getpid()); }")
            client.call("vm_compile", "BUILD_FAILED", source="fn main() { println(false + 1); }")
            assert not list(runtime.glob("*/*")), "failed compile leaked artifacts"
            vm = client.call("vm_compile", source=SOURCE)["id"]
            image = next(runtime.glob(f"*/{vm}/guest.elf"))
            assert image.read_bytes().startswith(b"\x7fELF") and not image.stat().st_mode & 0o111
            assert not list(runtime.rglob("source.on1x")), "source persisted after compilation"
            assert other.call("vm_list") == {"vms": []}
            other.call("vm_send", "VM_NOT_FOUND", id=vm, parameters=["1"])
            assert client.output(vm, "ready\n") == "ready\n"
            client.call("vm_send", "INVALID_ARGUMENT", id=vm, parameters=["9223372036854775808"])
            client.call("vm_send", "INVALID_ARGUMENT", id=vm, parameters=[9])
            assert client.call("vm_send", id=vm, parameters=["9"])["queued"] == 1
            assert client.output(vm, "9\n") == "9\n"
            if args.exercise_idle:
                time.sleep(61)
            assert client.call("vm_send", id=vm, parameters=["5"])["queued"] == 1
            assert client.output(vm, "14\n") == "14\n"
            assert client.call("vm_list")["vms"][0]["state"] == "running"
            assert client.call("vm_kill", id=vm)["state"] == "killed" and image.exists()
            assert client.call("vm_delete", id=vm)["deleted"] and not image.exists()

            busy = client.call("vm_compile", source="fn main() { while true {} }")["id"]
            deadline = time.monotonic() + 9
            errors = ""
            while True:
                row = client.call("vm_read", id=busy)
                errors += row["stderr"]
                if row["exit_status"] == 125 and "active deadline" in errors:
                    break
                assert time.monotonic() < deadline, (row, errors)
                time.sleep(0.03)
            client.call("vm_delete", id=busy)

            client.call("vm_compile", source=SOURCE)
            assert client.call("vm_invalidate") == {"deleted": 1}
            assert client.call("vm_list") == {"vms": []}
            client.call("vm_compile", source=SOURCE)
            other.call("vm_compile", source=SOURCE)
            client.close()
            assert len(list(runtime.glob("*/"))) == 1
            other.close(signal.SIGTERM)
            assert not list(runtime.iterdir()), "session cleanup leaked artifacts"

            # SIGKILL cannot run Python cleanup, but parent-death drops the VM.
            dying = Client(command + ["--stdio"])
            dying.init()
            user_vm = dying.call("vm_compile", source=SOURCE)["id"]
            dying.output(user_vm, "ready\n")
            child = Path(f"/proc/{dying.process.pid}/task/{dying.process.pid}/children")
            runner_pid, = child.read_text().split()
            dying.close(signal.SIGKILL)
            deadline = time.monotonic() + 4
            stat = Path(f"/proc/{runner_pid}/stat")
            while stat.exists() and stat.read_text().split(") ", 1)[1][0] != "Z":
                assert time.monotonic() < deadline, "runner survived MCP parent death"
                time.sleep(0.02)
        finally:
            for peer in (client, other, dying):
                if peer is not None and peer.process.poll() is None:
                    peer.close(signal.SIGTERM)
    print("MCP private session: real KVM compile, send, list, kill, delete, EOF and parent death passed")


if __name__ == "__main__":
    main()
