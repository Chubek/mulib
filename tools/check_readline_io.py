#!/usr/bin/env python3
"""Exercise readline raw-mode lifetime, key editing and paste through a PTY."""
import os
import pty
import select
import subprocess
import sys
import termios
import time


def check(executable, keys, expected):
    master, slave = pty.openpty()
    saved = termios.tcgetattr(slave)
    child = subprocess.Popen([executable, '--tty'], stdin=slave, stdout=slave,
                             stderr=slave, close_fds=True)
    output = bytearray()
    sent = False
    deadline = time.monotonic() + 10
    try:
        while time.monotonic() < deadline:
            if select.select([master], [], [], 0.05)[0]:
                output.extend(os.read(master, 65536))
            if not sent and b'ready> ' in output:
                assert b'\x1b[?2004h' in output, output
                os.write(master, keys)
                sent = True
            if child.poll() is not None:
                while select.select([master], [], [], 0)[0]:
                    output.extend(os.read(master, 65536))
                break
        assert child.poll() == 0, output
        assert expected in output, output
        assert b'\x1b[?2004l' in output, output
        assert termios.tcgetattr(slave) == saved, 'terminal attributes leaked'
    finally:
        if child.poll() is None:
            child.kill()
        child.wait()
        os.close(master)
        os.close(slave)


if __name__ == '__main__':
    check(sys.argv[1], b'abc\x1b[DX\r', b'result:1:abXc')
    check(sys.argv[1], b'\x1b[200~a\nb\x1b[201~\r', b'result:1:a b')
    check(sys.argv[1], b'\x03', b'result:3:')
    check(sys.argv[1], b'\x04', b'result:2:')
    print('readline PTY: editing, paste, interrupt, EOF and terminal restoration passed')
