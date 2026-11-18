#!/usr/bin/env python3
"""Run core features together, through checking and strict C11 execution."""
from support import NativeTests, run

t = NativeTests()
program = r'''
fn update(value: ref i64) { value := value + 0b10; }
fn attempt(value: i64) -> Result<i64, i64> {
  if value == 42 { return result_ok(value); }
  return result_error(value);
}
fn main() {
  let n = 0o50;
  update(n);
  attempt(n) ~ { Ok(-) => println("changed: $n"), Err(e) => errln("unexpected: $e") };
  attempt(n - 1) ~ { Ok(-) => errln("unexpected success"), Err(e) => errln("failed: ${e}") };
  assert(match /雪+/ ~ "雪雪");
}
'''
exe = t.build("core_syntax", text=program)
result = run([exe])
assert (result.returncode, result.stdout, result.stderr) == (0, "changed: 42\n", "failed: 41\n"), result

source = t.work / "core_syntax.on1x"
formatted = t.work / "core_syntax_formatted.on1x"
fmt = run([t.compiler, "fmt", source, "--target", "plain", "-o", formatted])
assert fmt.returncode == 0, fmt.stderr
assert run([t.compiler, "fmt", formatted, "--check"]).returncode == 0
exe = t.build("core_syntax_after_fmt", source=formatted)
assert run([exe]).stdout == result.stdout
assert run([exe]).stderr == result.stderr

# The newly available prelude name is not reserved; user declarations win.
t.valid("errln_shadow", "custom\n", text='''
fn errln(message: string) { println(message); }
fn main() { errln("custom"); }
''')
