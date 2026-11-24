#!/usr/bin/env python3
"""Compare exact token bytes, offsets and diagnostics from both scanners."""
from pathlib import Path
import random
import subprocess
import sys

generated, reference, examples, work = map(Path, sys.argv[1:5])
work.mkdir(parents=True, exist_ok=True)
count = 0


def probe(tool, path):
    return subprocess.run([str(tool), str(path)], capture_output=True, timeout=30)


def check(name, data, *, invalid_at=None, diagnostic='invalid token', valid=False):
    global count
    path = work / (name + '.on1x')
    path.write_bytes(data)
    r, g = probe(reference, path), probe(generated, path)
    assert g.returncode in (0, 1), (name, 'generated scanner crashed', g.returncode, g.stderr)
    assert r.returncode in (0, 1), (name, 'fallback scanner crashed', r.returncode, r.stderr)
    assert (r.returncode, r.stdout, r.stderr) == (g.returncode, g.stdout, g.stderr), (
        name, data[:200], r.returncode, r.stdout[:500], r.stderr,
        g.returncode, g.stdout[:500], g.stderr)
    if invalid_at is not None:
        line = data.count(b'\n', 0, invalid_at) + 1
        column = invalid_at - data.rfind(b'\n', 0, invalid_at)
        expected = f'{invalid_at}:{line}:{column}: error: {diagnostic}\n'.encode()
        assert g.returncode == 1 and g.stderr == expected, (name, g.stderr, expected)
        assert not g.stdout, (name, 'partial tokens exposed on failure')
    elif valid:
        assert g.returncode == 0, (name, g.stderr)
    if g.returncode == 0:
        tokens = g.stdout.splitlines()
        assert tokens, (name, 'missing EOF')
        previous_end = 0
        for i, token in enumerate(tokens):
            coordinates, text = token.split(b' ')
            offset, line, column = map(int, coordinates.split(b':'))
            text = bytes.fromhex(text.decode('ascii'))
            assert line == data.count(b'\n', 0, offset) + 1, (name, token)
            assert column == offset - data.rfind(b'\n', 0, offset), (name, token)
            assert previous_end <= offset <= len(data), (name, token)
            if i == len(tokens) - 1:
                assert offset == len(data) and text == b'<eof>', (name, token)
            else:
                assert data[offset:offset + len(text)] == text, (name, token)
                previous_end = offset + len(text)
    count += 1


for source in sorted(examples.glob('*.on1x')):
    check('example_' + source.stem, source.read_bytes(), valid=True)

