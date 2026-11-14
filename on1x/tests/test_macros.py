from support import NativeTests, run

t = NativeTests()
t.valid('macros', '42\n42\n99\nhex\nnumber\n')
t.valid('structural_macros', '1\n2\n3\n4\n5\n42\n42\ntrue\ntrue\n')
t.valid('nested_repeat_templates', '11\n12\n23\n42\n', text='''
macro rows {rule ([$base:Expression, [$x:Expression ...]] ...)=>block {println($base+$x)... ...;};}
macro forward {rule ([$x:Expression ... . $tail:Syntax])=>expression {sum!(. [$x... . $tail])};}
macro sum {rule($a:Expression,$b:Expression)=>expression{$a+$b};}
fn main(){rows!([10,[1,2]],[20,[3]], [30,[]]);println(forward!([40 . [2]]));}
''')
t.valid('structural_ast_and_literals', '42\n42\ntrue\ntrue\ntrue\ntrue\n', text='''
macro call_arg {rule(ast_call(_, $x:Expression))=>expression{$x};}
macro binary {rule(ast_binary("+",$a:Expression,$b:Expression))=>expression{$a+$b};}
macro datum {rule(-42)=>expression{true};rule(42)=>expression{true};rule(_)=>expression{false};}
macro underscore literals(_) {rule(_)=>expression{true};rule($x:Syntax)=>expression{false};}
macro escaped {rule([..., else])=>expression{true};}
macro outer {rule()=>expression{escaped!(escape([..., else]))};}
fn main(){println(call_arg!(not_called(42)));println(binary!(40+2));println(datum!(-0x2a));println(datum!(00042));println(underscore!(_));println(outer!());}
''')
t.valid('multiple_repetitions', '1\n2\n3\n4\ntrue\n', text='''
macro split {rule($a:IntegerLiteral ..., pivot, $b:IntegerLiteral ..., end)=>block{println($a)...;println($b)...;};}
macro vector {rule(vector[[$x:BooleanLiteral ...] ...])=>block{println($x)... ...;};}
fn main(){split!(1,2,pivot,3,4,end);split!(pivot,end);vector!(vector[[],[true]]);}
''')
t.valid('repeated_binding_templates', '40\n2\n42\n', text='''
macro bind {rule([$name:Identifier,$value:Expression] ...)=>block{syntax_block{let $name=$value;}...;};}
macro names {rule($name:Identifier ...)=>block{syntax_block{let ($name,ignored)=(42,0);println($name);}...;};}
fn main(){bind!([a,40],[b,2]);println(a);println(b);{names!(answer);}}
''')
t.valid('macro_hygiene', '42\n42\n99\n', text='''
fn helper(x:i64)->i64 {return x+1;}
macro plus_one {rule ($x:Expression)=>expression {helper($x)};}
macro with_temp {rule ($x:Expression)=>expression {fn(temporary:i64)->i64{return $x+temporary;}(2)};}
macro print_it {rule ($x:Expression)=>block {println($x);};}
fn main(){
 let helper=fn(x:i64)->i64{return 0;}; let temporary=40;
 println(plus_one!(41));println(with_temp!(temporary));
 let println=fn(x:i64){};print_it!(99);
}
''')
t.valid('identifier_binder', '42\n1\n', text='''
macro bind {rule ($name:Identifier,$value:Expression)=>block {let $name=$value;};}
fn main(){let answer=1;{bind!(answer,42);println(answer);}println(answer);}
''')
t.valid('literal_binding', '1\n2\n', text='''
macro choose {rule (done)=>expression {1};rule ($id:Identifier)=>expression {2};}
fn main(){println(choose!(done));{let done=42;println(choose!(done));}}
''')
t.valid('syntax_subtyping', 'hex\ninteger\nliteral\nexpression\n', text='''
macro classify {
 rule ($x:HexLiteral)=>expression {"hex"};
 rule ($x:IntegerLiteral)=>expression {"integer"};
 rule ($x:Literal)=>expression {"literal"};
 rule ($x:Expression)=>expression {"expression"};
}
fn main(){println(classify!(0x2a));println(classify!(42));println(classify!(true));println(classify!(1+2));}
''')
t.valid('empty_repeat', '42\n', text='''
macro run {rule ($body:Block ...)=>block {$body...;};}
fn main(){run!();run!({println(42);});}
''')
t.valid('nested_hygiene', '42\ntrue\n', text='''
macro outer {rule ($x:Expression)=>expression {inner!($x)};}
macro inner {rule ($x:Expression)=>expression {fn(temp:i64)->_ {return $x;}(0)};}
fn main(){let temp=42;println(outer!(temp));println(outer!(true));}
''')
t.source('macro_library', 'macro answer {rule ()=>expression {42};}')
t.valid('macro_import', '42\n', text='import "macro_library.on1x"; fn main(){println(answer!());}')
t.valid('hex_boundaries', '42\n9223372036854775807\n-9223372036854775808\n', text='''
fn main(){println(0x2a);println(0X7fffffffffffffff);println(-0x8000000000000000);}
''')
hooks=['-include',t.root/'tests/fixtures/closure_allocator.h',t.root/'tests/fixtures/closure_allocator.c']
t.valid('macro_ownership', '42\n', extra=hooks, text='''
extern fn live_environments()->i64;
macro close {rule ($x:Expression)=>expression {fn()->i64{return $x;}};}
fn main(){{let n=42;let f=close!(n);println(f());}assert(live_environments()==0);}
''')
reject = [
 ('category', 'macro m{rule($x:HexLiteral)=>expression{1};} fn main(){m!("s");}', 'no typed rule'),
 ('bad_type', 'macro m{rule($x:NotAType)=>expression{1};} fn main(){}', 'unsupported syntax category'),
 ('unknown_hole', 'macro m{rule()=>expression{$x};} fn main(){}', 'unknown template variable'),
 ('duplicate_hole', 'macro m{rule($x:Expression,$x:Expression)=>expression{1};} fn main(){}', 'duplicate pattern variable'),
 ('duplicate_macro', 'macro m{rule()=>expression{1};} macro m{rule()=>expression{2};} fn main(){}', 'duplicate macro'),
 ('block_expression', 'macro m{rule($x:Block)=>expression{$x};} fn main(){}', 'expected Expression'),
 ('bad_binding_type', 'macro m{rule($x:Expression)=>block{let $x=1;};} fn main(){}', 'requires one Identifier'),
 ('block_result', 'macro m{rule()=>block{};} fn main(){let n=m!();}', 'block macro used in expression'),
 ('repetition_use', 'macro m{rule($x:Expression ...)=>expression{$x};} fn main(){}', 'requires ellipsis'),
 ('repetition_context', 'macro m{rule($x:Expression ...)=>expression{$x...+1};} fn main(){}', 'only valid in'),
 ('invalid_dotted_repeat', 'macro m{rule($x:Expression . $tail:Syntax ...)=>expression{1};} fn main(){}', 'cannot itself repeat'),
 ('repetition_scalar', 'macro m{rule($x:Expression)=>expression{$x...};} fn main(){}', 'requires a repeated'),
 ('unbound_def_site', 'macro m{rule()=>expression{x};} fn main(){let x=42;m!();}', 'undefined variable: x'),
 ('binding_literal', 'macro m{rule(done)=>expression{1};} fn main(){let done=42;m!(done);}', 'no typed rule'),
 ('duplicate_generated', 'macro m{rule($n:Identifier)=>block{let $n=1;};} fn main(){let x=1;m!(x);}', 'duplicate local: x'),
 ('recursive_macro', 'macro m{rule()=>expression{m!()};} fn main(){m!();}', 'limit exceeded'),
 ('normal_type_error', 'macro m{rule()=>expression{1+true};} fn main(){m!();}', 'expected i64'),
 ('capture_mutable', 'macro m{rule($x:Expression)=>expression{fn()->i64{return $x;}};} fn main(){var x=1;let f=m!(x);}', 'capturing mutable'),
 ('undefined_macro', 'fn main(){unknown!();}', 'unhandled metaprogramming'),
 ('hex_overflow', 'fn main(){println(0xffffffffffffffff);}', 'out of i64 range'),
 ('nested_duplicate', 'macro m{rule([$x:Syntax],[$x:Syntax])=>expression{1};}fn main(){}', 'duplicate pattern variable'),
 ('missing_nested_ellipsis', 'macro m{rule([$x:Expression ...] ...)=>block{println($x)...;};}fn main(){}', 'requires ellipsis'),
 ('excess_ellipsis', 'macro m{rule($x:Expression ...)=>block{println($x)... ...;};}fn main(){}', 'at each repetition depth'),
 ('shape_mismatch', 'macro m{rule([$x:Expression ...],[$y:Expression ...])=>block{println($x+$y)...;};}fn main(){m!([1,2],[3]);}', 'incompatible repetition lengths'),
 ('vector_tail', 'macro m{rule(vector[$x:Syntax . $rest:Syntax])=>expression{1};}fn main(){}', 'cannot have dotted tails'),
 ('ellipsis_literal', 'macro m ellipsis dots literals(dots){rule()=>expression{1};}fn main(){}', 'cannot also be a literal'),
 ('escaped_syntax_escape', 'macro m{rule()=>expression{escape([...])};}fn main(){}', 'expected Expression'),
 ('unlowered_syntax', 'macro m{rule($x:Syntax)=>block{$x;};}fn main(){m!([1]);}', 'unhandled metaprogramming syntax'),
]
for name,text,diagnostic in reject:
    p=run([t.compiler,'--emit-c',t.source('reject_'+name,text)])
    assert p.returncode==1 and diagnostic in p.stderr and not p.stdout,(name,p.returncode,p.stderr)
# Flat pattern sequences must not recurse once per argument; ambiguous repeats
# must stop at the explicit work budget instead of exponential search.
t.valid('flat_macro_pattern', '42\n', text='macro m{rule('+','.join(['_']*10000)+')=>expression{42};}fn main(){println(m!('+','.join(['1']*10000)+'));}')
adversarial='macro m{rule('+','.join(['_ ...']*10+['missing'])+')=>expression{1};}fn main(){m!('+','.join(['1']*24)+');}'
p=run([t.compiler,'--emit-c',t.source('macro_match_work',adversarial)])
assert p.returncode==1 and not p.stdout and 'pattern matching work limit exceeded' in p.stderr,p.stderr
source=t.root/'examples/macros.on1x'
a,b=(run([t.compiler,'--emit-c',source]) for _ in range(2))
assert a.returncode==b.returncode==0 and a.stdout==b.stdout and '$' not in a.stdout
print(f'Typed hygienic AST macros: PASS ({len(reject)} rejections)')
