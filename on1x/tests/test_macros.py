import json

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

operator_template = t.source('operator_template', '''
macro calculate {
  rule ([$left:Identifier, $right:Identifier, $result:Expression] ...) => block {
    syntax_block {
      operator (lexeme: "<+>", arity: "binary", precedence: 5,
                formals: $left: i64, $right: i64) -> i64 {
        let template_local = $left * 10;
        return template_local + $result;
      }
      println(4 <+> 2);
    }...;
  };
}
fn main() {
  let template_local = 99;
  calculate!([a, b, b], [a, b, b + 1]);
  calculate!([a, b, b + 2]);
  println(template_local);
}
''')
t.valid('operator_template', '42\n43\n44\n99\n', source=operator_template)
first = run([t.compiler, '--operator-info', operator_template])
second = run([t.compiler, '--operator-info', operator_template])
assert first.returncode == second.returncode == 0, (first.stderr, second.stderr)
assert first.stdout == second.stdout, 'generated operator inspection is nondeterministic'
listing = json.loads(first.stdout)
declarations = {i: d for i, d in enumerate(listing['declarations']) if d['lexeme'] == '<+>'}
assert len(declarations) == 3, declarations
assert len({(d['file'], d['offset']) for d in declarations.values()}) == 1, declarations
assert sorted(u['declaration'] for u in listing['uses'] if u['declaration'] in declarations) == sorted(declarations)

# Definition-site notation/fixity must not follow the importing caller's same
# spelling. Captured descriptors and expressions keep their use-site binding.
t.source('operator_macro_lib', '''
operator (lexeme: "<^>", arity: "binary", precedence: 5,
          formals: a: i64, b: i64) -> i64 { return a * 10 + b; }
fn helper(value: i64) -> i64 { return value + 3; }
macro authored {
  rule ($op:CustomOperator, $operand:Expression) => block {
    infixr 5 <^>;
    println(1 <^> 2 <^> 3);
    println(ast_binary($op, 4, 5));
    println($operand);
    operator (lexeme: "<~>", arity: "unary", formals: operand: i64) -> i64 {
      return helper(operand);
    }
    println(<~> 39);
  };
}
''')
t.valid('operator_macro_import', '33\n405\n10203\n42\n10203\n', text='''
from .operator_macro_lib import authored;
operator (lexeme: "<^>", arity: "binary", precedence: 5,
          formals: a: i64, b: i64) -> i64 { return a * 100 + b; }
fn main() {
  authored!(operator("<^>"), 1 <^> 2 <^> 3);
  println(1 <^> 2 <^> 3);
}
''')

# Captured syntax can be discarded or copied without hoisting uninstantiated
# declarations. A nested definition sees its enclosing copied binding, and
# direct operator patterns match identity rather than punctuation.
t.valid('captured_operator_syntax', '33\n1\n2\n42\n33\n1\n2\n42\n', text='''
macro discard { rule ($body:Block) => block {}; }
macro replay { rule ($body:Block) => block { $body; $body; }; }
fn main() {
  discard!({
    operator (lexeme: "<~>", arity: "unary", formals: n: i64) -> i64 { return unbound(n); }
  });
  replay!({
    operator (lexeme: "<+>", arity: "binary", formals: a: i64, b: i64) -> i64 {
      return a * 10 + b;
    }
    infixr 5 <+>;
    println(1 <+> 2 <+> 3);
    let_syntax {
      macro which {
        rule (operator("<+>")) => expression { 1 };
        rule ($x:CustomOperator) => expression { 2 };
      }
      macro increment { rule ($x:Expression) => expression { $x + 1 }; }
    } block {
      println(which!(operator("<+>")));
      {
        operator (lexeme: "<+>", arity: "binary", formals: a: i64, b: i64) -> i64 {
          return increment!(a + b);
        }
        println(which!(operator("<+>")));
        println(20 <+> 21);
      }
    }
  });
}
''')