valid_cases = [
    b'', b'#', b' \t\r\n# comment\r\n# final',
    b'abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 00019',
    b'== != <= >= && || -> + - * / % < > = ! ; : , ( ) { } [ ]',
    b'@ppx foo "path" macro rule $name ... => 0x2a 0XFF 0x 0xG 0x1z ...',
    b'import kernel mulib.histprice as History; unsafe import kernel mulib.json; import kernel as ordinary;',
    b'macro m literals(else,_) ellipsis dots { rule ([$x:Syntax dots . $rest:Syntax]) => block { f($x) dots; }; }',
    b'quote { let sampled = lift(value); splice(code) + sampled } quote {} run(code) code<code<i64>>=value vector[1,2] . .. ... ....',
    b'let_syntax { macro m { rule () => expression { 42 }; } } expression { m!() } letrec_syntax {} block {}',
    b'===!==<==>==&&||- ->> ///',
    b'"# not a comment"# comment\n"snow: \xe9\x9b\xaa" name',
    b'`/bin/echo one "two three" \'four\' escaped\\ word`',
    b'`/bin/echo escaped\\`tick "" \'$literal\'`',
    b'```/bin/echo\n"multiline word"\n```',
    b'`` `left``right` `````` ```left``````right```',
    b'"\\\\ \\" \\n \\r \\t \\0"',
    b'"raw\tbyte\x01\xff"',
    b'# \xff invalid UTF-8 in comment\r\nfn main() {}',
    b'x# inline\r\n +# more\n1',
    b'name+12;\n' * 4096,
    b'0.0 1.25 1e3 1E+3 1.5e-12 0x1e3 1... 1. 1e+ 1.2e- 1.2.3 1e2e3',
    b'0u8 255u8 65535u16 4294967295u32 18446744073709551615u64 0xffu8 0x1f32 0xf64 0xffu64',
    b'0..10 0..=10 0...10 ... ..= .. ....= 1.0..2.0 1u64..=2u64',
    b'outer:for(var i=0;i<3;i=i+1){loop{continue outer;}} for x in -2..=2{break;}',
    b'match let x=f() with { (1|2) as n if n>0 => n, rest => 0 }; :: :::: ||| | record{x:_,..}',
    b'if match(flag) { print_flag(flag); } if match(value) { Pair((_, yes)) => yes } { print_flag(true); }',
    b'fn p(k:ref i64){k := 2;} foo() ~ { Ok(-) => println(1), Err(x) => println(x) };',
    b'fn inspect(x:borrow Cell){let y=borrow x;} fn edit(x:ref Cell){let y=borrow mut x;y:=x;}',
    b'enum List<T>{Nil,Cons(T,own<List<T>>)} let x=box(v); let y=move(x); take(y); drop(y);',
    b'cliche Show{fn show<T>(value:Self,x:T)->string;let fallback:Self;} impl<T> Show for Box<T> where Show<T>{let fallback:Box<T>=value;} Show<Box<i64>>::show<bool>(box,true);',
    br'let c = #\newline; let b = b!#\a; let raw = r!"a\n"; let n = 0o77 + 0b101;',
    br'println("value: ${text_len("abc")}"); println("""first'
    b'\nsecond""");',
    b"''' unprefixed\n  ${r!'''nested'''} ''' r!''' raw \\w\n''' b!'''\\x01\n'''",
    b"x{- outer\n{@ inner {- deeper -} @} -}y{@ ignored @}z{! doc }{>\n  more\n}",
    b'let r = {- /not-a-regex/ -}/a+/; match /b+/ {! docs /ignored/ } ~ "b";',
    b'fn main(){@ppx_label("check", {@nested ("check");});} {@ name("in a comment") @}',
    b'"${1 {- { } / -} + 2 {! inner } }" r!""" {! opaque } """',
    'let pattern = /雪+/; assert(match /[a-z]+/ ~ "élan"); let score = 24 / 6 / 2;'.encode(),
    b'fn main(){taskgroup within 25ms{taskgroup within duration() s{poll{after 0ms=>1};}}}',
    b'1f32 1.5f32 1.5e-12f32 1e3f64 1u128 1.5u8 1f16 0x1.0f32 1u8x 1f32x 1u16f32',
    b'"' + b'x' * 65536 + b'"',
    b'#' + b'x' * (2 * 1024 * 1024 - 1),
]
for i, data in enumerate(valid_cases):
    check(f'valid_{i}', data, valid=True)

prefix = b'# layout\r\n\t"snow: \xe9\x9b\xaa" ; '
check('defer_result', b'enum Result<T,E>{Ok(T),Err(E)} fn f(){defer println(1); read()??[0];}', valid=True)
for i, bad in enumerate([b'`', b'`trailing\\', b'```unterminated', b'````', b'`closed``unterminated',
                         b'```````', b'`line\nbreak`', b'&', b'\v', b'\f', b'\xff', b'"unclosed',
                         b'"bad\\q"', b'"bad\\', b'"raw\nnewline"', b'"raw\rnewline"',
                         b"'''unclosed", b"r!'''unclosed", b"b!'''unclosed", b"'single'",
                         b"r!'", b"b!''",
                         b'{- no close', b'{@ wrong close -}', b'{! missing brace',
                         b'{> missing brace', b'{- ' * 257 + b' -}' * 257]):
    offset = 8 if bad == b'`closed``unterminated' else 6 if bad == b'```````' else 0
    check(f'invalid_{i}', prefix + bad, invalid_at=len(prefix) + offset)
for i, data in enumerate([b'\0', b'# comment\0', b'"string\0"', b'@\n\0']):
    check(f'nul_{i}', data, invalid_at=data.index(b'\0'), diagnostic='NUL in source')
check('oversized', b'#' * (2 * 1024 * 1024 + 1), invalid_at=0,
      diagnostic='source exceeds 2 MiB limit')

rng = random.Random(20261114)
spellings = [b'name', b'_1', b'019', b'1.25e-3', b'1e+9', b'0x1e3', b'"text#\\n"', b'"\xe9\x9b\xaa"', b'<=', b'->', b'/', b'[]', b'!=', b'..', b'..=', b'...', b'::', b'|', b':=', b'~']
layouts = [b' ', b'\t', b'\r\n', b'# comment\n']
for i in range(40):
    data = b''.join(rng.choice(spellings) + rng.choice(layouts) for _ in range(32))
    check(f'random_valid_{i}', data, valid=True)
alphabet = b'abceE_01234. +-*/%!=<>&|#\\"`\n\r\t@\x7f\x80'
for i in range(160):
    data = bytes(rng.choice(alphabet) for _ in range(rng.randrange(150)))
    check(f'random_bytes_{i}', data)
print(f'Aurocks/fallback parity: {count} exact token/diagnostic cases PASS')
