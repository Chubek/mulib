#!/usr/bin/env python3
"""Real optional ArchVirt conformance; requires an accessible native /dev/kvm."""
import argparse
import fcntl
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--compiler", required=True, type=Path)
    cli.add_argument("--runner", required=True, type=Path)
    cli.add_argument("--clang", required=True, type=Path)
    cli.add_argument("--lld", type=Path)
    cli.add_argument("--runtime-include", required=True, type=Path)
    cli.add_argument("--archvirt-include", required=True, type=Path)
    cli.add_argument("--work-dir", type=Path, default=Path.cwd())
    args = cli.parse_args()
    stage = Path(__file__).with_name("compile.py")

    def call(*cmd, **kwargs):
        result = subprocess.run(cmd, capture_output=True, timeout=25, **kwargs)
        return result.returncode, result.stdout, result.stderr

    with tempfile.TemporaryDirectory(prefix="on1x-archvirt-test-", dir=args.work_dir) as scratch:
        work = Path(scratch)

        def build(name, source, *, raw=False, expected_error=None):
            c = work / (name + ".c")
            elf = work / (name + ".elf")
            if raw:
                c.write_text(source)
            else:
                on1x = work / (name + ".on1x")
                on1x.write_text(source)
                status, out, err = call(args.compiler.resolve(), "--emit-c", "--no-prelude", on1x, "-o", c)
                assert status == 0, (name, status, out, err)
            if expected_error is not None:
                elf.write_bytes(b"existing output")
            linker = ("--lld", args.lld.resolve()) if args.lld else ()
            status, out, err = call(sys.executable, stage, "--clang", args.clang.resolve(), *linker,
                                    "--runtime-include", args.runtime_include,
                                    "--archvirt-include", args.archvirt_include, c, "-o", elf)
            if expected_error is not None:
                assert status and expected_error in err and elf.read_bytes() == b"existing output", (status, out, err)
                return None
            assert status == 0, (name, status, out, err)
            assert not elf.stat().st_mode & 0o111, elf
            return elf

        def launch(elf, **kwargs):
            return call(args.runner.resolve(), elf, **kwargs)

        guest = build("real", """
fn fib(n: i64) -> i64 {
  if n <= 1 { return n; }
  return fib(n - 1) + fib(n - 2);
}
fn main() -> i64 {
  let (n, ok) = (fib(8) * -4 / 2, true);
  println(n);
  println(ok);
  println("real DomIOS console");
  return 37;
}""")
        elf = guest.read_bytes()
        assert elf[:9] == b"\x7fELF\x02\x01\x01\x00\x00"
        assert struct.unpack_from("<HHIQ", elf, 16) == (2, 62, 1, 0x200000)
        env = dict(os.environ, ARCHVIRT_MACHINE="aarch64", DOMVIRT_MACHINE="aarch64",
                   DOMVIRT_KERNEL="/nonexistent", DOMVIRT_BIOS="/nonexistent",
                   DOMVIRT_INITRD="/nonexistent")
        assert launch(guest, env=env) == (37, b"-42\ntrue\nreal DomIOS console\n", b"")

        trap = build("trap", """
fn quotient(a:i64, b:i64)->i64 { return a / b; }
fn main() { println("before"); println(quotient(1, 0)); }
""")
        assert launch(trap) == (70, b"before\n", b"On1x arithmetic error: invalid division\n")

        assert build("floating-point", "fn main() { println(1.25); }",
                     expected_error=b"unsupported On1x f64 target or runtime ABI") is None

        raw = build("serial", r'''
#include <on1x_runtime.h>
static volatile uint8_t zeroed;
int main(void) {
    uint8_t input;
    __asm__ volatile("inb %w1, %0" : "=a"(input) : "d"(0x3f8));
    on1x_print_string("\377\001\377\002");
    return input + zeroed;
}
''', raw=True)
        assert launch(raw, input=b"never guest data") == (0, b"\xff\x01\xff\x02\n", b"")

        assert build("unavailable", "int main(void) { extern int getpid(void); return getpid(); }",
                     raw=True, expected_error=b"undefined symbol: getpid") is None

        # Match the packaged launcher contract: the core receives only an ELF
        # copied to a sealed file descriptor, not a native executable path.
        with os.fdopen(os.memfd_create("sealed-guest", os.MFD_ALLOW_SEALING), "w+b") as binary:
            binary.write(elf)
            binary.flush()
            fcntl.fcntl(binary, fcntl.F_ADD_SEALS,
                        fcntl.F_SEAL_WRITE | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SEAL)
            assert launch(f"/proc/self/fd/{binary.fileno()}", pass_fds=(binary.fileno(),)) == (
                37, b"-42\ntrue\nreal DomIOS console\n", b"")

        wrong = work / "wrong-machine.elf"
        wrong.write_bytes(elf[:18] + struct.pack("<H", 183) + elf[20:])
        for path in (wrong, Path("/bin/echo")):
            status, out, err = launch(path)
            assert status == 125 and not out and b"not an On1x DomIOS" in err, (status, out, err)

        raw = build("unframed-halt", 'int main(void) { __asm__ volatile("hlt"); return 0; }', raw=True)
        status, out, err = launch(raw)
        assert status == 125 and not out and b"without a valid terminal record" in err, (status, out, err)

        raw = build("unbounded", 'int main(void) { for (;;) __asm__ volatile(""); }', raw=True)
        status, out, err = launch(raw)
        assert status == 125 and not out and b"exceeded the 5-second KVM deadline" in err, (status, out, err)

    print("ArchVirt guest ELF, native KVM, traps, serial, profile/link rejection and watchdog passed")


if __name__ == "__main__":
    main()
