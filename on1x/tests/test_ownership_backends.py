#!/usr/bin/env python3
"""Compare checked lexical loan execution with the direct-C11 reference."""
import argparse
from pathlib import Path
import re

from support import run


cli = argparse.ArgumentParser()
for name in ('compiler', 'vm', 'root', 'work'):
    cli.add_argument(name, type=Path)
cli.add_argument('cc')
cli.add_argument('--cflag', action='append', default=[])
cli.add_argument('--archon', nargs=2, type=Path, metavar=('DRIVER', 'SCALAR_ABI'))
cli.add_argument('--cephyr', action='store_true')
args = cli.parse_args()
args.work = args.work.resolve()
args.work.mkdir(parents=True, exist_ok=True)


def success(*command):
    p = run(command)
    assert p.returncode == 0, (command, p.returncode, p.stdout, p.stderr)
    return p


runtime = args.work / 'runtime.o'
strict = ('-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
          '-I', args.root / 'runtime', *args.cflag)
success(args.cc, *strict, '-c', args.root / 'runtime/on1x_runtime.c', '-o', runtime)


def c11(source, target, mode, options):
    success(args.compiler, mode, *options, source, '-o', target.with_suffix('.c'))
    success(args.cc, *strict, target.with_suffix('.c'), runtime, '-lm', '-o', target)
    return run([target])


def exercise(name, text, stdout, *, fault=None, status=0):
    source = args.work / (name + '.on1x')
    source.write_text(text, encoding='utf-8')
    results = {'direct C11': c11(source, args.work / (name + '-reference'), '--emit-c', [])}
    if args.cephyr:
        obj = args.work / (name + '-cephyr.o')
        exe = obj.with_suffix('')
        success(args.compiler, '--emit-cephyr-object', '--cephyr-target', 'x86_64-linux-gnu',
                source, '-o', obj)
        success(args.cc, *args.cflag, obj, runtime, '-lm', '-o', exe)
        results['Cephyr'] = run([exe])
    for profile, flags in [('plain', ['--opt-profile', 'debug']),
                           ('optimized', ['--opt-profile', 'release'])]:
        base = args.work / (name + '-' + profile)
        results[profile + ' MIR C11'] = c11(source, base, '--emit-ir-c', flags)
        ir = base.with_suffix('.mir')
        success(args.compiler, '--emit-ir', *flags, source, '-o', ir)
        results[profile + ' saved MIR'] = run([args.compiler, '--ir-input', '--run-ir', ir])
        image = base.with_suffix('.o1vm')
        success(args.compiler, '--emit-vm', *flags, source, '-o', image)
        success(args.vm, '--verify', image)
        results[profile + ' saved VM'] = run([args.vm, image])
        if args.archon:
            driver, abi = args.archon
            assembly, executable = base.with_suffix('.s'), base.with_suffix('.native')
            success(driver, '--ir-input', ir, '-o', assembly)
            success(args.cc, *args.cflag, assembly, abi, runtime, '-lm', '-o', executable)
            results[profile + ' Archon'] = run([executable])
    for profile, result in results.items():
        assert (result.returncode, result.stdout) == (status, stdout), (name, profile, result)
        if fault:
            anchor, message = fault
            line = next(i for i, row in enumerate(text.splitlines(), 1) if anchor in row)
            assert message in result.stderr, (name, profile, result.stderr)
            # C11 and the native scalar runtime use the existing unlocated
            # arithmetic helper. The interpreter products carry a source point.
            if profile.endswith(('saved MIR', 'saved VM')):
                assert f'{source}:{line}:' in result.stderr, (name, profile, result.stderr)
        else:
            assert not result.stderr, (name, profile, result.stderr)


exercise('lexical_root_identity', '''
fn mark(value:i64)->i64 { println(value); return value; }
fn adjust(value:i64, grow:bool)->i64 {
  {
    let edit=borrow mut value;
    {
      let edit=borrow mut edit;
      edit := edit + mark(5);
    }
    {
      let view=borrow edit;
      let nested=borrow view;
      println(nested);
      let value=100;
      println(value+view);
    }
    if grow { edit := edit+mark(7); }
  }
  return value;
}
fn main()->i64 {
  println(adjust(3,true));
  println(adjust(30,false));
  return 7;
}
''', '5\n8\n108\n7\n15\n5\n35\n135\n35\n', status=7)

exercise('loans_on_loop_edges', '''
fn main() {
  var total=0;
  outer:for i in 0..6 {
    let edit=borrow mut total;
    if i==4 { break outer; }
    let next=edit+i;
    edit := next;
    {
      let view=borrow edit;
      if i==2 { continue outer; }
      println(view);
    }
  }
  {
    let a=borrow total;
    let b=borrow total;
    assert(a==b);
    println(a);
  }
}
''', '0\n1\n6\n6\n')

