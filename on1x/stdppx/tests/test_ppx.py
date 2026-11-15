"""Schema-valid end-to-end PPX transformations, protocol behavior and rejection tests."""
import os
import json
import sys
import unittest
import tempfile
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from _support.sexpr import Atom,dumps,loads
from _support.ast import (mk,tag,typ,integer,literal,boolean,ident,binop,call,block,do,
                          fn,param,Schema,walk)
from _support.passes import PROCESSORS
AT=mk('Loc','test.on1x',Atom('0'),Atom('1'),Atom('1'))

def unit(stmt=None,attribute=None,ret='Void'):
    statements=[stmt] if stmt else []
    fun=fn('main',[],typ(ret),block(statements,AT),AT)
    if attribute:fun[5].append(attribute)
    return mk('Unit',Atom('1'),[fun])

def ext(label,op,*args):return mk('ExtensionExpr',label,op,list(args),AT)
def attr(label,op,*args):return mk('Attribute',label,op,list(args),AT)
def count(tree,t):return sum(tag(n)==t for n in walk(tree))

def run(name,tree,label=None,env=None):
    e=os.environ.copy();e.update({'ON1X_PPX_LABEL':label or name,'ON1X_PPX_PROTOCOL':'1'})
    if env:e.update(env)
    return subprocess.run([sys.executable,str(ROOT/(name+'.py'))],input=dumps(tree),stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=e,timeout=10)

