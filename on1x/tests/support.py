"""Shared, shell-free generated-program test harness."""
import argparse
from pathlib import Path
import subprocess


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], capture_output=True, text=True,
                          timeout=60, **kwargs)


class NativeTests:
    def __init__(self):
        cli = argparse.ArgumentParser()
        cli.add_argument('compiler', type=Path)
        cli.add_argument('root', type=Path)
        cli.add_argument('work', type=Path)
        cli.add_argument('cc')
        cli.add_argument('--cflag', action='append', default=[])
        args = cli.parse_args()
        self.compiler, self.root, self.work = args.compiler, args.root, args.work
        self.cc, self.flags = args.cc, args.cflag
        self.work.mkdir(parents=True, exist_ok=True)

    def source(self, name, text):
        path = self.work / (name + '.on1x')
        path.write_text(text, encoding='utf-8')
        return path

    def build(self, name, *, text=None, source=None, extra=()):
        if text is not None:
            source = self.source(name, text)
        if source is None:
            source = self.root / 'examples' / (name + '.on1x')
        out, exe = self.work / (name + '.c'), self.work / name
        p = run([self.compiler, '--emit-c', source, '-o', out])
        assert p.returncode == 0, (name, p.returncode, p.stderr)
        p = run([self.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-I', self.root / 'runtime', out, self.root / 'runtime/on1x_runtime.c',
                 *extra, *self.flags, '-o', exe])
        assert p.returncode == 0, (name, p.stderr)
        return exe

    def valid(self, name, expected, **kwargs):
        exe = self.build(name, **kwargs)
        p = run([exe])
        assert p.returncode == 0, (name, p.returncode, p.stdout, p.stderr)
        assert p.stdout == expected, (name, p.stdout, expected)
        return exe
