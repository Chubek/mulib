#!/usr/bin/env python3
"""Actual Exotract selected-header archive -> On1x execution and drift refusal."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

cli=argparse.ArgumentParser()
cli.add_argument('exotract',type=Path)
cli.add_argument('on1xc',type=Path)
cli.add_argument('root',type=Path)
cli.add_argument('work',type=Path)
cli.add_argument('cc')
cli.add_argument('--cflag',action='append',default=[])
a=cli.parse_args();a.work.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('ON1X_') or k.startswith('ON1X_TEST_')}

def run(args,check=True):
    p=subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=120,env=env)
    if check:assert p.returncode==0,(p.args,p.stdout,p.stderr)
    return p

with tempfile.TemporaryDirectory(dir=a.work,prefix='run-') as temporary:
    work=Path(temporary);include=work/'kernel headers';include.mkdir()
    header=kernels/'mulib-bitset.h'
    header.write_bytes((a.root.parent/'kernels/mulib-bitset.h').read_bytes()+b'\n/* selected-header SDK fixture */\n')
    archive=work/'archive'
    resource=run(['clang','-print-resource-dir']).stdout.strip()
    run([sys.executable,a.root/'tools/on1x-exotract.py','--mulib-path',include,
         'build','bitset','--output',archive,'--exotract',a.exotract,'--compiler',a.cc,
         '--resource-dir',resource,*['--cflag='+f for f in a.cflag]])
    recipe=json.loads((archive/'build-plan.json').read_text())
    assert recipe['modules'][0]['header_path']==str(header.resolve())
    assert run(['ar','t',archive/'libmulib.a']).stdout=='mulib.o\n'
    shim=work/'adapter.c'
    shim.write_text('''#include <mulib-bitset.h>
#include <stdint.h>
int64_t check_bits(void) {
 mulib_bitset_t b;
 if (!mulib_bitset_init(&b, 8)) return -1;
 (void)mulib_bitset_set(&b, 3, 1);
 int64_t n = (int64_t)mulib_bitset_count(&b);
 mulib_bitset_free(&b); return n;
}
''')
    source=work/'app.on1x';source.write_text('extern fn check_bits()->i64;fn main(){println(check_bits());}')
    exe=work/'app'
    build=[sys.executable,a.root/'tools/on1x-build.py',source,'--on1xc',a.on1xc,'--cc',a.cc,
           '--mulib-path',include,'--mulib','bitset','--mulib-archive',archive,
           '--extra-c',shim,'-o',exe,*['--cflag='+f for f in a.cflag]]
    run(build);assert run([exe]).stdout=='1\n'
    old=exe.read_bytes();header.write_bytes(header.read_bytes()+b'/* drift */\n')
    p=run(build,False)
    assert p.returncode and 'drift' in p.stderr and exe.read_bytes()==old,p
print('Exotract kernel-path archive, one TU, On1x execution and drift rejection: PASS')
