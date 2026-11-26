"""Core AST construction, traversal, schema checks and hygiene helpers."""
import json
import re
from pathlib import Path
from .sexpr import Atom,MAX_DEPTH,MAX_NODES
KEYWORDS={'fn','let','var','if','else','while','return','true','false','extern','import','forall','macro','rule','quote','splice','lift','let_syntax','letrec_syntax'}
NAME=re.compile(r'[A-Za-z_][A-Za-z0-9_]*\Z')

def name(s): return type(s)==str and bool(NAME.fullmatch(s)) and s not in KEYWORDS

def mk(tag,*fields):return [Atom(tag),*fields]
def tag(n): return str(n[0]) if isinstance(n,list) and n and isinstance(n[0],Atom) else None
def typ(which):return mk(which+'Type')
def loc():return mk('Loc','<generated>',Atom('0'),Atom('1'),Atom('1'))
def literal(s,at):return mk('StringLiteral',s,at)
def integer(n,at):return mk('DecimalLiteral',str(n),at)
def boolean(b,at):return mk('BooleanLiteral',Atom('true' if b else 'false'),at)
def ident(s,at):return mk('Identifier',s,at)
def binop(op,left,right,at):return mk('Binary',Atom(op),left,right,at)
def unary(op,value,at):return mk('Unary',Atom(op),value,at)
def operator_ref(lexeme,at):return mk('OperatorReference',lexeme,at)
def operator_unary(lexeme,position,value,at):return mk('OperatorUnary',lexeme,position,value,at)
def operator_binary(lexeme,left,right,at):return mk('OperatorBinary',lexeme,left,right,at)
def operator_ternary(lexeme,lexeme2,first,second,third,at):
    return mk('OperatorTernary',lexeme,lexeme2,first,second,third,at)
def operator_header(lexeme,lexeme2,arity,position,precedence,tightness,binds,at):
    return mk('OperatorHeader',lexeme,lexeme2,arity,position,Atom(str(precedence)),Atom(str(tightness)),binds,at)
def operator_declaration(header,generics,formals,result,body):
    return mk('OperatorDeclaration',header,generics,formals,result,body)
def fixity_directive(lexeme,tightness,binds,at):
    return mk('FixityDirective',lexeme,Atom(str(tightness)),binds,at)
def call(callee,args,at):return mk('Call', ident(callee,at) if isinstance(callee,str) else callee, args,at)
def block(body,at):return mk('Block',body,at)
def do(expr,at):return mk('Do',expr,at)
def param(name,type_node,at):return mk('Parameter',name,type_node,at)
def fn(name,params,result,body,at):return mk('Function',name,params,result,body,[],at)
def lam(params,result,body,at):return mk('Lambda',params,result,body,at)

def walk(tree):
    stack=[(tree,0)];count=0
    while stack:
        obj,depth=stack.pop();count+=1
        if count>MAX_NODES or depth>=MAX_DEPTH:raise ValueError('AST walk node/depth limit exceeded')
        yield obj
        if isinstance(obj,list):stack.extend((child,depth+1) for child in reversed(obj))

def rewrite(tree,proc):
    count=0
    def visit(obj,depth):
        nonlocal count
        count+=1
        if count>MAX_NODES or depth>=MAX_DEPTH:raise ValueError('AST rewrite node/depth limit exceeded')
        if isinstance(obj,list):obj=[visit(x,depth+1) for x in obj]
        return proc(obj)
    return visit(tree,0)

class Fresh:
    def __init__(self,tree):
        self.used={str(n) for n in walk(tree) if isinstance(n,str)};self.serial=0
    def new(self):
        while True:
            self.serial+=1
            candidate='on1x_ppx_'+str(self.serial)
            if candidate not in self.used:
                self.used.add(candidate);return candidate

