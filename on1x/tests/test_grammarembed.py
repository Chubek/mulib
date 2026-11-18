"""Compile and execute real native GrammarEmbed output across algorithm choices."""
import itertools
import json
import os
import sys
from support import NativeTests, run

t = NativeTests()
plugin = t.compiler.parent / 'stdppx/on1x-ppx-GrammarEmbed'
env = {**os.environ, 'ON1X_PPX_SCHEMA': str((t.root / 'grammar/PPX.schema.json').resolve())}
flags = ['--ppx', 'GrammarEmbed', plugin]


def marker(grammar, name='marker'):
    return '@GrammarEmbed("grammar",' + json.dumps(grammar) + ') fn ' + name + '(){}'


def execute(name, text, expected, extra=(), before=()):
    source = t.source(name, text)
    generated, exe = t.work / (name + '.c'), t.work / name
    p = run([t.compiler, '--emit-c', source, *before, *flags, *extra, '-o', generated], env=env)
    assert p.returncode == 0 and not p.stdout, (name, p.stderr)
    repeat = run([t.compiler, '--emit-c', source, *before, *flags, *extra], env=env)
    assert repeat.returncode == 0 and repeat.stdout == generated.read_text(), (name, repeat.stderr)
    p = run([t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', t.root / 'runtime',
             generated, t.root / 'runtime/on1x_runtime.c', *t.flags, '-o', exe])
    assert p.returncode == 0, (name, p.stderr)
    p = run([exe])
    assert p.returncode == 0 and p.stdout == expected and not p.stderr, (name, p.stdout, p.stderr)
    assert 'on1x_grammar_compile' not in generated.read_text()
    return exe


def reject(name, text, message):
    source = t.source('bad_' + name, text)
    generated = t.work / 'preserved.c'
    generated.write_text('preserved\n')
    p = run([t.compiler, '--emit-c', source, *flags, '-o', generated], env=env)
    assert p.returncode != 0 and not p.stdout, (name, p.stdout, p.stderr)
    assert message in p.stderr and generated.read_text() == 'preserved\n', (name, p.stderr)


for lexer, parser in itertools.product(('dfa', 'nfa'), ('lr1', 'lalr1', 'll1')):
    grammar = r'''
grammar Calc :lexer<LEXER> :parser<PARSER> {
  token INT { <[0-9]>+ }
  token WS :skip { \s+ }
  rule TOP { <INT> [ '+' <INT> ]* }
}'''.replace('LEXER', lexer).replace('PARSER', parser)
    exe = execute(f'{lexer}_{parser}', marker(grammar) + r'''
fn main(){
  let input=io_stdin();
  if text_len(input)>0 {
    var start=0;
    for i in 0..text_len(input) {
      if text_byte(input,i)==10{println(Calc_matches(text_slice(input,start,i)));start=i+1;}
    }return;
  }
  println(Calc_matches("12 + 30 + 4"));
  println(Calc_matches("12 +"));
  println(Calc_matches("12 x"));
  println(Calc_matches(""));
  let (s,t,p)=Calc_parse("12+3"); println(s);println(p);
  println(sx_text(sx_head(t)));
  let (ls,ts,lp)=Calc_lex(" 12 + 30 ");
  println(ls);println(lp);println(sx_text(sx_head(sx_tail(sx_head(ts)))));
  println(sx_text(sx_head(sx_tail(sx_tail(sx_tail(sx_head(ts)))))));
  let (bad,none,at)=Calc_parse("12 @");println(bad);println(at);println(sx_kind(none));
  let (syntax,empty,end)=Calc_parse("12+");println(syntax);println(end);
  let (first,tree,off)=Calc_parse("1");let (second,tree2,off2)=Calc_parse("2");
  println(first==second && off==off2);println(sx_text(sx_head(tree)));
}''', 'true\nfalse\nfalse\nfalse\n0\n4\nTOP\n0\n9\n1\n12\n1\n3\n0\n2\n3\ntrue\nTOP\n')
    corpus = [''.join(word) for n in range(5) for word in itertools.product('12+ x', repeat=n)]
    wire = '\n'.join(corpus) + '\n'
    oracle = run([t.compiler.parent / 'on1x_grammarembed_reference', parser], input=wire)
    actual = run([exe], input=wire)
    assert oracle.returncode == 0 and actual.returncode == 0, (oracle.stderr, actual.stderr)
    assert actual.stdout == oracle.stdout, (lexer, parser)

# Left recursion is genuinely admitted by LR, and rejected by LL.
left = "grammar Expr :parser<PARSER> { token N { \\d+ } rule TOP { <TOP> '+' <N> | <N> } }"
for parser in ('lr1', 'lalr1'):
    execute('left_' + parser, marker(left.replace('PARSER', parser)) +
            'fn main(){println(Expr_matches("1+2+3"));println(Expr_matches("1++2"));}', 'true\nfalse\n')
reject('left_ll', marker(left.replace('PARSER', 'll1')) + 'fn main(){}', 'left recursion')

