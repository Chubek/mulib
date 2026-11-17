"""Executable On1x PPX transformations; all outputs are core v1 AST nodes.

The six legacy processors preserve repository behavior. The remaining processors
implement narrowly defined, checked extensions, not general untyped macros.
"""
import os
from pathlib import Path
from copy import deepcopy
from .sexpr import Atom,MAX_NODES
from .ast import (mk,tag,typ,integer,literal,boolean,ident,binop,unary,call,block,do,
                  fn,param,lam,walk,rewrite,Fresh,string,numeric,extension,attribute)


def identity(tree,label):return tree


def checks(tree,label):
    fresh=Fresh(tree); wrapper,arg=fresh.new(),fresh.new();at=None
    def change(n):
        nonlocal at
        if not extension(n,label):return n
        _,_,op,args,where=n
        if op not in ('assert_eq','assert_ne') or len(args)!=2:raise ValueError('checks expects assert_eq/assert_ne with two expressions')
        at=where
        return call(wrapper,[binop('==' if op=='assert_eq' else '!=',args[0],args[1],where)],where)
    tree=rewrite(tree,change)
    if at is not None:
        body=block([do(call('assert',[ident(arg,at)],at),at)],at)
        tree[2].append(fn(wrapper,[param(arg,typ('Bool'),at)],typ('Void'),body,at))
    return tree


def control(tree,label):
    def change(n):
        if tag(n)!='Do' or not extension(n[1],label):return n
        _,_,op,args,at=n[1]
        if op not in ('when','unless','while','until') or len(args)!=2 or tag(args[1])!='Block':raise ValueError('control expects when/unless/while/until(condition, Block)')
        cond=args[0] if op in ('when','while') else unary('!',args[0],at)
        if op in ('while','until'):return mk('While',cond,args[1],at)
        return mk('If',cond,args[1],block([],at),at)
    return rewrite(tree,change)


def unroll(tree,label):
    remaining=MAX_NODES
    def change(n):
        nonlocal remaining
        if tag(n)!='Do' or not extension(n[1],label):return n
        _,_,op,args,at=n[1]
        if op!='repeat' or len(args)!=2 or tag(args[1])!='Block':raise ValueError('unroll expects repeat(integer, Block)')
        count=numeric(args[0])
        if not 0<=count<=256:raise ValueError('unroll count must be 0..256')
        # Charge before deepcopy: nested/sibling repetitions must not allocate
        # an exponentially large intermediate tree before schema validation.
        cost=count*(sum(1 for _ in walk(args[1]))+16)+16
        if cost>remaining:raise ValueError('unroll expanded node limit exceeded')
        remaining-=cost
        return mk('Scope',block([mk('Scope',deepcopy(args[1]),at) for _ in range(count)],at),at)
    return rewrite(tree,change)


def _render_type(t):
    k=tag(t)
    fixed={'I64Type':'i64','F64Type':'f64','BoolType':'bool','StringType':'string','VoidType':'void','JsonType':'json','JsonViewType':'json_view','SExprType':'sexpr','BufferType':'buffer','HoleType':'_','Inferred':'_'}
    if k in fixed:return fixed[k]
    if k=='BoundType':return 'a'+str(t[1])
    if k=='CodeType':return 'code<'+_render_type(t[1])+'>'
    if k=='ForallType':return 'forall('+', '.join('a'+str(i) for i in range(int(t[1])))+') '+_render_type(t[2])
    if k=='TupleType':return '('+', '.join(map(_render_type,t[1]))+')'
    if k=='FunctionType':return 'fn('+', '.join(map(_render_type,t[1]))+') -> '+_render_type(t[2])
    if k=='ValueDataType':
        kind=string(t[1])
        head={'record':'record{','sum':'sum{','array':'array<','slice':'slice<'}[kind]
        labels=[string(l) for l in t[2]];fields=[_render_type(f) for f in t[3]]
        if kind in ('record','sum'):return head+', '.join(l+':'+f for l,f in zip(labels,fields))+'}'
        return head+_render_type(t[3][0])+', '+str(int(t[4]))+'>'
    raise ValueError('unknown type: '+str(k))


