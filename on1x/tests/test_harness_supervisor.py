"""The trusted host selects resources; data requests cannot widen grants."""
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from on1x_harness import Denied, Limit, Supervisor, Tool

CC = sys.argv.pop(1) if len(sys.argv) > 1 else "cc"


class SupervisorTest(unittest.TestCase):
    def test_read_policy_and_budgets(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            root = base / "root"
            root.mkdir()
            (root / "data").write_bytes(b"\0abc")
            (base / "other").write_text("outside")
            (root / "link").symlink_to(base)
            (root / "filelink").symlink_to(base / "other")
            os.mkfifo(root / "fifo")
            with Supervisor(roots={"source": root}, tools={}, calls=11, output_bytes=6,
                            per_call_bytes=4, seconds=10, per_tool_seconds=1) as host:
                # Authorization refers to the pinned root, not its replaceable name.
                root.rename(base / "pinned")
                root.mkdir()
                (root / "data").write_text("fake")

                def request(path, name="source"):
                    return {"op": "read", "path": path, "root": name}

                for bad in ("../other", "/etc/passwd", "link/other", "filelink",
                            "data\0", "fifo", "."):
                    with self.assertRaises(Denied):
                        host.dispatch(request(bad))
                with self.assertRaises(Denied):
                    host.dispatch(request("data", "ungranted"))
                with self.assertRaises(Denied):
                    host.dispatch({"op": "connect", "host": "localhost"})
                self.assertEqual(host.dispatch(request("data")), b"\0abc")
                with self.assertRaises(Limit):
                    host.dispatch(request("data"))
                with self.assertRaisesRegex(Limit, "session budget"):
                    host.dispatch(request("data"))
            with self.assertRaisesRegex(Denied, "closed"):
                host.dispatch(request("data"))

    def test_real_isolated_tool(self):
        launcher = shutil.which("bwrap")
        if not launcher:
            self.skipTest("bubblewrap unavailable")
        with tempfile.TemporaryDirectory() as directory, socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            image = Path(directory) / "image"
            image.mkdir()
            result = subprocess.run([CC, "-std=c11", "-static", "-x", "c", "-",
                                     "-o", image / "probe"], input=r"""
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
int main(int argc, char **argv) {
    if (argc != 2) return 1;
    if (!strcmp(argv[1], "budget")) { puts("output exceeds limit"); return 0; }
    if (!strcmp(argv[1], "wait")) { sleep(10); return 1; }
    const char *value = getenv("VISIBLE");
    if (!value || getenv("ON1X_HOST_ONLY_TEST")) return 2;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(argv[1]));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0 || connect(fd, (void *)&address, sizeof address) == 0) return 3;
    close(fd);
    puts(value);
    return 0;
}
""", text=True, capture_output=True, timeout=30)
            if result.returncode and ("cannot find -lc" in result.stderr or
                                      "cannot find crt" in result.stderr):
                self.skipTest("static C development libraries unavailable")
            self.assertEqual(result.returncode, 0, result.stderr)
            probe = subprocess.run([launcher, "--unshare-user", "--disable-userns",
                                    "--unshare-net", "--unshare-pid", "--ro-bind",
                                    image, "/", "--", "/probe", "budget"],
                                   capture_output=True, timeout=5)
            if probe.returncode and probe.stderr.startswith(b"bwrap:"):
                self.skipTest(probe.stderr.decode().strip())
            self.assertEqual(probe.returncode, 0, probe.stderr)
            tools = {
                "fixed": Tool(image, ("/probe", str(listener.getsockname()[1])), {"VISIBLE": "yes"}),
                "overflow": Tool(image, ("/probe", "budget")),
                "delay": Tool(image, ("/probe", "wait")),
            }
            budgets = dict(calls=10, output_bytes=16, per_call_bytes=8,
                           seconds=5, per_tool_seconds=1)
            with Supervisor(roots={}, tools=tools, **budgets) as disabled:
                with self.assertRaises(Denied):
                    disabled.dispatch({"op": "run", "tool": "fixed"})
            with Supervisor(roots={}, tools=tools, launcher=launcher, **budgets) as host:
                for extra in ({"argv": "/bin/sh"}, {"environment": "VISIBLE=no"}, {"cwd": "/"}):
                    with self.assertRaises(Denied):
                        host.dispatch({"op": "run", "tool": "fixed", **extra})
                with self.assertRaises(Denied):
                    host.dispatch({"op": "run", "tool": "/probe"})
                os.environ["ON1X_HOST_ONLY_TEST"] = "secret"
                try:
                    self.assertEqual(host.dispatch({"op": "run", "tool": "fixed"}), b"yes\n")
                finally:
                    del os.environ["ON1X_HOST_ONLY_TEST"]
                with self.assertRaisesRegex(Limit, "output"):
                    host.dispatch({"op": "run", "tool": "overflow"})
                with self.assertRaisesRegex(Limit, "deadline"):
                    host.dispatch({"op": "run", "tool": "delay"})


if __name__ == "__main__":
    unittest.main()
