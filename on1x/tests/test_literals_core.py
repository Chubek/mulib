"""Checked core literals through parsing, PPX, formatting and C11 AOT."""
from support import NativeTests, run

t = NativeTests()
source = r'''# normal comment with #\and "q"
macro show { rule ($value: Expression) => expression { "${$value}:${"end"}" }; }
fn display(c:char)->char { return c; }
fn side(i:i64)->i64 { println(i); return i+1; }
fn main() {
 let suffix="end";
 assert(display(#\a)==#\x61);
 assert(#\pound==#\£);
 assert(#\u+ff22==#\Ｂ);
 assert(#\U+10ffff != #\x2211);
 println(char_codepoint(#\\));
 println(char_codepoint(#\newline));
 println(char_codepoint(#\"));
 println(char_codepoint(#\f));
 println(char_codepoint(#\nul));
 println(#\pound);
 println(match #\J with { #\o112 => "octal char", rest => "bad" });
 let byte:u8=b!#\xFF; println(byte);
 let ascii:u8=b!#\a; assert(ascii==97u8);
 let buf:buffer=b!"A\0\xff\t"; let alias=buf;
 assert(buffer_len(buf)==4);
 assert(buffer_at(buf,0)==b!#\A);
 assert(buffer_at(buf,1)==0u8 && buffer_at(buf,2)==byte);
 buffer_push(alias,b!#\nul);
 assert(buffer_at(buf,4)==0u8);
 assert(buffer_len(b!"")==0);
 assert(buffer_len(b!"""x
y""")==3);
 println(buffer_text(b!"plain"));
 println(r!"$suffix \w ${z}");
 println("""one
two "# inside" $suffix""");
 println(r!"""literal "quotes" # $suffix
line two""");
 println("\$suffix $$ ${"inner $suffix"} ${record{x: 10}["x"]}");
 println(show!(0B101010));
 let braces="{name}";
 assert(format("foo$braces")=="foo{name}");
 println(format("{{{}}} {} {} {}", side(1), side(2), true, #\x2211));
 println("order: ${side(3)} ${side(4)}");
 println(0o42 + 0b1000);
 assert(0O377u8==255u8);
 assert(-0o1000000000000000000000==-9223372036854775808);
 assert(match -0b10 with { -0o2 => true, rest => false });
 assert(1.25e+2==125f64 && 2E-1f32==0.2f32);
}
'''
expected = ('92\n10\n34\n102\n0\n£\noctal char\n255\nplain\n'
            '$suffix \\w ${z}\none\ntwo "# inside" end\n'
            'literal "quotes" # $suffix\nline two\n'
            '$suffix $ inner end 10\n42:end\n1\n2\n{2} 3 true ∑\n'
            '3\n4\norder: 4 5\n42\n')
path = t.source("test_literals_positive", source)
t.valid("test_literals_positive", expected, source=path)

formatted = run([t.compiler, "fmt", path])
assert formatted.returncode == 0, formatted.stderr
assert r'b!"A\0\xff\t"' in formatted.stdout
assert r'#\pound' in formatted.stdout
assert '"""one\ntwo "# inside" $suffix"""' in formatted.stdout
new = t.source("test_literals_formatter", formatted.stdout)
assert run([t.compiler, "fmt", new]).stdout == formatted.stdout
t.valid("test_literals_formatter", expected, source=new)

t.valid("test_literals_global_format", "42\n", text='''
fn format(n:i64)->i64 { return n+1; }
fn main(){ let f=format; println(f(41)); }
''')
t.source("test_literals_format_provider", "fn other(n:i64)->i64 {return n+2;}")
t.valid("test_literals_imported_format", "42\n", text='''
from test_literals_format_provider import other as format;
fn main(){ println(format(40)); }
''')
p = run([t.compiler, "--check", t.source("test_literals_legacy_argv", '''
fn main() { taskgroup {
 let process=launch exec("/bin/echo", "$unset", "${not_a_binding}", "$(literal)");
 assert(join process==0);
} }
''')])
assert p.returncode == 0, p.stderr

