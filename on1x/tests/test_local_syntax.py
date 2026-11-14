from support import NativeTests, run

t = NativeTests()
hooks=['-include',t.root/'tests/fixtures/closure_allocator.h',t.root/'tests/fixtures/closure_allocator.c']
t.valid('local_syntax','42\n1\n99\n42\n',extra=hooks)
t.valid('syntax_parallel_recursive','1\n2\n1\n2\n',text='''
macro helper{rule()=>expression{1};}
fn main(){
 println(let_syntax{macro helper{rule()=>expression{2};}macro call{rule()=>expression{helper!()};}}expression{call!()});
 println(letrec_syntax{macro helper{rule()=>expression{2};}macro call{rule()=>expression{helper!()};}}expression{call!()});
 println(let_syntax{macro helper{rule()=>expression{helper!()};}}expression{helper!()});
 println(letrec_syntax{macro a{rule(done)=>expression{2};rule($x:Syntax)=>expression{b!(done)};}macro b{rule($x:Syntax)=>expression{a!($x)};}}expression{a!(start)});
}
''')
t.valid('syntax_value_capture','42\n42\n40\n',extra=hooks,text='''
extern fn live_environments()->i64;
fn make((x,offset):(i64,i64))->fn()->i64{
 return let_syntax{macro m{rule()=>expression{fn()->i64{return x+offset;}};}}expression{m!()};
}
fn main(){
 {let f=make((40,2));println(f());}
 assert(live_environments()==0);
 var x=40;
 let_syntax{macro m{rule()=>expression{x};}}block{x=42;println(m!());}
 let f=fn(x:i64)->i64{return let_syntax{macro m{rule()=>expression{x};}}expression{m!()};};println(f(40));
}
''')
t.valid('syntax_literal_identity','1\n2\n1\n1\n2\n',text='''
fn main(){let flag=true;
 let_syntax{macro m literals(flag){rule(flag)=>expression{1};rule(_)=>expression{2};}}block{
  println(m!(flag));{let flag=false;println(m!(flag));}println(m!(flag));
 }
 println(letrec_syntax{macro m literals(m){rule(m)=>expression{1};rule(_)=>expression{2};}}expression{m!(m)});
 println(let_syntax{macro m literals(m){rule(m)=>expression{1};rule(_)=>expression{2};}}expression{m!(m)});
}
''')
t.valid('syntax_caller_context','1\n99\n42\n',text='''
macro helper{rule()=>expression{1};}
macro call{rule()=>expression{helper!()};}
macro pass{rule($x:Expression)=>expression{$x};}
macro body{rule($b:Block)=>block{$b;};}
fn main(){
 let_syntax{macro helper{rule()=>expression{99};}}block{println(call!());println(pass!(helper!()));}
 body!({let_syntax{macro m{rule([$x:Expression ...] ...)=>block{println($x)... ...;};}}block{m!([],[42]);}});
}
''')
t.valid('syntax_literal_value_namespace','42\n',text='''
macro choose{rule(println)=>expression{42};rule(_)=>expression{0};}
fn main(){let_syntax{macro println{rule()=>expression{0};}}block{println(choose!(println));}}
''')
t.valid('syntax_definition_assignment','42\n',text='''
fn main(){var n=40;
 let_syntax{macro bump{rule()=>block{n=n+2;};}}block{var n=0;bump!();assert(n==0);}
 println(n);
}
''')
t.valid('syntax_namespaces_and_capture','42\n7\n42\n',text='''
fn m(x:i64)->i64{return x;}
fn main(){
 let_syntax{macro m{rule($x:Expression)=>expression{$x+2};}macro bind{rule($n:Identifier,$x:Expression)=>block{let $n=$x;};}}block{
  println(m(m!(40)));bind!(answer,7);println(answer);
 }
 println(let_syntax{}expression{42});let_syntax{}block{}
}
''')
t.valid('syntax_local_captures','42\n42\ntrue\n',text='''
macro pass{rule($x:Expression)=>expression{$x};}
macro block{rule($b:Block)=>block{$b;};}
fn main(){
 println(pass!(let_syntax{macro m{rule()=>expression{42};}}expression{m!()}));
 block!(let_syntax{macro m{rule()=>block{println(42);};}}block{m!();});
 let identity=let_syntax{macro id{rule()=>expression{fn(x)->_{return x;}};}}expression{id!()};
 assert(identity(42)==42);println(identity(true));
}
''')
t.valid('syntax_stage_and_shadow','42\n42\n',extra=hooks,text='''
fn main(){let n=40;
 let c=let_syntax{macro m{rule()=>expression{quote{lift(n)+2}};}}expression{m!()};println(run(c));
 let_syntax{macro m{rule()=>expression{n+2};}}block{let on1x_hygiene_1=0;let n=0;println(m!());}
}
''')
reject=[
 ('escape','fn main(){let_syntax{macro m{rule()=>expression{1};}}block{}m!();}','undefined macro m'),
 ('siblings_parallel','fn main(){let_syntax{macro a{rule()=>expression{b!()};}macro b{rule()=>expression{1};}}block{a!();}}','undefined macro b'),
 ('recursive_limit','fn main(){letrec_syntax{macro m{rule()=>expression{m!()};}}block{m!();}}','limit exceeded'),
 ('duplicate','fn main(){let_syntax{macro m{rule()=>expression{1};}macro m{rule()=>expression{2};}}block{}}','duplicate macro m'),
 ('unknown_category','fn main(){let_syntax{macro m{rule($x:Bad)=>expression{1};}}block{}}','unsupported syntax category'),
 ('unused_bad_template','fn main(){let_syntax{macro m{rule()=>expression{$missing};}}block{}}','unknown template variable'),
 ('body_value_escape','fn main(){let_syntax{}block{let hidden=1;}println(hidden);}','undefined variable: hidden'),
 ('definition_future','fn main(){let_syntax{macro m{rule()=>expression{x};}}block{let x=42;println(m!());}}','undefined variable: x'),
 ('mutable_closure','fn main(){var x=1;let_syntax{macro m{rule()=>expression{fn()->i64{return x;}};}}block{let f=m!();}}','capturing mutable binding'),
 ('block_value','fn main(){let x=let_syntax{}block{};}','block local syntax used in expression position'),
 ('generated_definitions','macro m{rule()=>expression{let_syntax{}expression{1}};}fn main(){}','cannot be generated by a macro template'),
 ('bad_body','fn main(){let_syntax{}1;}','expected'),
 ('json_capture','fn main(){let root=json_parse_or_die("{}");let_syntax{macro m{rule()=>expression{fn(){json_print(root);}};}}block{let f=m!();}}','capturing JSON'),
]
for name,text,diagnostic in reject:
 p=run([t.compiler,'--emit-c',t.source('reject_syntax_'+name,text)])
 assert p.returncode==1 and not p.stdout and diagnostic in p.stderr,(name,p.returncode,p.stderr)

