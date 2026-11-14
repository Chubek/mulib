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
if mode=='empty':
    raise SystemExit
if mode=='deep':
    print('('*300+'x'+')'*300);raise SystemExit
if mode=='ignore_input':
    print('(Unit 1 ((Function "main" () (VoidType) (Block () (Loc "generated" 0 1 1)) () (Loc "generated" 0 1 1))))');raise SystemExit
tree=loads(sys.stdin.buffer.read())
if mode=='wrong_version':tree[1]=Atom('2')
if mode=='wrong_field':tree[2]='not a function list'
if mode=='internal_node':tree[2][0][4][1]=[[Atom('PolyPack')]]
if mode=='duplicate':tree[2].append(tree[2][0])
if mode=='unbound_type':tree[2][0][3]=[Atom('BoundType'),Atom('0')]
if mode=='huge_binders':tree[2][0][3]=[Atom('ForallType'),Atom(str((1<<64)-1)),[Atom('FunctionType'),[[Atom('BoundType'),Atom('0')]],[Atom('BoundType'),Atom('0')]]]
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
