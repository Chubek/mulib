#!/usr/bin/env python3
"""Compare C11 and saved VM artifacts against slice snapshot and bounds oracles.

Run from the repository root after building on1xc and on1x-vm:
  python3 -B on1x/tests/test_acceptance_slice_snapshots.py \
    "$PWD/build/on1x/on1xc" "$PWD/on1x" "$PWD/build/slice-acceptance" clang
"""
from support import NativeTests, run

t = NativeTests()
runner = t.compiler.with_name('on1x-vm')


def check(name, outer, inner, index):
    # Keep the actual storage distinct from the source-language oracle. In
    # particular, Python's negative indexing must not turn a trap into success.
    values = (11, 23, 37, 53, 71, 89)
    lo, hi = outer
    start, end = inner
    first = values[lo:hi]
    output = f'{lo}\n{hi}\n{start}\n{end}\n'
    if not 0 <= start <= end <= len(first):
        trap = 'slice bounds out of range'
        location = 'let window'
    else:
        snapshot = first[start:end]
        output += f'{len(snapshot)}\n{index}\n'
        if 0 <= index < len(snapshot):
            output += f'{snapshot[index]}\n990\n'
            trap = None
        else:
            trap = 'index out of bounds'
            location = 'println(window[mark'
    text = f'''fn mark(x:i64)->i64 {{ println(x); return x; }}
fn main() {{
  var backing = array[{','.join(map(str, values))}];
  let first = backing[mark({lo}):mark({hi})];
  backing = array[990,991,992,993,994,995];
  let window = first[mark({start}):mark({end})];
  println(window[]);
  println(window[mark({index})]);
  println(backing[0]);
}}
'''
    source = t.source(name, text)
    status = 70 if trap else 0
    native = run([t.build(name, source=source)])
    assert (native.returncode, native.stdout) == (status, output), (name, native)
    if trap:
        line = next(n for n, row in enumerate(text.splitlines(), 1) if location in row)
        assert f'{source}:{line}:' in native.stderr and trap in native.stderr, (name, native)
    else:
        assert not native.stderr, (name, native)

    for options in ([], ['--optimize-ir']):
        label = 'optimized' if options else 'plain'
        image = t.work / f'{name}-{label}.o1vm'
        result = run([t.compiler, '--emit-vm', source, *options, '-o', image])
        assert result.returncode == 0, (name, label, result)
        result = run([runner, '--verify', image])
        assert result.returncode == 0, (name, label, result)
        guest = run([runner, image])
        assert (guest.returncode, guest.stdout) == (status, output), (name, label, guest)
        if trap:
            # VM product lowering currently represents bounds checks as MIR
            # assertions; C11 names the specific bounds failure.
            assert f'{source}:{line}:' in guest.stderr, (name, label, guest)
            assert ': ON1X-VM: assertion failed' in guest.stderr, (name, label, guest)
        else:
            assert not guest.stderr, (name, label, guest)


check('nested_first', (1, 6), (2, 4), 0)
check('nested_last', (1, 6), (2, 4), 1)
# These offsets would index valid backing-array elements if the implementation
# checked capacity or rebased the offset before checking the logical window.
check('nested_past_end', (1, 6), (2, 4), 2)
check('nested_negative', (1, 6), (2, 4), -1)
check('nested_empty', (1, 6), (5, 5), 0)
check('inner_past_first', (1, 4), (1, 4), 0)
check('inner_reversed', (1, 6), (3, 2), 0)
check('inner_negative', (1, 6), (-1, 2), 0)
print('Slice snapshots: 8 cases passed in direct C11 and both saved VM modes.')
