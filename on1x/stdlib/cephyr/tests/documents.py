"""Build and run an On1x consumer of Cephyr's two sexpr-producing stages."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    compiler, cc, archive, library = sys.argv[1:]
    runtime = Path(__file__).resolve().parents[3] / "runtime"
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        source, generated, exe = (work / name for name in ("test.on1x", "test.c", "test"))
        source.write_text(r'''
import std.cephyr as C;
import std.sexpr as S;
import std.String as Text;

fn field(items: sexpr, name: string) -> sexpr {
  var rest = items;
  while sx_kind(rest) == 3 {
    let item = sx_head(rest);
    if sx_kind(item) == 3 && sx_text(sx_head(item)) == name {
      return item;
    }
    rest = sx_tail(rest);
  }
  tool_fail(text_concat("missing field: ", name));
  return sx_nil();
}

fn test_documents() {
  let source = tool_read_file("input.c");
  let syntax: sexpr = C.cparser(source);
  let interface: sexpr = C.csemantics(source);
  assert(sx_kind(syntax) == 3);
  assert(sx_kind(sx_head(syntax)) == 1 && sx_text(sx_head(syntax)) == "1");
  let document = S.sexpr_nth(syntax, 1);
  assert(sx_text(sx_head(document)) == "object");
  let ast = S.sexpr_nth(field(sx_tail(document), "ast"), 1);
  assert(sx_kind(field(sx_tail(ast), "kind")) == 3);
  let source_map = S.sexpr_nth(field(sx_tail(document), "source"), 1);
  let ctext = S.sexpr_nth(field(sx_tail(source_map), "text"), 1);
  assert(sx_kind(ctext) == 2);
  assert(Text.contains(sx_text(ctext), buffer_text(b!"\"caf\xc3\xa9\\n\"")));
  assert(Text.contains(sx_text(ctext), "\n"));
  assert(sx_text(sx_head(interface)) == "interface");
  assert(sx_text(S.sexpr_nth(interface, 1)) == "1");
  let declarations = sx_tail(sx_tail(interface));
  let target = S.sexpr_nth(field(declarations, "target"), 1);
  assert(sx_kind(target) == 2 && sx_text(target) == "x86_64-linux-gnu");
  let array = field(declarations, "object");
  assert(sx_text(S.sexpr_nth(array, 1)) == "table");
  let layout = field(sx_tail(sx_tail(array)), "layout");
  assert(sx_text(S.sexpr_nth(field(sx_tail(layout), "size"), 1)) == "32");
  assert(sx_text(S.sexpr_nth(field(sx_tail(layout), "align"), 1)) == "8");

  # Syntax parsing does not evaluate a static assertion.
  let parsed_only = C.cparser("_Static_assert(0, \"projection validation\");");
  assert(sx_text(sx_head(parsed_only)) == "1");
}

fn main() {
  let mode = tool_env("TEST_CEPHYR_MODE");
  if mode == "invalid-syntax" {
    C.cparser("int malformed(;");
    return;
  }
  if mode == "invalid-semantics" {
    C.csemantics("_Static_assert(0, \"projection validation\");");
    return;
  }
  let before = data_live_nodes();
  test_documents();
  assert(data_live_nodes() == before);
}
''')
        subprocess.run([compiler, "--emit-c", source, "-o", generated], check=True)
        subprocess.run([cc, "-std=c11", "-I", runtime, generated, runtime / "on1x_runtime.c",
                        archive, library, f"-Wl,-rpath,{Path(library).parent}", "-o", exe],
                       check=True)
        (work / "input.c").write_text(
            '#include "types.h"\nextern struct Pair table[COUNT];\n'
            'extern const char *message;\nconst char *message = "café\\n";\n',
            encoding="utf-8",
        )
        (work / "types.h").write_text(
            "#define COUNT 2\ntypedef unsigned long Word;\nstruct Pair { char tag; Word value; };\n"
        )
        (work / "Cephyr.toml").write_text("[invalid TOML")
        env = dict(os.environ, CEPHYR_CONFIG=str(work / "Cephyr.toml"), PATH="",
                   TEST_CEPHYR_MODE="")
        subprocess.run([exe], cwd=work, env=env, check=True)
        for mode, message in (
            ("invalid-syntax", b"cephyr.parse"),
            ("invalid-semantics", b"projection validation"),
        ):
            failed = subprocess.run([exe], cwd=work, capture_output=True,
                                    env=dict(env, TEST_CEPHYR_MODE=mode))
            assert failed.returncode == 73 and message in failed.stderr, failed


if __name__ == "__main__":
    main()
