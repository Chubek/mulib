from support import NativeTests, run

t = NativeTests()
hooks = ['-include', t.root/'tests/fixtures/closure_allocator.h', t.root/'tests/fixtures/closure_allocator.c']
t.valid('staging', '2\nconstructed\n40\n42\n40\n42\n42\n', extra=hooks)
t.valid('stage_snapshot', '40\n42\n', extra=hooks, text='''
extern fn live_environments()->i64;
fn make(n:i64)->code<i64>{return quote{lift(n)+2};}
fn main(){
 {var n=40;let c=quote{lift(n)};n=2;println(run(c));
  let outer=quote{quote{lift(lift(make(40)))}};let inner=run(outer);println(run(run(inner)));}
 assert(live_environments()==0);
}
''')
t.valid('stage_regions', '1\n39\nbuilt\n42\n42\n7\n6\n42\n', extra=hooks, text='''
extern fn live_environments()->i64;
fn mark(n:i64)->i64{println(n);return n;}
fn build()->code<i64>{
 return quote{
  let one=lift(mark(1));
  let rest=lift(mark(39));
  var total=one+rest;
  total=total+1;
  let ready=total;
  let finish=fn(n:i64)->i64{return ready+n;};
  finish(1)
 };
}
fn main(){
 {let c=build();println("built");println(run(c));println(run(c));
  let empty=quote{};run(empty);
  let no_tail=quote{let value=1;assert(value==1);};run(no_tail);
  let stop=quote{return;};run(stop);
  let choice=quote{if lift(true){return 7;}return 0;};println(run(choice));
  let looped=quote{var total=0;for i in 0..4{total=total+i;}total};
  println(run(looped));
  let seed=40;
  let outer=quote{let value=lift(seed);quote{let copied=lift(value);copied+2}};
  println(run(run(outer)));}
 assert(live_environments()==0);
}
''')
t.valid('stage_region_cleanup', 'region\nregion\n', extra=hooks, text='''
extern fn live_environments()->i64;
fn main(){
 {let c=quote{let node=sx_string("region");sx_text(node)};
  println(run(c));assert(data_live_nodes()==0);
  println(run(c));assert(data_live_nodes()==0);}
 assert(live_environments()==0);
}
''')
t.valid('stage_region_compatibility', '42\n7\n9\n42\n', text='''
fn main(){
 let value_loop=quote{answer:loop{break answer with 42;}};
 let local_syntax=quote{
  let_syntax{macro answer{rule()=>expression{7};}}expression{answer!()}
 };
 let statement_loop=quote{loop{break;}let n=9;n};
 let statement_match=quote{match true with {true=>1,false=>false}42};
 println(run(value_loop));println(run(local_syntax));println(run(statement_loop));
 println(run(statement_match));
}
''')
t.valid('stage_functions', '42\ntrue\n42\n', extra=hooks, text='''
extern fn live_environments()->i64;
fn main(){
 {let id=quote{fn(x)->_{return x;}};println(run(id)(42));println(run(id)(true));
  let nested=quote{fn(x:i64)->code<i64>{return quote{lift(x)+2};}};
  println(run(run(nested)(40)));}
 assert(live_environments()==0);
}
''')
t.valid('stage_resource_values', 'answer\nanswer\n', extra=hooks, text='''
extern fn live_environments()->i64;
fn build()->code<(sexpr,fn()->string)> {
 let value=sx_string("answer");return quote{(lift(value),fn()->string{return sx_text(lift(value));})};
}
fn main(){
 {let c=build();var alias=c;alias=alias;let (node,f)=run(alias);println(sx_text(node));println(f());}
 assert(data_live_nodes()==0);assert(live_environments()==0);
}
''')
t.valid('stage_void_order', '1\n2\nbuilt\n3\n3\n', extra=hooks, text='''
fn emit(n:i64)->i64{println(n);return n;}
fn main(){let c=quote{(lift(emit(1)),lift(emit(2)),fn(){println(3);})};println("built");let(a,b,f)=run(c);f();let d=quote{println(3)};run(d);}
''')
t.valid('stage_short_circuit', '7\nbuilt\nfalse\n', extra=hooks, text='''
fn prepare()->code<bool>{println(7);return quote{true};}
fn main(){let c=quote{false&&splice(prepare())};println("built");println(run(c));}
''')
t.valid('stage_shadow_run', '42\n42\n', text='''
fn run(x:i64)->i64{return x+2;}
fn main(){println(run(40));let run=fn(n:i64)->i64{return n;};println(run(42));}
''')
t.valid('stage_macro', '42\n100\n', text='''
macro stage{rule($x:Expression)=>expression{quote{let value=lift($x);value+2}};}
fn main(){let value=100;let n=40;println(run(stage!(n)));println(value);}
''')
t.valid('stage_universal', 'true\n42\n', extra=hooks, text='''
extern fn live_environments()->i64;
fn make(x)->_{return quote{lift(x)};}
fn accept(f:forall(a) fn(a)->code<a>)->i64{println(run(f(true)));return run(f(42));}
fn main(){{println(accept(make));}assert(live_environments()==0);}
''')
reject = [
 ('unquoted_splice', 'fn main(){splice(quote{1});}', 'requires an enclosing quotation'),
 ('unquoted_lift', 'fn main(){lift(1);}', 'requires an enclosing quotation'),
 ('cross_stage', 'fn main(){let n=1;let c=quote{n};}', 'cross-stage reference requires lift'),
 ('cross_stage_assignment', 'fn main(){var n=1;let c=quote{fn(){n=2;}};}', 'cross-stage reference'),
 ('future_escape', 'fn main(){let c=quote{fn(x:i64)->i64{return splice(quote{x});}};}', 'future-stage binding escapes'),
 ('region_cross_stage', 'fn main(){let n=1;let c=quote{let x=n;x};}', 'cross-stage reference requires lift'),
 ('region_future_escape', 'fn main(){let c=quote{let x=1;lift(x)};}', 'future-stage binding escapes'),
 ('region_annotation', 'fn main(){let c:code<bool>=quote{return 42;};}', 'expected bool'),
 ('region_return_path', 'fn main(){let c=quote{if true{return 42;}};}', 'not all paths return'),
 ('region_missing_semicolon', 'fn main(){let c=quote{println(1) println(2)};}', "quotation expression statement requires ';'"),
 ('region_loop_escape', 'fn main(){outer:loop{let c=quote{break outer;};break;}}', 'loop exit has no lexical target'),
 ('bad_splice', 'fn main(){let c=quote{splice(1)};}', 'expected code<'),
 ('bad_run', 'fn main(){run(42);}', 'expected code<'),
 ('bad_arity', 'fn main(){run();}', 'requires one code<T> operand'),
 ('annotation', 'fn main(){let c:code<bool>=quote{42};}', 'expected bool'),
 ('opaque', 'fn main(){let c=quote{42};c();}', 'cannot call value of type code<i64>'),
 ('mutable_hidden_capture', 'fn main(){var n=1;let c=quote{lift(fn()->i64{return n;})};}', 'capturing mutable'),
 ('void_lift', 'fn main(){let c=quote{lift(println(1))};}', 'parameter type cannot be void'),
 ('extern_return', 'extern fn make()->code<i64>;fn main(){}', 'runtime-managed extern ABI'),
 ('extern_param', 'extern fn take(c:code<i64>);fn main(){}', 'runtime-managed extern ABI'),
 ('json_code', 'fn main(){let c=quote{json_parse_or_die("[]")};}', 'resource'),
 ('infinite_code_type', 'fn bad(x)->_{return x(quote{lift(x)});}fn main(){}', 'occurs check failed'),
]
for name, text, diagnostic in reject:
    p = run([t.compiler, '--emit-c', t.source('reject_stage_'+name, text)])
    assert p.returncode == 1 and not p.stdout and diagnostic in p.stderr, (name, p.returncode, p.stderr)
