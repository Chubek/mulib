#!/usr/bin/env python3
"""Exercise exclusive replacement and its managed lifetimes in generated C11."""
import os

from support import NativeTests, run


t = NativeTests()
fixtures = t.root / 'tests/fixtures'
allocator = ('-include', fixtures / 'unique_allocator.h',
             fixtures / 'unique_allocator.c')

source = t.source('ref_replace_lifecycle', r'''
import std.container.Box as Box;
extern fn on1x_unique_live_allocations() -> i64;

# Replacing an RC parameter must not release the caller's slot. Cover the
# original := path, the new exchange, and an aggregate with an RC field.
record Cell { value:sexpr }
fn update(x:sexpr, next:sexpr) {
  let place = borrow mut x;
  place := next;
  assert(sx_text(place) == "new");
}
fn swap_copy(x:Cell, next:sexpr)->Cell {
  let place = borrow mut x;
  let nested = borrow mut place;
  return ref_replace(nested, record<Cell>{value:next});
}
fn swap_bytes(x:buffer, next:buffer)->buffer {
  let place=borrow mut x;
  let previous=ref_replace(place,next);
  assert(buffer_len(place)==0);
  return previous;
}
fn keep(x:borrow Box.Box<own<Box.Box<i64>>>) {}
fn transfer_or_leave(place:ref i64, candidate:Result<i64,u8>)->Result<i64,u8> {
  defer println(7);
  let previous=ref_replace(place, candidate?);
  return variant<Result<i64,u8>>(Ok, previous);
}
fn exercise() {
  let owner = box(record<Box.Box<own<Box.Box<i64>>>>{value:Box.basis_box_new(1)});
  assert(on1x_unique_live_allocations() == 2);
  var count = 2;
  while count <= 32 {
    let next = Box.basis_box_new(count);
    let previous = Box.basis_box_exchange_box(owner, move(next));
    assert(on1x_unique_live_allocations() == 3);
    assert(Box.basis_box_into_inner(move(previous)) == count-1);
    assert(on1x_unique_live_allocations() == 2);
    count = count+1;
  }
  keep(owner);
  let previous = Box.basis_box_exchange_box(owner, Box.basis_box_new(33));
  assert(Box.basis_box_into_inner(move(previous)) == 32);
}
fn main() {
  assert(data_live_nodes()==0);
  {
    let copy=sx_string("old");
    let next=sx_string("new");
    update(copy,next);
    let old = swap_copy(record<Cell>{value:copy},next);
    assert(data_live_nodes()==2 && sx_text(copy)=="old" && sx_text(old["value"])=="old");
  }
  {
    let bytes=b!"A\0\xff";
    let old=swap_bytes(bytes,b!"");
    assert(buffer_len(old)==3 && buffer_at(old,1)==0u8 && buffer_at(old,2)==255u8);
    assert(buffer_len(bytes)==3 && data_live_nodes()==1);
  }
  assert(data_live_nodes()==0);
  let x=1;
  assert(match transfer_or_leave(x,variant<Result<i64,u8>>(Err,4u8))
    with {Err(e)=>e==4u8,Ok(_)=>false});
  assert(x==1);
  assert(match transfer_or_leave(x,variant<Result<i64,u8>>(Ok,2))
    with {Err(_)=>false,Ok(old)=>old==1});
  assert(x==2);
  var rounds=0;
  while rounds<100 {
    exercise();
    assert(on1x_unique_live_allocations()==0);
    rounds=rounds+1;
  }
  println("exclusive replacement lifecycle");
}
''')
exe = t.build('ref_replace_lifecycle', source=source, extra=(*allocator, '-O2'))
env = {k: v for k, v in os.environ.items() if k != 'ON1X_TEST_UNIQUE_FAIL_AFTER'}
expected = '7\n7\nexclusive replacement lifecycle\n'
result = run([exe], env=env)
assert (result.returncode, result.stdout, result.stderr) == (0, expected, ''), result
for fault in (0, 1, 2, 15, 33, 34, 3399):
    result = run([exe], env={**env, 'ON1X_TEST_UNIQUE_FAIL_AFTER': str(fault)})
    assert result.returncode == 72 and result.stdout == '7\n7\n', (fault, result)
    assert 'owning-box allocation failure' in result.stderr, (fault, result)

t.valid('ref_replace_shadowed', '2\n3\n', text='''
fn ref_replace(n:i64)->i64 { return n+1; }
fn main() {
  println(ref_replace(1));
  let ref_replace=fn(n:i64)->i64 {return n+1;};
  println(ref_replace(2));
}
''')

for name, body, diagnostic in (
    ('plain', 'fn main(){var x=1;ref_replace(x,2);}', 'named exclusive borrowed place'),
    ('temporary', 'fn main(){ref_replace(1,2);}', 'named exclusive borrowed place'),
    ('shared', 'fn f(x:borrow i64){ref_replace(x,2);}fn main(){}',
     'named exclusive borrowed place'),
    ('read_target', 'fn f(x:ref i64){ref_replace(x,x+1);}fn main(){}',
     'during its exclusive borrow'),
    ('shared_child', 'fn f(x:ref i64){let r=borrow x;ref_replace(x,2);}fn main(){}',
     'overlapping exclusive borrow'),
    ('nested_replace', 'fn f(x:ref i64){ref_replace(x,ref_replace(x,2));}fn main(){}',
     'overlapping exclusive borrow'),
    ('type', 'fn f(x:ref i64){ref_replace(x,false);}fn main(){}', 'i64'),
    ('defer', 'fn f(x:ref i64){defer ref_replace(x,2);}fn main(){}',
     'defer cannot retain a borrowed call'),
    ('not_first_class', 'fn main(){let f=ref_replace;}', 'ref_replace'),
    ('missing_move', '''record Box{value:own<Item>}record Item{}
      fn f(x:ref Box,y:Box){ref_replace(x,y);}fn main(){}''',
     'copy of unique local requires move(local)'),
    ('twice', '''record Box{value:own<Item>}record Item{}
      fn f(x:ref Box,y:Box){drop(ref_replace(x,move(y)));drop(y);}fn main(){}''',
     'use after move'),
):
    src = t.source('ref_replace_bad_' + name, body)
    result = run([t.compiler, '--emit-c', src])
    assert result.returncode == 1 and not result.stdout, (name, result)
    assert diagnostic in result.stderr and ': error:' in result.stderr, (name, result.stderr)

mir = run([t.compiler, '--emit-ir', source])
assert mir.returncode == 1 and not mir.stdout and 'ON1X-MIR-UNSUPPORTED' in mir.stderr, mir

print('ref_replace: lexical rejection, RC slots, typed early exit, and 3,400 unique box lifecycles')
