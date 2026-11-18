"""Real loader/constructor isolation, discovery, and On1x SDK dispatch."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    compiler, fixture, cc = (Path(arg).resolve() for arg in sys.argv[1:])
    root = Path(__file__).resolve().parents[2]
    source = root / "examples" / "hello.on1x"
    with tempfile.TemporaryDirectory(prefix="on1x-dynamic-") as tmp:
        work = Path(tmp)
        first, second = work / "first", work / "second"
        first.mkdir()
        second.mkdir()
        plugin = second / "libexample.so"
        shutil.copyfile(fixture, plugin)
        (first / "unrelated.txt").write_text("not an ELF")
        (first / "ignored.so").symlink_to(plugin)
        env = dict(os.environ, ON1X_PLUGIN_PATH=f":{first}::{second}:")

        def run(*args, environment=env, succeeds=True):
            completed = subprocess.run([compiler, *args], env=environment, capture_output=True, timeout=12)
            assert (completed.returncode == 0) == succeeds, (args, completed)
            return completed

        mir = run("--emit-ir", source).stdout
        assert b"SDK-FIXTURE-MIR" in run("--sdk-analyze", "mir-check", source).stderr
        assert run("--plugin", "+example", "--sdk-emit", "mir-copy", source).stdout == mir
        assert run("--plugin", "+example", "--plugin", f"+{plugin}",
                   "--sdk-emit", "mir-copy", source).stdout == mir
        assert run("--sdk-emit", "mir-copy", "--plugin", f"+{plugin}", source,
                   environment=dict(env, ON1X_PLUGIN_PATH="/nonexistent")).stdout == mir
        run("--plugin", "example", "--sdk-emit", "mir-copy", source, succeeds=False)
        run("--plugin", "+example", "--check", source, succeeds=False)
        run("--plugin", "+missing", "--sdk-emit", "mir-copy", source, succeeds=False)

        relocated = work / "bin"
        relocated.mkdir()
        copied_compiler = relocated / "on1xc"
        shutil.copy2(compiler, copied_compiler)
        args = [copied_compiler, "--sdk-emit", "mir-copy", "--plugin", f"+{plugin}", source]
        absent = subprocess.run(args, env=env, capture_output=True, timeout=12)
        assert absent.returncode != 0 and b"on1x-plugin-worker is missing" in absent.stderr
        shutil.copy2(compiler.with_name("on1x-plugin-worker"), relocated)
        present = subprocess.run(args, env=env, capture_output=True, timeout=12)
        assert present.returncode == 0 and present.stdout == mir, present

        duplicate = first / plugin.name
        shutil.copyfile(fixture, duplicate)
        run("--plugin", "+example", "--sdk-emit", "mir-copy", source, succeeds=False)
        run("--sdk-emit", "mir-copy", source, succeeds=False)
        duplicate.unlink()

        # An allowed read-only open still cannot see real host files.
        secret, probe = work / "secret", work / "probe.c"
        secret.write_text("private")
        probe.write_text('#include <fcntl.h>\n#include <unistd.h>\n'
                         '__attribute__((constructor)) static void probe(void) {'
                         f'if (open("{secret}", O_RDONLY) >= 0) _exit(99);'
                         '}\n')
        isolated = work / "read-check.so"
        subprocess.run([cc, "-shared", "-fPIC", "-I", root / "sdk/include", "-o", isolated,
                        probe, root / "sdk/tests/linked_extension.c"], check=True, timeout=12)
        assert run("--plugin", f"+{isolated}", "--sdk-emit", "mir-copy", source).stdout == mir

        # The constructor can only observe the plugin jail, even before dlsym.
        # Forbidden file creation/socket syscalls terminate the worker; neither
        # writes to the real output nor a success result are possible.
        marker = work / "must-not-exist"
        c = work / "constructor.c"
        hostile = first / "constructor.so"
        output = work / "preserved.mir"
        output.write_bytes(b"keep")
        attacks = [
            f'(void)open("{marker}", O_WRONLY|O_CREAT, 0600);',
            '(void)socket(AF_INET, SOCK_STREAM, 0);',
            '(void)fork();',
            'for (;;) {}',
        ]
        for body in attacks:
            c.write_text('#include <fcntl.h>\n#include <sys/socket.h>\n#include <unistd.h>\n'
                         f'__attribute__((constructor)) static void attempt(void) {{ {body} }}\n')
            subprocess.run([cc, "-shared", "-fPIC", "-o", hostile, c], check=True, timeout=12)
            assert run("--check", source).returncode == 0
            failed = run("--sdk-emit", "mir-copy", "--plugin", f"+{hostile}", source,
                         "-o", output, succeeds=False)
            assert not failed.stdout and output.read_bytes() == b"keep" and not marker.exists()
            assert b"failed or was terminated" in failed.stderr or b"timed out" in failed.stderr, failed


if __name__ == "__main__":
    main()