provenance = t.source('reject_stage_region_provenance', '''
fn main(){
 let n=1;
 let c=quote{
  let x=n;
  x
 };
}
''')
p = run([t.compiler, '--emit-c', provenance])
assert p.returncode == 1 and not p.stdout and f'{provenance}:5:9:' in p.stderr, p.stderr
deep = 'fn main(){let c=' + 'quote{'*70 + '1' + '}'*70 + ';}'
p = run([t.compiler, '--emit-c', t.source('reject_stage_depth', deep)])
assert p.returncode == 1 and not p.stdout and 'stage limit exceeded' in p.stderr, p.stderr

# Repeated execution/assignment/discard must not accumulate code environments.
t.valid('stage_loop_cleanup', '', extra=hooks, text='''
extern fn live_environments()->i64;
fn main(){var i=0;while i<10000{{let c=quote{lift(i)};let d=quote{splice(c)+1};assert(run(d)==i+1);}assert(live_environments()==0);i=i+1;}}
''')
source = t.root/'examples/staging.on1x'
a, b = (run([t.compiler, '--emit-c', source]) for _ in range(2))
assert a.returncode == b.returncode == 0 and a.stdout == b.stdout
types_source=t.source('stage_public_types','fn identity_code()->_{return quote{fn(x)->_{return x;}};}fn main(){println(run(identity_code())(42));}')
p=run([t.compiler,'--types',types_source])
assert p.returncode==0 and 'identity_code : forall(a0) fn() -> code<fn(a0) -> a0>\n' in p.stdout, p.stderr or p.stdout
print(f'Typed closed-code AOT MSP: PASS ({len(reject)} rejections)')