class Schema:
    def __init__(self,path=None):
        p=Path(path) if path else Path(__file__).resolve().parents[1]/'schemas/PPX.schema.json'
        with p.open('rb') as stream:source=stream.read(2*1024*1024+1)
        if len(source)>2*1024*1024:raise ValueError('schema exceeds 2 MiB')
        data=json.loads(source,object_pairs_hook=self._unique)
        if data['version']!=1 or data['schema_format']!='on1x.sexpr-ast/v1':raise ValueError('unsupported PPX schema')
        self.parents=data['categories'];self.nodes=data['nodes']
    @staticmethod
    def _unique(pairs):
        d={}
        for key,value in pairs:
            if key in d:raise ValueError('duplicate schema key '+key)
            d[key]=value
        return d
    def subtype(self,child,parent):
        for _ in range(len(self.parents)+1):
            if child==parent:return True
            child=self.parents.get(child)
            if child is None or child=='':return False
        return False
    def validate(self,root):
        nodes=0
        def scalar(n,k):
            quoted=type(n)==str
            if not isinstance(n,str) or '\x00' in n:return False
            if k=='Bytes':return quoted
            if k=='File':return quoted and len(n)<=4096 and all(32<=ord(c)<127 for c in n)
            if k=='Name':return name(n)
            if k=='Version':return not quoted and n=='1'
            if k in ('Nat','Positive'):
                return not quoted and bool(re.fullmatch(r'[0-9]+',n)) and int(n)<=2**64-1 and (k!='Positive' or int(n)>0)
            if k=='Boolean':return not quoted and n in ('true','false')
            if k=='OperatorLexeme':return quoted and bool(re.fullmatch(r'[!%&*+?^|~<>$=.\-]{2,6}',n))
            if k=='UnaryPosition':return quoted and n in ('prefix','postfix')
            if k=='UnaryOperator':return not quoted and n in ('!','-')
            if k=='BinaryOperator':return not quoted and n in ('+','-','*','/','%','<','>','<=','>=','==','!=','&&','||')
            if k=='DecimalSpelling':return quoted and bool(re.fullmatch(r'-?[0-9]+',n))
            if k=='HexSpelling':return quoted and bool(re.fullmatch(r'0[xX][0-9a-fA-F]+',n))
            if k=='FloatSpelling':return quoted and len(n)<=1024 and bool(re.fullmatch(r'(?:[0-9]+(?:\.[0-9]+(?:[eE][+-]?[0-9]+)?|[eE][+-]?[0-9]+)(?:f32|f64)?|[0-9]+(?:f32|f64))',n))
            if k=='UnsignedSpelling':return quoted and len(n)<=1024 and bool(re.fullmatch(r'(?:0[xX][0-9a-fA-F]+|[0-9]+)(?:u8|u16|u32|u64)',n))
            return False
        def check(obj,expected,depth,path):
            nonlocal nodes
            nodes+=1
            if nodes>MAX_NODES or depth>=MAX_DEPTH:raise ValueError(path+': tree limits exceeded')
            if expected.endswith('*'):
                if not isinstance(obj,list):raise ValueError(path+': expected list')
                for i,x in enumerate(obj):check(x,expected[:-1],depth+1,path+'['+str(i)+']')
                return
            if expected not in self.parents:
                if not scalar(obj,expected):raise ValueError(path+': expected '+expected)
                return
            node_def=self.nodes.get(tag(obj))
            if node_def is None or not self.subtype(node_def['category'],expected):raise ValueError(path+': bad constructor for '+expected)
            fields=node_def['fields']
            if len(obj)!=len(fields)+1:raise ValueError(path+': wrong field count')
            for v,(field,kind) in zip(obj[1:],fields):check(v,kind,depth+1,path+'.'+field)
        check(root,'Unit',0,'$')


def extensions_left(tree,label):
    return [n for n in walk(tree) if tag(n) in ('ExtensionExpr','Attribute') and n[1]==label]

def expect_kind(node,kind,explain=''):
    if tag(node)!=kind:raise ValueError(explain or ('expected '+kind))
    return node

def string(node):
    expect_kind(node,'StringLiteral','expected a string literal')
    return node[1]

def numeric(node):
    t=tag(node)
    if t not in ('DecimalLiteral','HexLiteral'):raise ValueError('expected integer literal')
    n=int(node[1],16 if t=='HexLiteral' else 10)
    if not -(2**63)<=n<2**63:raise ValueError('integer literal outside i64')
    return n

def extension(node,label):return tag(node)=='ExtensionExpr' and node[1]==label

def attribute(node,label):return tag(node)=='Attribute' and node[1]==label

def require(op,args,allowed,minimum=None,maximum=None):
    if op not in allowed:raise ValueError('expected operation '+('/'.join(allowed)))
    if minimum is not None and len(args)<minimum:raise ValueError(op+': not enough arguments')
    if maximum is not None and len(args)>maximum:raise ValueError(op+': too many arguments')