# This classic LR(1) language is not LALR(1).
lr_only = """grammar LR :parser<PARSER> {
 rule TOP { 'a' <A> 'd' | 'b' <A> 'e' | 'a' <B> 'e' | 'b' <B> 'd' }
 rule A { 'c' } rule B { 'c' }
}"""
execute('canonical', marker(lr_only.replace('PARSER', 'lr1')) +
        'fn main(){println(LR_matches("acd"));println(LR_matches("bce"));println(LR_matches("ace"));'
        'println(LR_matches("bcd"));println(LR_matches("acc"));}', 'true\ntrue\ntrue\ntrue\nfalse\n')
reject('lalr_conflict', marker(lr_only.replace('PARSER', 'lalr1')) + 'fn main(){}', 'conflicting')

for lexer in ('dfa', 'nfa'):
    grammar = """grammar Words :lexer<LEXER> {
 token LETTER { <[a-z]> }
 token ID { <LETTER>+ }
 token SPACE :skip { ' '+ }
 rule TOP { 'if' <ID>? }
}""".replace('LEXER', lexer)
    execute('priority_' + lexer, marker(grammar) +
            'fn main(){println(Words_matches("if"));println(Words_matches("if abc"));'
            'println(Words_matches("iffy"));println(Words_matches("if a"));}', 'true\ntrue\nfalse\ntrue\n')

for parser in ('lr1', 'lalr1', 'll1'):
    execute('epsilon_' + parser, marker(f"grammar Empty :parser<{parser}> {{ rule TOP {{ 'a'? }} }}") +
            'fn main(){println(Empty_matches(""));println(Empty_matches("a"));println(Empty_matches("aa"));'
            'let (s,t,p)=Empty_parse("");println(s);println(p);}', 'true\ntrue\nfalse\n0\n0\n')

# No stdlib source dependencies are injected. Multiple generated grammars coexist
# with user names that resemble generator internals.
execute('multiple', marker("grammar A {rule TOP {'a'}}", 'a_marker') +
        marker("grammar B :parser<ll1> {rule TOP {'b'}}", 'b_marker') +
        'fn on1x_ppx_1()->i64{return 42;}fn ge_parse()->i64{return 7;}'
        'fn main(){println(A_matches("a"));println(B_matches("b"));println(on1x_ppx_1());println(ge_parse());}',
        'true\ntrue\n42\n7\n', ['--no-prelude'])

execute('tree', marker("grammar Tree {token N {\\d+} rule TOP {<N> '+' <N>}}") +
        'import "' + str((t.root / 'stdlib/sexpr/sexpr.on1x').resolve()) + '";'
        'fn main(){let(s,t,p)=Tree_parse("12+3");io_stdout(sexpr_print(t));}',
        '("TOP" 0 4 (("N" 0 2 "12") ("+" 2 3 "+") ("N" 3 4 "3")))\n')
execute('symbol_names', marker("grammar Names :start<token_rule> {rule literal0 {'x'} rule token_rule {<literal0>}}") +
        'fn main(){let(s,t,p)=Names_parse("x");println(sx_text(sx_head(t)));'
        'println(Names_name(-1));println(Names_name(99999));}', 'token_rule\n\n\n')
execute('comments', marker("grammar Comment {\n rule TOP { # } is only a comment\n 'a' }\n}") +
        'fn main(){println(Comment_matches("a"));}', 'true\n')
for lexer in ('dfa', 'nfa'):
    execute('byte_classes_' + lexer, marker(
        r"grammar Bytes :lexer<LEXER> {token X { [^a-c]+ } rule TOP { <X> }}"
        .replace('LEXER', lexer)) +
        'fn main(){let b=buffer_new();buffer_byte(b,195);buffer_byte(b,169);let value=buffer_text(b);'
        'println(Bytes_matches(text_concat("xy",value)));println(Bytes_matches("b"));'
        'let(s,t,p)=Bytes_parse(value);println(p);}', 'true\nfalse\n2\n')
lua = """local L=llembed
local attribute=L.node("Attribute",{L.quote("GrammarEmbed"),L.quote("grammar"),
  L.list({L.string("grammar Meta {rule TOP {'yes'}}")}),L.location})
local marker=L.node("Function",{L.quote("generated_marker"),"()",L.type_("VoidType"),
  L.block({}),L.list({attribute}),L.location})
L.emit(L.unit({marker}))"""
execute('lua_chain', '@LuaEmbed("program",' + json.dumps(lua) + ') fn lua_marker(){}'
        'fn main(){println(Meta_matches("yes"));}', 'true\n',
        before=['--ppx', 'LuaEmbed', t.compiler.parent / 'stdppx/on1x-ppx-LuaEmbed'])