wire = run([t.compiler, "--emit-ast", path])
assert wire.returncode == 0, wire.stderr
for tag in ("CharType", "CharacterLiteral", "ByteBufferLiteral", "InterpolatedString"):
    assert "(" + tag + " " in wire.stdout or "(" + tag + ")" in wire.stdout, tag
ppx_c = t.work / "test_literals_ppx.c"
ppx_exe = t.work / "test_literals_ppx"
p = run([t.compiler, "--emit-c", path, "-o", ppx_c,
         "--ppx", "identity", t.root.resolve() / "stdppx" / "identity.py"])
assert p.returncode == 0, p.stderr
p = run([t.cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
         "-I", t.root / "runtime", ppx_c, t.root / "runtime/on1x_runtime.c",
         *t.flags, "-o", ppx_exe])
assert p.returncode == 0, p.stderr
p = run([ppx_exe])
assert p.returncode == 0 and p.stdout == expected, (p.stdout, p.stderr)

for name, value, message in (
    ("above_unicode", r'#\U+ffffff', "outside the Unicode scalar"),
    ("above_u16", r'#\u+10000', "outside its 16-bit range"),
    ("surrogate", r'#\xd800', "outside the Unicode scalar"),
    ("octal_char", r'#\o118', "invalid character code point digits"),
    ("missing_char", '#\\', "invalid token"),
    ("unknown_name", r'#\notachar', "unknown named character"),
    ("byte_range", r'b!#\u+100', "outside 0..255"),
    ("byte_not_integer", r'b!#\a + 1', "expected i64, got u8"),
    ("byte_escape", r'b!"\x0z"', "invalid token"),
    ("ordinary_escape", r'"bad\q"', "invalid token"),
    ("nul_escape", r'"no\0"', "NUL escape"),
    ("triple", '"""unclosed', "invalid token"),
    ("interp_missing", '"${name"', "invalid token"),
    ("interp_empty", '"${}"', "expected identifier"),
    ("interp_inline", '"${42 34}"', "expected '<eof>'"),
    ("interp_type", '"${(1,2)}"', "unsupported interpolation type"),
    ("format_type", 'format("{}", (1,2))', "unsupported format argument"),
    ("char_not_integer", r'#\a + 1', "expected i64"),
    ("radix_overflow", '0o1000000000000000000000', "out of i64 range"),
    ("radix_suffix", '0b100000000u8', "out of u8 range"),
    ("bad_radix_digit", '0o8', "expected"),
    ("bad_exponent", '1e+', "expected"),
    ("huge_exponent", '1e309', "overflows or underflows"),
):
    text = value if name == "missing_char" else f"fn main() {{ println({value}); }}"
    p = run([t.compiler, "--check", t.source("test_literals_" + name, text)])
    assert p.returncode == 1 and message in p.stderr, (name, p.stderr)

for name, invalid, message in (
    ("invalid_utf8", b"#\\\xc0\x80", "valid UTF-8 scalar"),
    ("escaped_surrogate_utf8", b"#\\\xed\xa0\x80", "outside the Unicode scalar"),
    ("source_nul", b'"X\x00Y"', "NUL in source"),
):
    corrupt = t.work / ("test_literals_" + name + ".on1x")
    corrupt.write_bytes(b"fn main() { println(" + invalid + b"); }")
    p = run([t.compiler, "--check", corrupt])
    assert p.returncode == 1 and message in p.stderr, (name, p.stderr)

for name, expr, error in (
    ("format_arity", 'format("{}", 1, 2)', "too many arguments"),
    ("format_bad_template", 'format("{:d}", 1)', "invalid brace"),
    ("format_char_nul", r'format("{}", #\nul)', "NUL character"),
    ("buffer_nul_text", r'buffer_text(b!"a\0b")', "NUL cannot be converted"),
    ("buffer_bounds", r'"${buffer_at(b!"A", 1)}"', "buffer index out of range"),
):
    exe = t.build("test_literals_" + name, text=f"fn main() {{ println({expr}); }}")
    p = run([exe])
    assert p.returncode == 73 and error in p.stderr, (name, p.returncode, p.stderr)
