#!/usr/bin/env python3
"""Adversarial protocol fixture. A PPX label selects the behavior."""
import os
from pathlib import Path
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'stdlib'))
from sexpr import Atom, loads, dumps
from visitor import rewrite, tag
mode=os.environ['ON1X_PPX_LABEL']
if mode=='forkhang':
    if os.fork()==0:
        Path(os.environ['ON1X_TEST_PIDFILE']).write_text(str(os.getpid()))
        time.sleep(60)
        os._exit(0)
if mode=='timeout':
    time.sleep(60)
if mode=='flood':
    sys.stdout.write('x'*(4*1024*1024+1));raise SystemExit
if mode=='stderr_flood':
    sys.stderr.write('x'*65537);raise SystemExit
if mode=='malformed':
    print('(broken');raise SystemExit
if mode=='exit_error':
    print('fixture failure',file=sys.stderr);raise SystemExit(23)
if mode=='exit_control':
    sys.stderr.buffer.write(b'forged line\n\x1b[31mred\x1b[0m');raise SystemExit(23)
if mode=='empty':
    raise SystemExit
if mode=='deep':
    print('('*300+'x'+')'*300);raise SystemExit
if mode=='ignore_input':
    print('(Unit 1 ((Function "main" () (VoidType) (Block () (Loc "generated" 0 1 1)) () (Loc "generated" 0 1 1))))');raise SystemExit
tree=loads(sys.stdin.buffer.read())
if mode in ('docstrings_drop', 'docstrings_check'):
    docs=[n for n in tree[2] if tag(n)=='Docstring']
    assert [(n[1], n[2]) for n in docs] == [
        ('>', ' Imported. '), ('!', ' Main\n  details. '),
        ('>', ' Embedded. '), ('!', ' Trailing. ')]
    assert [(Path(n[3][1]).name, int(n[3][3]), int(n[3][4])) for n in docs] == [
        ('doc_provider.on1x', 1, 1), ('doc_input.on1x', 2, 1),
        ('doc_input.on1x', 4, 28), ('doc_input.on1x', 5, 1)]
    if mode=='docstrings_drop':
        tree[2]=[n for n in tree[2] if tag(n)!='Docstring']
if mode=='hostile_diagnostic':
    identity=os.environ['ON1X_PPX_DIAGNOSTIC_ID']
    os.write(3,(f'(register "{identity}")\n'
                f'(emit "{identity}" (Loc "ppx.on1x" 0 1 1))\n').encode())
function=next(n for n in tree[2] if tag(n)=='Function')
if mode=='wrong_version':tree[1]=Atom('2')
if mode=='wrong_field':tree[2]='not a function list'
if mode=='internal_node':function[4][1]=[[Atom('PolyPack')]]
if mode=='duplicate':tree[2].append(function)
if mode=='unbound_type':function[3]=[Atom('BoundType'),Atom('0')]
if mode=='huge_binders':function[3]=[Atom('ForallType'),Atom(str((1<<64)-1)),[Atom('FunctionType'),[[Atom('BoundType'),Atom('0')]],[Atom('BoundType'),Atom('0')]]]
if mode=='unknown':tree=[Atom('Unrecognized')]
if mode=='trailing':print(dumps(tree)+'junk');raise SystemExit

def transform(n,path):
    if tag(n)=='DecimalLiteral':
        if mode=='increment':return [n[0],str(int(n[1])+1),n[2]]
        if mode=='double':return [n[0],str(int(n[1])*2),n[2]]
        if mode=='introduce':return [Atom('introduce:Box'),n,n[2]]
        if mode=='bad_semantics':return [Atom('Binary'),Atom('+'),n,[Atom('BooleanLiteral'),Atom('true'),n[2]],n[2]]
    if mode=='consume' and tag(n)=='introduce:Box':return n[1]
    return n
print(dumps(rewrite(tree,transform)),end='')
