"""Compile and run an On1x consumer of the fixed Cephyr preprocessor mode."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    compiler, cc, archive, library = sys.argv[1:]
    runtime = Path(__file__).resolve().parents[3] / "runtime"
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        source = work / "consumer.on1x"
        generated, exe = work / "consumer.c", work / "consumer"
        source.write_text(r'''
import std.cephyr as C;

fn main() {
  let src = "#include \"answer.h\"\nint incomplete = ANSWER\n";
  let (ok, expanded, expansion_notes) = C.preprocess(src);
  assert(ok == 0);
  assert(text_len(expansion_notes) == 0);
  let (syntax, syntax_notes) = C.check(src);
  assert(syntax == 1);
  assert(text_len(syntax_notes) > 0);

  let (bad, empty, error) = C.preprocess("#error preprocess_marker\n");
  assert(bad == 1);
  assert(text_len(empty) == 0);

  let (big, no_partial, limit) = C.preprocess(tool_read_file("large.c"));
  assert(big == 3);
  assert(text_len(no_partial) == 0);

  let (blank, blank_output, notes) = C.preprocess("");
  assert(blank == 0);
  assert(text_len(notes) == 0);
  println(expanded);
  println(error);
  println(limit);
}
''')
        subprocess.run([compiler, "--emit-c", source, "-o", generated], check=True)
        link = [cc, "-std=c11", "-I", runtime, generated, runtime / "on1x_runtime.c",
                archive, library, f"-Wl,-rpath,{Path(library).parent}", "-o", exe]
        subprocess.run(link, check=True)
        # The fixed config ignores invalid caller TOML, and this stage needs no PATH.
        (work / "Cephyr.toml").write_text("[invalid TOML")
        (work / "answer.h").write_text("#define ANSWER 42\n")
        env = dict(os.environ, CEPHYR_CONFIG=str(work / "Cephyr.toml"), PATH="")
        (work / "large.c").write_text('#define LARGE "' + "x" * 1024 + '"\n' + "LARGE\n" * 4200)
        output = subprocess.check_output([exe], cwd=work, env=env)
        assert re.search(rb"\bint\s+incomplete\s*=\s*42\b", output), output
        assert b'# 1 "<stdin>"' in output, output
        assert b"preprocess_marker" in output, output
        assert b"invalid or oversized Cephyr text result" in output, output

        subprocess.run(link + ["-DON1X_DATA_MALLOC(n)=((void)(n),(void*)0)"], check=True)
        failure = subprocess.run([exe], cwd=work, env=env, capture_output=True)
        assert failure.returncode == 73 and b"text allocation failure" in failure.stderr, failure


if __name__ == "__main__":
    main()