t.valid('operator_body_rewrite', '42\n', text='''
macro double { rule ($value:Expression) => expression { $value * 2 }; }
operator (lexeme: "<+>", arity: "binary", formals: left: i64, right: i64) -> i64 {
  return double!(left + right);
}
fn main() { println(20 <+> 1); }
''')
t.valid('operator_body_block_hole', '42\n', text='''
macro wrap {
  rule ($a:Identifier, $b:Identifier, $body:Block) => block {
    operator (lexeme: "<+>", arity: "binary", formals: $a: i64, $b: i64) -> i64 {
      $body;
    }
    println(20 <+> 21);
  };
}
fn main() { wrap!(left, right, { return left + right + 1; }); }
''')

t.valid('macro_fixity_choice', '33\n123\n123\n123\n', text='''
operator (lexeme: "<+>", arity: "binary", formals: a: i64, b: i64) -> i64 {
  return a * 10 + b;
}
macro regroup {
  rule (true) => block { infixr 5 <+>; println(1 <+> 2 <+> 3); println((1 <+> 2) <+> 3); };
  rule (false) => block { infix 5 <+>; println(1 <+> 2 <+> 3); };
}
fn main() { regroup!(true); regroup!(false); println(1 <+> 2 <+> 3); }
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
  ('operator_formal_category', '''
     macro m { rule ($bad:Expression) => block {
       operator (lexeme:"<+>", arity:"unary", formals:$bad:i64) -> i64 { return 1; }
     }; } fn main() {}
   ''', 'binding/name hole requires one Identifier'),
  ('operator_formal_collision', '''
     macro m { rule ($name:Identifier) => block {
       operator (lexeme:"<+>", arity:"binary", formals:$name:i64,a:i64) -> i64 { return 1; }
     }; } fn main() { m!(a); }
   ''', 'duplicate operator formal: a'),
  ('operator_no_value_capture', '''
     fn a() -> i64 { return 40; }
     macro m { rule ($free:Expression) => block {
       operator (lexeme:"<+>", arity:"binary", formals:a:i64,b:i64) -> i64 { return $free; }
       println(1 <+> 2);
     }; } fn main() { let a = 41; m!(a); }
   ''', 'operator body cannot capture outer value: a'),
  ('operator_body_definition_site', '''
     macro free { rule () => expression { a }; }
     operator (lexeme:"<+>", arity:"binary", formals:a:i64,b:i64) -> i64 { return free!(); }
     fn main() { println(1 <+> 2); }
   ''', 'undefined variable: a'),
  ('operator_no_syntax_export', '''
     macro m { rule () => block {
       operator (lexeme:"<+>", arity:"binary", formals:a:i64,b:i64) -> i64 { return a+b; }
     }; } fn main() { m!(); println(1 <+> 2); }
   ''', 'no visible trailing operator'),
  ('operator_literal_header', '''
     macro m { rule ($lexeme:StringLiteral) => block {
       operator (lexeme:$lexeme, arity:"binary", formals:a:i64,b:i64) -> i64 { return a+b; }
     }; } fn main() {}
   ''', 'macro operator header attributes must be literal'),
  ('operator_literal_fixity', '''
     operator (lexeme:"<+>", arity:"binary", formals:a:i64,b:i64) -> i64 { return a+b; }
     macro m { rule ($rank:IntegerLiteral) => block { infixr $rank <+>; }; }
     fn main() {}
   ''', 'macro fixity directives require a literal tightness'),
  ('operator_no_definition_capture', '''
     macro m { rule () => block { infixr 5 <+>; }; }
     fn main() {
       operator (lexeme:"<+>", arity:"binary", formals:a:i64,b:i64) -> i64 { return a+b; }
       m!();
     }
   ''', 'fixity directive requires a visible infix operator'),
 ]
for name,text,diagnostic in reject:
    p=run([t.compiler,'--emit-c',t.source('reject_'+name,text)])
    assert p.returncode==1 and diagnostic in p.stderr and not p.stdout,(name,p.returncode,p.stderr)
operator_limit = '''
macro many {
  rule ($n:IntegerLiteral ...) => block {
    syntax_block {
      operator (lexeme:"<+>", arity:"unary", formals:n:i64) -> i64 { return n + $n; }
    }...;
  };
}
fn main() { many!(%s); }
''' % ','.join(['1'] * 513)
p = run([t.compiler, '--emit-c', t.source('macro_operator_limit', operator_limit)])
assert p.returncode == 1 and 'generated operator declaration limit exceeded' in p.stderr and not p.stdout, p.stderr
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
