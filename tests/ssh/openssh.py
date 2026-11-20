#!/usr/bin/env python3
"""Bidirectional SSH fixture on loopback; uses disposable host and user keys."""
import argparse
import base64
import os
from pathlib import Path
import pwd
import shutil
import socket
import struct
import subprocess
import tempfile


COMMAND = "printf mulib-ssh-interop; printf mulib-ssh-stderr >&2"
STDOUT = b"mulib-ssh-interop"
STDERR = b"mulib-ssh-stderr"
SUITE = [
    "KexAlgorithms=curve25519-sha256",
    "HostKeyAlgorithms=ssh-ed25519",
    "PubkeyAcceptedAlgorithms=ssh-ed25519",
    "Ciphers=aes128-gcm@openssh.com",
]


def key_blob(raw_key):
    name = b"ssh-ed25519"
    blob = b"".join(struct.pack(">I", len(s)) + s for s in (name, raw_key))
    return "ssh-ed25519 " + base64.b64encode(blob).decode("ascii")


def raw_key(public_line):
    blob = base64.b64decode(public_line.split()[1], validate=True)
    if len(blob) != 51 or not blob.startswith(b"\0\0\0\x0bssh-ed25519\0\0\0\x20"):
        raise ValueError("Expected one raw Ed25519 key")
    return blob[-32:].hex()


def finish(proc):
    try:
        out, err = proc.communicate(timeout=15)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
        raise RuntimeError(f"{proc.args[0]} timed out: {err!r}") from None
    return proc.returncode, out, err


def ssh_to_mulib(ssh, peer, user, user_key, known_hosts, reject=False):
    user_pin = raw_key(user_key.with_suffix(".pub").read_text())
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(10)
        options = SUITE + [
            "BatchMode=yes", "IdentitiesOnly=yes", "HostKeyAlias=mulib-fixture",
            "StrictHostKeyChecking=yes", f"UserKnownHostsFile={known_hosts}",
            "GlobalKnownHostsFile=/dev/null", "PreferredAuthentications=publickey",
            "IdentityAgent=none", "UpdateHostKeys=no", "RequestTTY=no", "LogLevel=ERROR",
        ]
        args = [ssh, "-F", "/dev/null", "-n", "-p", str(listener.getsockname()[1]),
                "-i", str(user_key), "-l", user]
        args += [value for option in options for value in ("-o", option)]
        with subprocess.Popen(args + ["127.0.0.1", COMMAND], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE) as client:
            try:
                connection, _ = listener.accept()
                with connection, subprocess.Popen(
                    [peer, "reject" if reject else "server", user, user_pin],
                    stdin=connection, stdout=connection, stderr=subprocess.PIPE,
                ) as server:
                    connection.close()
                    result = finish(client)
                    server_result = finish(server)
            finally:
                if client.returncode is None:
                    client.kill()
        if server_result[0] != 0:
            raise RuntimeError(f"MuLIB server failed: {server_result!r}; SSH: {result!r}")
        if reject:
            if result[0] != 255 or result[1] or b"Permission denied" not in result[2]:
                raise RuntimeError(f"Expected OpenSSH authorization rejection: {result!r}")
        elif result != (0, STDOUT, STDERR):
            raise RuntimeError(f"Unexpected OpenSSH client output: {result!r}")


def mulib_to_sshd(sshd, peer, user, host_pin, config):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        with socket.create_connection(listener.getsockname()) as client_fd:
            server_fd, _ = listener.accept()
            with server_fd, tempfile.TemporaryFile() as server_log:
                with subprocess.Popen([sshd, "-i", "-e", "-f", str(config)],
                                      stdin=server_fd, stdout=server_fd, stderr=server_log) as server:
                    server_fd.close()
                    with subprocess.Popen([peer, "client", user, host_pin], stdin=client_fd,
                                          stdout=client_fd, stderr=subprocess.PIPE) as client:
                        client_fd.close()
                        result = finish(client)
                    finish(server)
                    if result[0]:
                        server_log.seek(0)
                        raise RuntimeError(f"MuLIB client failed: {result!r}; sshd: {server_log.read()!r}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("peer", type=lambda path: str(Path(path).resolve()))
    args = parser.parse_args()
    ssh, sshd, ssh_keygen = (shutil.which(name) for name in ("ssh", "sshd", "ssh-keygen"))
    if not all((ssh, sshd, ssh_keygen)):
        print("SKIP: ssh, sshd and ssh-keygen are required")
        return 77
    user = pwd.getpwuid(os.getuid()).pw_name
    parent = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
    with tempfile.TemporaryDirectory(prefix="mulib-ssh-", dir=parent) as temp:
        work = Path(temp)
        host, identity = work / "host", work / "identity"
        for key in (host, identity):
            subprocess.run([ssh_keygen, "-q", "-t", "ed25519", "-N", "", "-f", str(key)],
                           check=True, timeout=15)
        host_key = bytes.fromhex(subprocess.check_output([args.peer, "host-key"], text=True, timeout=15))
        user_key = bytes.fromhex(subprocess.check_output([args.peer, "user-key"], text=True, timeout=15))
        known_hosts, authorized_keys = work / "known_hosts", work / "authorized_keys"
        known_hosts.write_text(f"mulib-fixture {key_blob(host_key)}\n")
        authorized_keys.write_text(f"restrict {key_blob(user_key)}\n")
        config = work / "sshd_config"
        config.write_text("\n".join(s.replace("=", " ", 1) for s in SUITE) + "\n"
                          f"HostKey {host}\nAuthorizedKeysFile {authorized_keys}\n"
                          "StrictModes no\nPubkeyAuthentication yes\nPasswordAuthentication no\n"
                          "KbdInteractiveAuthentication no\nPermitRootLogin prohibit-password\n"
                          "UsePAM no\nDisableForwarding yes\nPrintMotd no\nPrintLastLog no\n"
                          f"ForceCommand {COMMAND}\n")
        ssh_to_mulib(ssh, args.peer, user, identity, known_hosts)
        ssh_to_mulib(ssh, args.peer, user, identity, known_hosts, reject=True)
        mulib_to_sshd(sshd, args.peer, user, raw_key(host.with_suffix(".pub").read_text()), config)
    print("OpenSSH interoperability passed in both directions (pins, rekey, auth, extended data, close).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