def derive(tree,label):
    extra=[]; used={entry[1] for entry in tree[2]}
    for entry in tree[2]:
        attrs_idx=4 if tag(entry)=='Extern' else 5
        rest=[]
        for a in entry[attrs_idx]:
            if not attribute(a,label) or a[2]!='derive':rest.append(a);continue
            for request in a[3]:
                kind=string(request)
                if kind not in ('arity','signature'):raise ValueError('derive supports only arity/signature')
                new_name=entry[1]+'_'+kind
                if new_name in used:raise ValueError('derived function name collision: '+new_name)
                used.add(new_name);at=a[4]
                if kind=='arity':result_type,value=typ('I64'),integer(len(entry[2]),at)
                else:
                    result_type=typ('String');value=literal('fn('+', '.join(_render_type(p[2]) for p in entry[2])+') -> '+_render_type(entry[3]),at)
                extra.append(fn(new_name,[],result_type,block([mk('Return',value,at)],at),at))
        entry[attrs_idx]=rest
    tree[2].extend(extra)
    return tree


def fold(tree,label):
    lo=-2**63;hi=2**63-1
    def change(n):
        if tag(n)!='Binary' or n[1] not in ('+','-','*','/','%'):return n
        try:a=numeric(n[2]);b=numeric(n[3])
        except ValueError:return n
        op=n[1]
        if op in ('/','%') and (b==0 or a==lo and b==-1):return n
        if op=='+':v=a+b
        elif op=='-':v=a-b
        elif op=='*':v=a*b
        else:
            q=(abs(a)//abs(b)) * (-1 if (a<0)!=(b<0) else 1)
            v=q if op=='/' else a-q*b
        return integer(v,n[4]) if lo<=v<=hi else n
    return rewrite(tree,change)


def pipe(tree,label):
    def change(n):
        if not extension(n,label):return n
        _,_,op,args,at=n
        if op!='apply' or len(args)<2:raise ValueError('pipe expects apply(value, function, ...)')
        result=args[0]
        for step in args[1:]:
            if tag(step)!='Identifier':raise ValueError('pipe functions must be identifiers (no repeated/effectful function expressions)')
            result=call(step,[result],at)
        return result
    return rewrite(tree,change)


def compose(tree,label):
    def change(n):
        if not extension(n,label):return n
        _,_,op,args,at=n
        if op!='apply' or len(args)<2:raise ValueError('compose expects apply(value, outer_function, inner_function, ...)')
        result=args[0]
        for step in reversed(args[1:]):
            if tag(step)!='Identifier':raise ValueError('compose functions must be identifiers')
            result=call(step,[result],at)
        return result
    return rewrite(tree,change)


def cond(tree,label):
    def change(n):
        if tag(n)!='Do' or not extension(n[1],label):return n
        _,_,op,args,at=n[1]
        if op=='choose' and len(args)==3:
            if any(tag(arm)!='Block' for arm in args[1:]):raise ValueError('cond requires Block arms')
            return mk('If',*args,at)
        if op!='branches' or len(args)<3 or len(args)%2!=1:raise ValueError('cond expects branches(cond,Block,...,defaultBlock)')
        result=args[-1]
        if tag(result)!='Block':raise ValueError('cond default must be Block')
        for i in reversed(range(0,len(args)-1,2)):
            if tag(args[i+1])!='Block':raise ValueError('cond requires Block arms')
            result=block([mk('If',args[i],args[i+1],result,at)],at)
        return mk('Scope',result,at)
    return rewrite(tree,change)


def tap(tree,label):
    fresh=Fresh(tree)
    def change(n):
        if not extension(n,label):return n
        _,_,op,args,at=n
        if op=='identity' and len(args)==1:return args[0]
        if op!='call' or len(args)!=2 or tag(args[1])!='Identifier':raise ValueError('tap expects call(value, callback_identifier)')
        name,callback=fresh.new(),fresh.new();variable=ident(name,at)
        worker=lam([param(name,typ('Hole'),at),param(callback,typ('Hole'),at)],typ('Hole'),block([
            do(call(callback,[variable],at),at),mk('Return',variable,at)],at),at)
        return call(worker,args,at)
    return rewrite(tree,change)


def assertions(tree,label):
    fresh=Fresh(tree); wrapper=fresh.new();value=fresh.new(); used=False
    def change(n):
        nonlocal used
        if not extension(n,label):return n
        _,_,op,args,at=n
        if op=='assert_true' and len(args)==1:check=args[0]
        elif op=='assert_false' and len(args)==1:check=unary('!',args[0],at)
        elif op in ('assert_eq','assert_ne') and len(args)==2:check=binop('==' if op=='assert_eq' else '!=',*args,at)
        elif op=='between' and len(args)==3:
            value,low,high=(fresh.new() for _ in range(3))
            condition=binop('&&',binop('>=',ident(value,at),ident(low,at),at),binop('<=',ident(value,at),ident(high,at),at),at)
            check=call(lam([param(n,typ('I64'),at) for n in (value,low,high)],typ('Bool'),block([mk('Return',condition,at)],at),at),args,at)
        else:raise ValueError('assertions accepts assert_true/false, assert_eq/ne, or between(value, low, high)')
        used=True
        return call(wrapper,[check],at)
    result=rewrite(tree,change)
    if used:
        at=result[2][0][-1] if result[2] else mk('Loc','<generated>',Atom('0'),Atom('1'),Atom('1'))
        result[2].append(fn(wrapper,[param(value,typ('Bool'),at)],typ('Void'),block([do(call('assert',[ident(value,at)],at),at)],at),at))
    return result


def trace(tree,label):
    for f in tree[2]:
        if tag(f)!='Function':continue
        keep=[]; logs=[]
        for a in f[5]:
            if not attribute(a,label):keep.append(a);continue
            if a[2]!='entry' or len(a[3])>1:raise ValueError('trace attribute expects entry([message])')
            message=string(a[3][0]) if a[3] else f[1]
            logs.append(do(call('io_stderr',[literal('TRACE '+message+'\n',a[4])],a[4]),a[4]))
        f[5]=keep
        if logs:f[4][1][:0]=logs
    return tree


def logging(tree,label):
    def change(n):
        if tag(n)!='Do' or not extension(n[1],label):return n
        _,_,op,args,at=n[1]
        if op=='print' and len(args)==1:return do(call('println',args,at),at)
        if op not in ('debug','info','warn','error') or len(args)!=1:raise ValueError('logging expects debug/info/warn/error(string_expression)')
        text=call('text_concat',[literal(op.upper()+': ',at),args[0]],at)
        text=call('text_concat',[text,literal('\n',at)],at)
        return do(call('io_stderr',[text],at),at)
    return rewrite(tree,change)


def _format_parts(template):
    out=[]; current='';pos=0;next_arg=0
    while pos<len(template):
        char=template[pos]
        if char=='{':
            if pos+1<len(template) and template[pos+1]=='{':current+='{';pos+=2;continue
            end=template.find('}',pos+1)
            if end<0:raise ValueError('format: unclosed brace')
            slot=template[pos+1:end]
            if not slot.isascii() or not slot.isdecimal() or len(slot)>4:raise ValueError('format: placeholders must be {0}, {1}, ...')
            idx=int(slot)
            if idx!=next_arg:raise ValueError('format: placeholders must be in argument order and occur once each')
            out.append(('text',current));current='';out.append(('arg',idx));next_arg+=1;pos=end+1;continue
        if char=='}':
            if pos+1<len(template) and template[pos+1]=='}':current+='}';pos+=2;continue
            raise ValueError('format: unmatched closing brace')
        current+=char;pos+=1
    out.append(('text',current))
    return out,next_arg


def format_(tree,label):
    def change(n):
        if not extension(n,label):return n
        _,_,op,args,at=n
        if op=='concat' and len(args)==2:return call('text_concat',args,at)
        if op!='string' or len(args)<1:raise ValueError('format expects string(template, string_args...)')
        parts,used=_format_parts(string(args[0]))
        if used!=len(args)-1:raise ValueError('format: expected '+str(used)+' value arguments')
        result=None
        for kind,value in parts:
            if kind=='text' and not value and result is not None:continue
            part=literal(value,at) if kind=='text' else args[value+1]
            result=part if result is None else call('text_concat',[result,part],at)
        return result if result is not None else literal('',at)
    return rewrite(tree,change)


def embed(tree,label):
    base=os.environ.get('ON1X_PPX_EMBED_ROOT')
    def change(n):
        if not extension(n,label):return n
        _,_,op,args,at=n
        if op!='text' or len(args)!=1:raise ValueError('embed expects text(relative_file_path)')
        if not base:raise ValueError('embed needs ON1X_PPX_EMBED_ROOT (explicit hermetic source directory)')
        requested=string(args[0]);relative=Path(requested)
        if relative.is_absolute() or '..' in relative.parts:raise ValueError('embed: only confined relative paths are allowed')
        root=Path(base).resolve(strict=True);resolved=(root/relative).resolve(strict=True)
        try: resolved.relative_to(root)
        except ValueError:raise ValueError('embed: path escapes declared root')
        if not resolved.is_file() or resolved.stat().st_size>1024*1024:raise ValueError('embed: file absent/nonregular/over 1 MiB')
        data=resolved.read_bytes()
        if b'\x00' in data or len(data)>1024*1024:raise ValueError('embed: input is not NUL-free bounded text')
        return literal(data.decode('latin-1'),at)
    return rewrite(tree,change)


def _regex_lambda(expr,pattern,op,at,fresh):
    name=fresh.new();x=ident(name,at);n=len(pattern);plen=integer(n,at)
    if op=='full':
        return binop('==',expr,literal(pattern,at),at)
    if op=='prefix':
        part=call('text_slice',[x,integer(0,at),plen],at)
    elif op=='suffix':
        length=call('text_len',[x],at)
        part=call('text_slice',[x,binop('-',length,plen,at),length],at)
    else:raise ValueError('unsupported regex subset')
    body=block([
        mk('If',binop('<',call('text_len',[x],at),plen,at),block([mk('Return',boolean(False,at),at)],at),block([],at),at),
        mk('Return',binop('==',part,literal(pattern,at),at),at)
    ],at)
    return call(lam([param(name,typ('String'),at)],typ('Bool'),body,at),[expr],at)


def regex(tree,label):
    fresh=Fresh(tree)
    def change(n):
        if not extension(n,label):return n
        _,_,op,args,at=n
        if op not in ('full','prefix','suffix') or len(args)!=2:raise ValueError('regex supports full/prefix/suffix literal patterns only')
        pattern=string(args[1]);
        if len(pattern)>4096:raise ValueError('regex literal exceeds 4096 bytes')
        return _regex_lambda(args[0],pattern,op,at,fresh)
    return rewrite(tree,change)


def contract(tree,label):
    fresh=Fresh(tree);wrapper=fresh.new();var=fresh.new();found=False
    for f in tree[2]:
        if tag(f)!='Function':continue
        keep=[];pre=[]
        for a in f[5]:
            if not attribute(a,label):keep.append(a);continue
            if a[2]!='requires' or len(a[3])!=1:raise ValueError('contract supports requires(condition) only')
            found=True;pre.append(do(call(wrapper,[a[3][0]],a[4]),a[4]))
        f[5]=keep
        if pre:f[4][1][:0]=pre
    if found:
        at=tree[2][0][-1]
        tree[2].append(fn(wrapper,[param(var,typ('Bool'),at)],typ('Void'),block([do(call('assert',[ident(var,at)],at),at)],at),at))
    return tree


def match_int(tree,label):
    fresh=Fresh(tree)
    def change(n):
        if tag(n)!='Do' or not extension(n[1],label):return n
        _,_,op,args,at=n[1]
        if op!='cases' or len(args)<4 or len(args)%2!=0 or tag(args[-1])!='Block':
            raise ValueError('match_int expects cases(value, literal, Block, ..., defaultBlock)')
        var=fresh.new();source=args[0];fallback=args[-1];seen=set()
        for i in reversed(range(1,len(args)-1,2)):
            key=numeric(args[i]); arm=args[i+1]
            if tag(arm)!='Block':raise ValueError('match_int: each case body must be Block')
            if key in seen:raise ValueError('match_int: duplicate case literal')
            seen.add(key)
            eq=binop('==',ident(var,at),integer(key,at),at)
            fallback=block([mk('If',eq,arm,fallback,at)],at)
        return mk('Scope',block([mk('Let',var,typ('I64'),source,at),mk('Scope',fallback,at)],at),at)
    return rewrite(tree,change)


def state_machine(tree,label):
    fresh=Fresh(tree)
    def change(n):
        if tag(n)=='Do' and extension(n[1],label) and n[1][2]=='set':
            _,_,_,args,at=n[1]
            if len(args)!=2 or tag(args[0])!='Identifier':raise ValueError('state_machine set expects identifier,expression')
            return mk('Assign',args[0][1],args[1],at)
        if not extension(n,label):return n
        _,_,op,args,at=n
        if op=='set':return n # Statement parent validates and consumes it.
        if op!='transition' or len(args)<5 or (len(args)-2)%3!=0:raise ValueError('state_machine expects transition(state, event, from, on, to, ...)')
        st,ev=fresh.new(),fresh.new();arms=[];seen=set()
        if (len(args)-2)//3>64:raise ValueError('state_machine supports at most 64 transitions')
        for i in range(2,len(args),3):
            source,event,target=map(numeric,args[i:i+3])
            if (source,event) in seen:raise ValueError('duplicate state/event transition')
            seen.add((source,event))
            c=binop('&&',binop('==',ident(st,at),integer(source,at),at),binop('==',ident(ev,at),integer(event,at),at),at)
            arms.append((c,integer(target,at)))
        statements=[]
        for c,value in arms:
            statements.append(mk('If',c,block([mk('Return',value,at)],at),block([],at),at))
        statements.append(mk('Return',ident(st,at),at))
        func=lam([param(st,typ('I64'),at),param(ev,typ('I64'),at)],typ('I64'),block(statements,at),at)
        return call(func,args[:2],at)
    return rewrite(tree,change)


def cfg(tree,label):
    features={x.strip() for x in os.environ.get('ON1X_PPX_FEATURES','').split(',') if x.strip()}
    result=[]
    for f in tree[2]:
        keep=[];enabled=True
        i=4 if tag(f)=='Extern' else 5
        for a in f[i]:
            if not attribute(a,label):keep.append(a);continue
            if a[2] not in ('enabled','disabled') or len(a[3])!=1:raise ValueError('cfg expects enabled/disabled("feature")')
            feature=string(a[3][0])
            if not feature or any(c not in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-' for c in feature):raise ValueError('invalid feature name')
            enabled=enabled and ((feature in features) if a[2]=='enabled' else (feature not in features))
        f[i]=keep
        if enabled:result.append(f)
    tree[2]=result
    return tree


def test(tree,label):
    tests=[];names={f[1] for f in tree[2]}
    for f in tree[2]:
        if tag(f)!='Function':continue
        keep=[]
        for a in f[5]:
            if not attribute(a,label):keep.append(a);continue
            if a[2]!='case' or a[3]:raise ValueError('test expects @test("case") with no arguments')
            if f[2] or tag(f[3])!='VoidType':raise ValueError('test case must have fn () -> void signature')
            tests.append((f[1],a[4]))
        f[5]=keep
    if tests:
        if 'on1x_run_tests' in names:raise ValueError('test runner name on1x_run_tests is taken')
        at=tests[0][1]
        tree[2].append(fn('on1x_run_tests',[],typ('Void'),block([do(call(n,[],a),a) for n,a in tests],at),at))
    return tree


def _rewrite_returns(body, wrap):
    # Return inside a nested lambda or staged quote belongs to a different world.
    if tag(body) in ('Lambda','Quote'):
        return body
    if tag(body)=='Return':
        return mk('Return',wrap(body[1],body[2]),body[2])
    if tag(body)=='Block':
        return mk('Block',[_rewrite_returns(s,wrap) for s in body[1]],body[2])
    if tag(body)=='If':
        return mk('If',body[1],_rewrite_returns(body[2],wrap),_rewrite_returns(body[3],wrap),body[4])
    if tag(body)=='While':
        return mk('While',body[1],_rewrite_returns(body[2],wrap),body[3])
    if tag(body)=='Scope':
        return mk('Scope',_rewrite_returns(body[1],wrap),body[2])
    return body


def return_guard(tree,label):
    fresh=Fresh(tree);wrapper=fresh.new();arg=fresh.new();needed=False
    for f in tree[2]:
        if tag(f)!='Function':continue
        kept=[];active=False
        for a in f[5]:
            if not attribute(a,label):kept.append(a);continue
            if a[2]!='nonnegative' or a[3] or tag(f[3])!='I64Type':raise ValueError('return_guard requires nonnegative on explicit i64 result functions')
            active=True;needed=True
        f[5]=kept
        if active:
            f[4]=_rewrite_returns(f[4],lambda expr,at:call(wrapper,[expr],at))
    if needed:
        at=tree[2][0][-1];value=ident(arg,at)
        tree[2].append(fn(wrapper,[param(arg,typ('I64'),at)],typ('I64'),block([
            do(call('assert',[binop('>=',value,integer(0,at),at)],at),at),
            mk('Return',value,at)],at),at))
    return tree


def timing(tree,label):
    fresh=Fresh(tree);now='on1x_ppx_clock_ns';report='on1x_ppx_timing_log';changed=False
    existing={entry[1] for entry in tree[2]}
    for f in tree[2]:
        if tag(f)!='Function':continue
        at=f[-1];keep=[];active=False
        for a in f[5]:
            if not attribute(a,label):keep.append(a);continue
            if a[2]!='elapsed' or a[3]:raise ValueError('timing supports @timing("elapsed") only')
            if tag(f[3]) not in ('I64Type','BoolType','StringType','VoidType'):
                raise ValueError('timing supports explicit i64/bool/string/void return types only')
            active=True;changed=True
        f[5]=keep
        if not active:continue
        start=fresh.new();retvar=fresh.new()
        result_type=f[3]
        def finish(start=start,at=at,name=f[1]):
            delta=binop('-',call(now,[],at),ident(start,at),at)
            return do(call(report,[literal(name,at),delta],at),at)
        # The value is first fully evaluated, then clocked/logged, then returned.
        def instrument(body):
            kind=tag(body)
            if kind in ('Lambda','Quote'):return body
            if kind=='Return':
                # Scope is a statement, so multiple statements preserve the original return expression's single evaluation.
                return mk('Scope',block([mk('Let',retvar,result_type,body[1],body[2]),finish(),mk('Return',ident(retvar,body[2]),body[2])],body[2]),body[2])
            if kind=='ReturnVoid':
                return mk('Scope',block([finish(),body],body[1]),body[1])
            if kind=='Block':return mk('Block',[instrument(s) for s in body[1]],body[2])
            if kind=='If':return mk('If',body[1],instrument(body[2]),instrument(body[3]),body[4])
            if kind=='While':return mk('While',body[1],instrument(body[2]),body[3])
            if kind=='Scope':return mk('Scope',instrument(body[1]),body[2])
            return body
        inner=instrument(f[4]);inner[1].insert(0,mk('Let',start,typ('I64'),call(now,[],at),at))
        def falls_through(node):
            kind=tag(node)
            if kind in ('Return','ReturnVoid'):return False
            if kind=='Block':return not node[1] or falls_through(node[1][-1])
            if kind=='Scope':return falls_through(node[1])
            if kind=='If':return falls_through(node[2]) or falls_through(node[3])
            return True
        if tag(result_type)=='VoidType' and falls_through(inner):inner[1].append(finish())
        f[4]=inner
    if changed:
        if now in existing or report in existing:raise ValueError('timing runtime hook name collides with an existing declaration')
        at=tree[2][0][-1]
        tree[2].append(mk('Extern',now,[],typ('I64'),[],at))
        tree[2].append(mk('Extern',report,[param('name',typ('String'),at),param('duration',typ('I64'),at)],typ('Void'),[],at))
    return tree


def ffi_bind(tree,label):
    manifest_path=os.environ.get('ON1X_PPX_FFI_MANIFEST','')
    target=os.environ.get('ON1X_PPX_FFI_TARGET','')
    required=[]
    for f in tree[2]:
        i=4 if tag(f)=='Extern' else 5
        if not any(attribute(a,label) for a in f[i]):continue
        if tag(f)!='Extern':raise ValueError('ffi_bind attributes apply only to extern fn declarations')
        if not manifest_path or not target:raise ValueError('ffi_bind requires ON1X_PPX_FFI_MANIFEST and ON1X_PPX_FFI_TARGET')
        kept=[]
        for a in f[i]:
            if not attribute(a,label):kept.append(a);continue
            if a[2]!='raw_scalar' or a[3]:raise ValueError('ffi_bind supports @ffi_bind("raw_scalar") only')
            arguments=[]
            for p in f[2]:
                if tag(p)!='Parameter' or tag(p[2]) not in ('I64Type','BoolType'):
                    raise ValueError('ffi_bind only admits scalar i64/bool ABI parameters')
                arguments.append('i64' if tag(p[2])=='I64Type' else 'bool')
            if tag(f[3]) not in ('I64Type','BoolType','VoidType'):
                raise ValueError('ffi_bind only admits scalar i64/bool/void return')
            required.append((f[1],arguments,{'I64Type':'i64','BoolType':'bool','VoidType':'void'}[tag(f[3])]))
        f[i]=kept
    if required:
        import json
        path=Path(manifest_path)
        if not path.is_file() or path.stat().st_size>1024*1024:raise ValueError('invalid or oversized ffi manifest')
        from .ast import Schema
        with path.open('rb') as stream:wire=stream.read(1024*1024+1)
        if len(wire)>1024*1024:raise ValueError('oversized ffi manifest')
        data=json.loads(wire,object_pairs_hook=Schema._unique,
                        parse_constant=lambda _:(_ for _ in ()).throw(ValueError('nonfinite ffi manifest value')))
        if not isinstance(data,dict) or set(data)!={'schema','target','functions'}:raise ValueError('ffi manifest requires exact schema/target/functions fields')
        if data.get('schema')!='on1x-raw-ffi/v1' or data.get('target')!=target:raise ValueError('ffi manifest schema or target mismatch')
        catalog=data.get('functions')
        if not isinstance(catalog,dict):raise ValueError('ffi manifest requires functions object')
        for name,args,result in required:
            v=catalog.get(name)
            if not isinstance(v,dict) or set(v)!={'parameters','return','reviewed'} or v.get('parameters')!=args or v.get('return')!=result or v.get('reviewed') is not True:
                raise ValueError('unreviewed/mismatched ffi declaration: '+name)
    return tree


PROCESSORS={
    'identity':identity,'checks':checks,'control':control,'unroll':unroll,'derive':derive,'fold':fold,
    'pipe':pipe,'compose':compose,'cond':cond,'tap':tap,'assertions':assertions,
    'trace':trace,'logging':logging,'format':format_,'embed':embed,'regex':regex,
    'contract':contract,'match_int':match_int,'state_machine':state_machine,
    'cfg':cfg,'test':test,'return_guard':return_guard,'timing':timing,'ffi_bind':ffi_bind,
}