# Forwarding captured definitions must not retain dangling AST pointers after a
# local scope is erased; recursive groups must not leak compiler environments.
t.valid('syntax_nested_definition_environment','42\n',text='''
fn main(){let x=40;
 let_syntax{macro outer{rule($x:Expression)=>expression{$x+2};}}block{
  let_syntax{macro captured{rule()=>expression{outer!(x)};}}block{
   let_syntax{macro outer{rule($x:Expression)=>expression{0};}}block{println(captured!());}
  }
 }
}
''')
source=t.root/'examples/local_syntax.on1x'
a,b=(run([t.compiler,'--emit-c',source]) for _ in range(2))
assert a.returncode==b.returncode==0 and a.stdout==b.stdout
wire=run([t.compiler,'--emit-ast',source]);assert wire.returncode==0 and 'SyntaxLet' not in wire.stdout,wire.stderr
for name,text,diagnostic in [
 ('syntax_group_count','fn main(){let_syntax{'+''.join('macro m'+str(i)+'{rule()=>expression{1};}' for i in range(513))+'}block{}}','local macro definition limit'),
 ('syntax_unit_count','fn main(){'+('let_syntax{'+''.join('macro m'+str(i)+'{rule()=>expression{1};}' for i in range(512))+'}block{}')*9+'}','lexical macro definition limit'),
 ('syntax_environment_bytes','fn main(){'+''.join('let n'+str(i)+'=1;' for i in range(2000))+'let_syntax{macro m{rule()=>expression{1};}}block{}'*100+'}','expanded syntax byte limit'),
 ('syntax_copied_definition_bytes','macro many{rule($b:Block)=>block{'+('$b;'*10)+'};}fn main(){many!(let_syntax{macro m{rule()=>expression{"'+('x'*1000000)+'"};}}block{});}','expanded syntax byte limit'),
]:
 p=run([t.compiler,'--emit-c',t.source(name,text)])
 assert p.returncode==1 and not p.stdout and diagnostic in p.stderr,(name,p.returncode,p.stderr)
print(f'Lexical let_syntax/letrec_syntax: PASS ({len(reject)} rejections)')