class PPXTests(unittest.TestCase):
    def setUp(self):self.schema=Schema()
    def transformed(self,name,tree,label=None,env=None):
        label=label or name
        result=PROCESSORS[name](loads(dumps(tree)),label) if env is None else None
        if env is not None:
            completed=run(name,tree,label,env)
            self.assertEqual(completed.returncode,0,completed.stderr.decode())
            result=loads(completed.stdout)
        self.schema.validate(result)
        return result
    def test_schema_and_codec(self):
        u=unit(do(call('println',[literal('hello\n',AT)],AT),AT))
        self.schema.validate(loads(dumps(u)))
        self.assertEqual(loads(dumps(u)),u)
        with self.assertRaises(ValueError):loads(b'(Unit 1 (')
        with self.assertRaises(ValueError):self.schema.validate(mk('Unit',Atom('2'),[]))
    def test_original_six(self):
        for name in ('identity','checks','control','unroll','derive','fold'):
            with self.subTest(name=name):
                u=unit(do(ext(name,'assert_eq',integer(2,AT),integer(2,AT)),AT)) if name=='checks' else unit()
                if name=='control':u=unit(do(ext(name,'when',boolean(True,AT),block([],AT)),AT))
                if name=='unroll':u=unit(do(ext(name,'repeat',integer(3,AT),block([],AT)),AT))
                if name=='derive':u=unit(attribute=attr(name,'derive',literal('arity',AT)))
                if name=='fold':u=unit(do(binop('+',integer(40,AT),integer(2,AT),AT),AT))
                got=self.transformed(name,u)
                self.assertEqual(run(name,u).returncode,0)
                if name=='checks':self.assertEqual(len(got[2]),2)
                if name=='unroll':self.assertEqual(count(got,'Scope'),4)
                if name=='derive':self.assertEqual(got[2][-1][1],'main_arity')
                if name=='fold':self.assertEqual(got[2][0][4][1][0][1][1],'42')
    def test_pipe(self):
        u=unit(do(ext('pipe','apply',integer(1,AT),ident('f',AT),ident('g',AT)),AT))
        t=self.transformed('pipe',u);self.assertEqual(t[2][0][4][1][0][1][1][1],'g')
    def test_compose(self):
        u=unit(do(ext('compose','apply',integer(1,AT),ident('f',AT),ident('g',AT)),AT))
        t=self.transformed('compose',u);self.assertEqual(t[2][0][4][1][0][1][1][1],'f')
    def test_cond(self):
        t=self.transformed('cond',unit(do(ext('cond','branches',boolean(True,AT),block([],AT),block([],AT)),AT)))
        self.assertEqual(count(t,'If'),1)
    def test_tap(self):
        t=self.transformed('tap',unit(do(ext('tap','call',integer(5,AT),ident('println',AT)),AT)))
        self.assertEqual(count(t,'Lambda'),1)
    def test_assertions(self):
        t=self.transformed('assertions',unit(do(ext('assertions','assert_true',boolean(True,AT)),AT)))
        self.assertEqual(len(t[2]),2)
    def test_trace(self):
        t=self.transformed('trace',unit(attribute=attr('trace','entry',literal('entry message',AT))))
        self.assertEqual(tag(t[2][0][4][1][0]),'Do')
        self.assertEqual(t[2][0][5],[])
    def test_logging(self):
        t=self.transformed('logging',unit(do(ext('logging','warn',literal('oops',AT)),AT)))
        self.assertEqual(count(t,'ExtensionExpr'),0)
    def test_format(self):
        t=self.transformed('format',unit(do(ext('format','string',literal('Hello {0}, {{ok}}',AT),literal('World',AT)),AT)))
        self.assertEqual(count(t,'ExtensionExpr'),0)
        bad=unit(do(ext('format','string',literal('oops {1}',AT),literal('World',AT)),AT))
        self.assertNotEqual(run('format',bad).returncode,0)
    def test_embed(self):
        with tempfile.TemporaryDirectory() as temp:
            Path(temp,'file.txt').write_bytes(b'example: 42\n')
            u=unit(do(ext('embed','text',literal('file.txt',AT)),AT))
            t=self.transformed('embed',u,env={'ON1X_PPX_EMBED_ROOT':temp})
            self.assertIn('example: 42',t[2][0][4][1][0][1][1])
            outside=unit(do(ext('embed','text',literal('../elsewhere',AT)),AT))
            self.assertNotEqual(run('embed',outside,env={'ON1X_PPX_EMBED_ROOT':temp}).returncode,0)
    def test_regex(self):
        for mode in ('full','prefix','suffix'):
            t=self.transformed('regex',unit(do(ext('regex',mode,literal('abc',AT),literal('a',AT)),AT)))
            self.assertFalse(any(tag(n)=='ExtensionExpr' for n in walk(t)))
    def test_contract(self):
        t=self.transformed('contract',unit(attribute=attr('contract','requires',boolean(True,AT))))
        self.assertEqual(len(t[2]),2)
    def test_match_int(self):
        t=self.transformed('match_int',unit(do(ext('match_int','cases',integer(1,AT),integer(1,AT),block([],AT),block([],AT)),AT)))
        self.assertEqual(count(t,'Let'),1)
    def test_state_machine(self):
        t=self.transformed('state_machine',unit(do(ext('state_machine','transition',integer(0,AT),integer(1,AT),integer(0,AT),integer(1,AT),integer(2,AT)),AT)))
        self.assertEqual(count(t,'Lambda'),1)
        self.assertEqual(count(t,'If'),1)
    def test_cfg(self):
        u=unit(attribute=attr('cfg','enabled',literal('featureA',AT)))
        t=self.transformed('cfg',u,env={'ON1X_PPX_FEATURES':'featureA'})
        self.assertEqual(len(t[2]),1)
        t=self.transformed('cfg',u,env={'ON1X_PPX_FEATURES':'featureB'})
        self.assertEqual(len(t[2]),0)
    def test_test_runner(self):
        t=self.transformed('test',unit(attribute=attr('test','case')))
        self.assertEqual(len(t[2]),2)
        self.assertEqual(t[2][1][1],'on1x_run_tests')
    def test_return_guard(self):
        u=unit(mk('Return',integer(3,AT),AT),attr('return_guard','nonnegative'),ret='I64')
        t=self.transformed('return_guard',u)
        self.assertEqual(len(t[2]),2)
        self.assertEqual(tag(t[2][0][4][1][0][1]),'Call')
    def test_timing(self):
        t=self.transformed('timing',unit(mk('Return',integer(3,AT),AT),attr('timing','elapsed'),ret='I64'))
        self.assertEqual(len(t[2]),3)
        self.assertEqual(sum(tag(n)=='Extern' for n in walk(t)),2)
        self.assertEqual(tag(t[2][0][4][1][0]),'Let')
    def test_ffi_bind(self):
        import json
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'approved.json'
            path.write_text(json.dumps({'schema':'on1x-raw-ffi/v1','target':'x86_64-unknown-linux-gnu','functions':{'c_safe':{'parameters':['i64'],'return':'i64','reviewed':True}}}))
            f=mk('Extern','c_safe',[param('a',typ('I64'),AT)],typ('I64'),[attr('ffi_bind','raw_scalar')],AT)
            u=mk('Unit',Atom('1'),[f]);env={'ON1X_PPX_FFI_MANIFEST':str(path),'ON1X_PPX_FFI_TARGET':'x86_64-unknown-linux-gnu'}
            t=self.transformed('ffi_bind',u,env=env)
            self.assertEqual(t[2][0][4],[])
            env['ON1X_PPX_FFI_TARGET']='wrong-target'
            self.assertNotEqual(run('ffi_bind',u,env=env).returncode,0)
    def test_ffi_manifest_ambiguity(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'ffi.json'
            f=mk('Extern','f',[],typ('I64'),[attr('ffi_bind','raw_scalar')],AT)
            u=mk('Unit',Atom('1'),[f])
            env={'ON1X_PPX_FFI_MANIFEST':str(path),'ON1X_PPX_FFI_TARGET':'test'}
            for wire in [
                '{"schema":"on1x-raw-ffi/v1","target":"wrong","target":"test","functions":{}}',
                '{"schema":"on1x-raw-ffi/v1","target":"test","functions":{"f":{"parameters":[],"return":"i64","reviewed":false,"reviewed":true}}}',
                '{"schema":"on1x-raw-ffi/v1","target":"test","functions":{},"ignored":NaN}',
            ]:
                path.write_text(wire)
                p=run('ffi_bind',u,env=env)
                self.assertNotEqual(p.returncode,0)
                self.assertEqual(p.stdout,b'')
    def test_invalid_transform_rejected(self):
        u=unit(do(ext('pipe','apply',integer(2,AT),integer(3,AT)),AT))
        c=run('pipe',u)
        self.assertNotEqual(c.returncode,0);self.assertEqual(c.stdout,b'')
        self.assertIn(b'identifiers',c.stderr)
    def test_missing_protocol_rejected(self):
        env=os.environ.copy();env.pop('ON1X_PPX_PROTOCOL',None);env['ON1X_PPX_LABEL']='checks'
        c=subprocess.run([sys.executable,str(ROOT/'checks.py')],input=dumps(unit()),stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env)
        self.assertNotEqual(c.returncode,0);self.assertEqual(c.stdout,b'')
    def test_unknown_schema_constructor_rejected(self):
        u=unit(do(ext('identity','any'),AT))
        u[2][0][4][1][0]=mk('Evil',literal('x',AT))
        c=run('identity',u)
        self.assertNotEqual(c.returncode,0)
    def test_legacy_protocol_schema_path(self):
        path=ROOT/'schemas/PPX.schema.json'
        c=run('identity',unit(),env={'ON1X_PPX_SCHEMA':str(path)})
        self.assertEqual(c.returncode,0,c.stderr.decode())
    def test_schema_matches_compiler(self):
        self.assertEqual(json.loads((ROOT/'schemas/PPX.schema.json').read_text()),
                         json.loads((ROOT.parent/'grammar/PPX.schema.json').read_text()))
    def test_nested_unroll_budget(self):
        inner=do(ext('unroll','repeat',integer(256,AT),block([do(call('println',[integer(1,AT)],AT),AT)],AT)),AT)
        u=unit(do(ext('unroll','repeat',integer(256,AT),block([inner],AT)),AT))
        result=run('unroll',u)
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(result.stdout,b'')
        self.assertIn(b'node limit',result.stderr)
    def test_timing_void_return(self):
        u=unit(mk('ReturnVoid',AT),attr('timing','elapsed'))
        t=self.transformed('timing',u)
        self.assertEqual(tag(t[2][0][4][1][-1]),'Scope')

if __name__=='__main__':unittest.main()