for name, grammar, message in [
    ('unknown', "grammar X {rule TOP {<missing>}}", 'reference'),
    ('nullable', "grammar X {token A {'a'*} rule TOP {<A>}}", 'consume'),
    ('cycle', "grammar X {token A {<A>} rule TOP {<A>}}", 'recursive token'),
    ('duplicate', "grammar X {rule TOP {'a'} rule TOP {'b'}}", 'duplicate'),
    ('ambiguous', "grammar X {rule TOP {<TOP> <TOP> | 'a'}}", 'conflicting'),
    ('ll_conflict', "grammar X :parser<ll1> {rule TOP {'a' | 'a' 'b'}}", 'conflicting'),
    ('skip_reference', "grammar X {token WS :skip {' '} rule TOP {<WS>}}", 'reference'),
    ('unknown_option', "grammar X :parser<glr> {rule TOP {'a'}}", 'option'),
    ('duplicate_option', "grammar X :lexer<dfa> :lexer<nfa> {rule TOP {'a'}}", 'option'),
    ('bad_regex', "grammar X {token A {'a'**} rule TOP {<A>}}", 'liblr'),
    ('bad_helper', "grammar X {token A {'a'**} rule TOP {'b'}}", 'liblr'),
    ('bad_rule', "grammar X {rule TOP {return 1;}}", 'malformed'),
    ('mismatched_group', "grammar X {rule TOP {('a']}}", 'malformed'),
    ('trailing', "grammar X {rule TOP {'a'}} garbage", 'malformed'),
    ('missing_close', "grammar X {rule TOP {'a'}", 'identifier'),
    ('source_limit', ' ' * 16385, 'oversized'),
]:
    reject(name, marker(grammar) + 'fn main(){}', message)
reject('nonliteral', '@GrammarEmbed("grammar",42) fn marker(){}fn main(){}', 'literal')
reject('nonempty_marker', marker("grammar X {rule TOP {'a'}}").replace('(){}', '(){println(1);}') +
       'fn main(){}', 'marker')
reject('collision', marker("grammar X {rule TOP {'a'}}") +
       'fn X_parse(){}fn main(){}', 'collides')
reject('duplicate_grammar', marker("grammar X {rule TOP {'a'}}", 'first') +
       marker("grammar X {rule TOP {'b'}}", 'second') + 'fn main(){}', 'collides')
reject('type', marker("grammar X {rule TOP {'a'}}") + 'fn main(){X_matches(42);}', 'type')

# Runtime bounds return a distinct status with no partial token/tree output.
execute('runtime_limits', marker("grammar Limit {rule TOP {'a'*}}") + '''
fn main(){
 let b=buffer_new();for i in 0..4097{buffer_append(b,"a");}
 let(s,t,p)=Limit_lex(buffer_text(b));println(s);println(sx_kind(t));println(p);
 let c=buffer_new();for i in 0..129{buffer_append(c,"a");}
 let(d,u,q)=Limit_parse(buffer_text(c));println(d);println(sx_kind(u));
}''', '3\n0\n4096\n3\n0\n')
execute('input_limit', marker("grammar Limit {token A {'a'+} rule TOP {<A>}}") + '''
fn main(){let b=buffer_new();for i in 0..65537{buffer_append(b,"a");}
 let(s,t,p)=Limit_parse(buffer_text(b));println(s);println(sx_kind(t));println(p);
}''', '3\n0\n0\n')
execute('work_limit', marker("grammar Limit :lexer<nfa> {token A {'a'+} rule TOP {<A>}}") + '''
fn main(){let b=buffer_new();for i in 0..60000{buffer_append(b,"a");}
 let(s,t,p)=Limit_parse(buffer_text(b));println(s);println(sx_kind(t));println(p);
}''', '3\n0\n0\n')

protocol = {**env, 'ON1X_PPX_PROTOCOL': '1', 'ON1X_PPX_LABEL': 'GrammarEmbed'}
for wire in ('(Unit 1 ())', '(Unit 1 ()) trailing', '(Unit 99 ())', '(Unit 1 ('):
    p = run([plugin], input=wire, env=protocol)
    if wire == '(Unit 1 ())':
        assert p.returncode == 0 and p.stdout == '(Unit 1 ())\n', p
    else:
        assert p.returncode != 0 and not p.stdout, p

source = t.source('discovery', marker("grammar Found {rule TOP {'ok'}}") +
                  'fn main(){println(Found_matches("ok"));}')
exe = t.work / 'discovered'
p = run([sys.executable, t.root / 'tools/on1x-build.py', source, '--on1xc', t.compiler,
         '--cc', t.cc, '--stdppx-path', plugin.parent, '--use-ppx', 'GrammarEmbed',
         '-o', exe, *['--cflag=' + flag for flag in t.flags]])
assert p.returncode == 0, p.stderr
assert run([exe]).stdout == 'true\n'
example = t.work / 'example'
p = run([sys.executable, t.root / 'tools/on1x-build.py', t.root / 'stdppx/examples/grammarembed.on1x',
         '--on1xc', t.compiler, '--cc', t.cc, '--stdppx-path', plugin.parent,
         '--use-ppx', 'GrammarEmbed', '-o', example, *['--cflag=' + flag for flag in t.flags]])
assert p.returncode == 0, p.stderr
assert run([example]).stdout == 'true\nfalse\n0\nTOP\n7\n'
print('GrammarEmbed: NFA/DFA x LR1/LALR1/LL1, trees, spans, conflicts, limits and discovery PASS')