exercise('ref_store_first_fault', '''
fn mark(value:i64)->i64 { println(value); return value; }
fn update(divisor:i64) {
  var number=4;
  let edit=borrow mut number;
  edit := mark(11) / divisor + mark(12);
  println(edit);
}
fn main() { update(2); update(0); println(99); }
''', '11\n12\n17\n11\n', fault=('edit := mark', 'invalid division'), status=70)

exercise('scalar_storage_types', '''
fn main() {
  let flag=false;
  let count=42u16;
  {
    let edit_flag=borrow mut flag;
    let edit_count=borrow mut count;
    edit_flag := !edit_flag;
    if edit_flag { edit_count := 9u16; }
  }
  println(flag);
  println(count);
  {
    let left=borrow count;
    let right=borrow count;
    println(left<=right);
    println(left<right);
  }
}
''', 'true\n9\ntrue\nfalse\n')

exercise('exclusive_exchange', '''
fn mark(value:i64)->i64 { println(value); return value; }
fn replace(stop:bool)->i64 {
  let point=10;
  let edit=borrow mut point;
  let old=ref_replace(edit, match stop with {
    true=>{return 77;},
    false=>mark(32)
  });
  println(old);
  return edit;
}
fn main() {
  println(replace(true));
  println(replace(false));
}
''', '77\n32\n10\n32\n')

# The pointer-free fixture must use one MIR place for every access to the
# root, even through sequential aliases. Check plain serialization separately
# from output equality, which could also be achieved with copy/writeback.
structure = args.work / 'single_slot.on1x'
structure.write_text('''
fn main() {
  let point=1;
  {
    let edit=borrow mut point;
    let nested=borrow mut edit;
    nested:=nested+4;
  }
  { let view=borrow point; println(view); }
}
''', encoding='utf-8')
ir = success(args.compiler, '--emit-ir', '--opt-profile', 'debug', structure).stdout
slot_ids = re.findall(r'\(value (\d+) i64 \(at ', ir)
access_ids = re.findall(r'\(instruction \d+ (?:load|store) (?:i64|void) '
                        r'\([^)]*\) 0 (\d+) ', ir)
assert len(slot_ids) == 1 and len(access_ids) == 4 and set(access_ids) == set(slot_ids), ir

for name, text, diagnostic in [
    ('loan_conflict', 'fn main(){var x=1;let r=borrow x;let m=borrow mut x;println(r);println(m);}',
     'overlapping exclusive borrow'),
    ('shared_write', 'fn main(){let x=1;let r=borrow x;r:=2;}',
     'requires an exclusive borrowed place'),
    ('exchange_rhs_overlap', 'fn main(){let x=1;let r=borrow mut x;println(ref_replace(r,r+1));}',
     'during its exclusive borrow'),
]:
    source = args.work / (name + '.on1x')
    source.write_text(text, encoding='utf-8')
    for mode in ('--emit-c', '--emit-ir', '--emit-vm'):
        output = args.work / (name + '-artifact')
        result = run([args.compiler, mode, source, '-o', output])
        assert result.returncode == 1 and str(source) in result.stderr and diagnostic in result.stderr, (
            name, mode, result)
        assert not output.exists(), (name, mode, output)

for name, text in [
    ('borrowed_parameter', '''
fn get(value:borrow i64)->i64 { return value; }
fn main() { let point=42; println(get(point)); }
'''),
    ('owner_payload', '''
record Cell { value:i64 }
fn main() {
  let owner=box(record<Cell>{value:42});
  { let view=borrow owner; println(view["value"]); }
  drop(owner);
}
'''),
]:
    source = args.work / (name + '.on1x')
    source.write_text(text, encoding='utf-8')
    reference = c11(source, args.work / (name + '-reference'), '--emit-c', [])
    assert (reference.returncode, reference.stdout, reference.stderr) == (0, '42\n', ''), reference
    for mode in ('--emit-ir-c', '--emit-ir', '--emit-vm'):
        output = args.work / (name + '-unpublished')
        result = run([args.compiler, mode, source, '-o', output])
        assert result.returncode == 1 and 'ON1X-MIR-UNSUPPORTED' in result.stderr, result
        assert not output.exists(), (name, mode, output)

print('Scoped loan differential: direct C11, MIR C11, saved MIR and saved VM' +
      (', Archon AMD64' if args.archon else '') + (', Cephyr AMD64' if args.cephyr else '') +
      ' passed.')
