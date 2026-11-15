#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-mune.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void execute(mulib_mune_state_t *state, const char *source)
{
    int mode;
    for (mode = MULIB_MUNE_TREE; mode <= MULIB_MUNE_JIT; ++mode) {
    mulib_mune_result_t *r = NULL;
    mulib_mune_error_t e;
    mulib_mune_status_t status;
    if (mode == MULIB_MUNE_JIT && !mulib_jit_supported()) continue;
    assert(mulib_mune_set_mode(state, (mulib_mune_mode_t)mode) == MULIB_MUNE_OK);
    status = mulib_mune_run_source(state, source, strlen(source), &r, &e);
    if (status) fprintf(stderr, "%zu: %s\n%s\n", e.offset, e.message, source);
    assert(status == MULIB_MUNE_OK);
    mulib_mune_result_destroy(r);
    }
}
static int writer(void *user, const char *bytes, size_t n)
{ return fwrite(bytes,1,n,(FILE *)user)==n; }
static mulib_mune_status_t native_sum(mulib_mune_state_t *s, const mulib_mune_value_t *args,
                                    size_t count, mulib_mune_result_t *out, void *user)
{
    mulib_mune_value_t v = {MULIB_MUNE_INTEGER,{.integer=0}};
    size_t k;
    (void)s; (void)user;
    for(k=0;k<count;++k) {
        if(args[k].type!=MULIB_MUNE_INTEGER) return MULIB_MUNE_RUNTIME;
        v.as.integer += args[k].as.integer;
    }
    return mulib_mune_result_push(out,v)?MULIB_MUNE_OK:MULIB_MUNE_MEMORY;
}
static mulib_mune_status_t native_roundtrip(mulib_mune_state_t *s, const mulib_mune_value_t *args,
                                          size_t count, mulib_mune_result_t *out, void *user)
{
    mulib_mune_result_t *r=NULL;
    mulib_mune_status_t status;
    size_t k;
    (void)user;
    if(!count) return MULIB_MUNE_INVALID;
    mulib_mune_collect(s);
    status=mulib_mune_call(s,args[0],args+1,count-1,&r,NULL);
    mulib_mune_collect(s);
    for(k=0;status==MULIB_MUNE_OK && k<mulib_mune_result_count(r);++k)
        if(!mulib_mune_result_push(out,mulib_mune_result_get(r,k))) status=MULIB_MUNE_MEMORY;
    mulib_mune_result_destroy(r);
    return status;
}

int main(int argc, char **argv)
{
    mulib_mune_state_t *s = mulib_mune_state_create(NULL);
    assert(s);
    if (argc == 3 && !strcmp(argv[1], "--bundle")) {
        mulib_mune_error_t e;
        mulib_mune_chunk_t *chunk=mulib_mune_parse_file(argv[2],NULL,&e);
        mulib_mune_status_t status;
        mulib_mune_bundle_module_t item;
        if(!chunk) { fprintf(stderr,"%zu: %s\n",e.offset,e.message); mulib_mune_state_destroy(s); return 1; }
        item.name="test_bundle"; item.source=mulib_mune_chunk_ast(chunk)->text; item.length=mulib_mune_chunk_ast(chunk)->length;
        status=mulib_mune_bundle_write(&item,1,writer,stdout,&e);
        mulib_mune_chunk_destroy(chunk);mulib_mune_state_destroy(s);return status?1:0;
    }
    if (argc == 3 && !strcmp(argv[1], "--emit")) {
        mulib_mune_error_t e;
        mulib_mune_chunk_t *chunk=mulib_mune_parse_file(argv[2],NULL,&e);
        int status;
        if(!chunk) { fprintf(stderr,"%zu: %s\n",e.offset,e.message); return 1; }
        puts("#define MULIB_IMPL_MUNE\n#define MULIB_IMPL_DYNLOAD\n#define MULIB_IMPL_PARSEC\n#define MULIB_IMPL_PARSER\n#define MULIB_IMPL_PEG\n#define MULIB_IMPL_AST\n#define MULIB_IMPL_STRBUF\n#define MULIB_IMPL_GLOB\n#define MULIB_IMPL_MIR\n#define MULIB_IMPL_VMWEAVE\n#define MULIB_IMPL_EGRAPH\n#define MULIB_IMPL_REWRITE\n#define MULIB_IMPL_DYNCALL\n#define MULIB_IMPL_JIT");
        status=mulib_mune_emit_c(chunk,"mulib_generated_lua",writer,stdout,&e);
        puts("int main(void){mulib_mune_state_t *s=mulib_mune_state_create(NULL);mulib_mune_result_t *r=NULL;mulib_mune_error_t e;int status;if(!s)return 1;status=mulib_generated_lua(s,&r,&e);if(status)fprintf(stderr,\"%zu: %s\\n\",e.offset,e.message);mulib_mune_result_destroy(r);mulib_mune_state_destroy(s);return status?1:0;}");
        mulib_mune_chunk_destroy(chunk);mulib_mune_state_destroy(s);return status?1:0;
    }
    if (argc == 3 && (!strcmp(argv[1], "--run") || !strcmp(argv[1], "--vm") || !strcmp(argv[1], "--jit"))) {
        mulib_mune_error_t e;
        mulib_mune_result_t *r = NULL;
        int status;
        if (!strcmp(argv[1],"--jit") && !mulib_jit_supported()) { mulib_mune_state_destroy(s); return 77; }
        assert(mulib_mune_set_mode(s,!strcmp(argv[1],"--vm")?MULIB_MUNE_VM:!strcmp(argv[1],"--jit")?MULIB_MUNE_JIT:MULIB_MUNE_TREE)==MULIB_MUNE_OK);
        status = mulib_mune_run_file(s, argv[2], &r, &e);
        if (status) fprintf(stderr, "%zu: %s\n", e.offset, e.message);
        mulib_mune_result_destroy(r); mulib_mune_state_destroy(s); return status ? 1 : 0;
    }
    execute(s, "assert(1+2*3==7); assert(-2^2==-4 and 2^-2==.25); assert(7//-3==-3 and 7%-3==-2); assert(1<<-1==0 and -1>>63==1); assert(0xffffffffffffffff==-1)");
    execute(s, "local a,b,c; local function f() return 10,nil,30 end; a,b,c=f(); assert(a==10 and b==nil and c==30); a,b=(f()); assert(a==10 and b==nil); a,a=1,2; assert(a==1); assert(select('#',f())==3)");
    execute(s, "local function fact(n) if n<=1 then return 1 end; return n*fact(n-1) end; assert(fact(10)==3628800)");
    execute(s, "local function f() return 42 end; local function g() return {f()} end; assert(g()[1]==42); local function h() return function() f() end end; assert(type(h())=='function')");
    execute(s, "local function count(n,v) if n==0 then return v end; return count(n-1,v+1) end; assert(count(1000,0)==1000)");
    execute(s, "local f; do local x=40; f=function(n) x=x+n; return x end end; collectgarbage(); assert(f(1)==41 and f(1)==42)");
    execute(s, "local f={}; for i=1,3 do f[i]=function() return i end end; assert(f[1]()==1 and f[3]()==3); local n=0; for i=10,1,-2 do n=n+i; i=0 end; assert(n==30)");
    execute(s, "local n=0; repeat local x=n+1; n=x until x==4; while n<10 do n=n+1; if n==7 then break end end; assert(n==7)");
    execute(s, "local n=0; ::again:: n=n+1; do if n<5 then goto again end end; assert(n==5); goto done; local x=2; ::done::");
    execute(s, "local t={a=2,[false]=3,10,20}; local sum=0; for k,v in pairs(t) do sum=sum+v end; assert(sum==35); sum=0; for i,v in ipairs(t) do sum=sum+v end; assert(sum==30)");
    execute(s, "local t={}; function t:add(x) self.n=(self.n or 0)+x; return self.n end; assert(t:add(20)==20 and t:add(22)==42)");
    execute(s, "local t=setmetatable({}, {__index=function(t,k) return k..'!' end, __newindex=function(t,k,v) rawset(t,k,v*2) end}); assert(t.x=='x!'); t.x=21; assert(t.x==42)");
    execute(s, "local m={__add=function(a,b) return a.x+b.x end, __len=function(a) return a.x end, __call=function(a,x) return a.x*x end}; local a=setmetatable({x=6},m); local b=setmetatable({x=7},m); assert(a+b==13 and #a==6 and a(7)==42)");
    execute(s, "local log={}; do local a<close> =setmetatable({}, {__close=function(a,e) log[#log+1]='a'; assert(e==nil) end}); do local b<close> =setmetatable({}, {__close=function() log[#log+1]='b' end}) end end; assert(table.concat(log)=='ba')");
    execute(s, "local log; local ok,e=pcall(function() local x<close> =setmetatable({}, {__close=function(x,err) log=err end}); error('boom',0) end); assert(not ok and e=='boom' and log=='boom'); assert(xpcall(function() error(4) end,function(e) return e*2 end)==false)");
    execute(s, "local t=table.pack(1,nil,3); assert(t.n==3 and select('#',table.unpack(t,1,3))==3); t={3,1,2}; table.sort(t); assert(table.concat(t,',')=='1,2,3'); table.insert(t,2,4); assert(table.remove(t,2)==4); table.move(t,1,2,2); assert(table.concat(t)== '112')");
    execute(s, "assert(math.type(1)=='integer' and math.type(1.0)=='float'); assert(math.floor(-1.2)==-2); assert(math.tointeger(1.5)==nil); assert(math.maxinteger+1==math.mininteger); assert(math.maxinteger>9223372036854775806); assert(math.maxinteger<9223372036854775808.0)");
    execute(s, "assert(('AbC'):lower()=='abc'); assert(string.sub('abc',-2)=='bc'); assert(string.rep('ab',3,':')=='ab:ab:ab'); assert(string.byte('xyz',2)==121); assert(string.char(0,255)== '\\0\\255'); assert(string.format('%04d:%s',42,'ok')=='0042:ok')");
    execute(s, "local f=assert(load('return x+1',nil,'t',{x=41})); assert(f()==42); local _ENV={assert=assert,x=5}; assert(x==5)");
    execute(s, "assert(('abc123'):match('(%a+)(%d+)')=='abc'); local a,b,c=('one two'):find('(%w+)',3); assert(a==3 and b==3 and c=='e'); assert(('(a(b)c)'):match('%b()')=='(a(b)c)'); assert(('hello hello'):match('(%w+) %1')=='hello')");
    execute(s, "local a,b=string.gsub('a1 b22','(%a)(%d+)',function(x,y) collectgarbage(); return x..tonumber(y)*2 end); assert(a=='a2 b44' and b==2); local t={}; for w in string.gmatch('one two three','%w+') do t[#t+1]=w end; assert(table.concat(t,':')=='one:two:three')");
    execute(s, "assert(('ab'):gsub('','.')=='.a.b.'); assert(('abc'):gsub('(.)','%1%0')=='aabbcc'); assert((' x foo!'):match('%f[%a]foo%f[%A]')=='foo'); assert(('aXXbYYb'):match('a.-b')=='aXXb'); assert(('123'):match('()(%d+)')==1)");
    execute(s, "local t={}; t.self=t; for i=1,200 do t[i]={i} end; local before=collectgarbage('count'); t=nil; collectgarbage(); assert(collectgarbage('count')<before)");
    execute(s, "local weak=setmetatable({}, {__mode='kv'}); do local k,v={},{}; weak[k]=v end; collectgarbage(); assert(next(weak)==nil); local x=0; do local t=setmetatable({}, {__gc=function() x=x+1 end}) end; collectgarbage(); assert(x==1); collectgarbage(); assert(x==1)");
    execute(s, "collectgarbage('stop'); collectgarbage('incremental',200,100,8); local weak=setmetatable({}, {__mode='v'}); local saved={}; do local x={}; weak[1]=x end; local finished=false; for i=1,2000 do saved[i]={i}; if collectgarbage('step',0) then finished=true; break end end; assert(finished and weak[1]==nil and not collectgarbage('isrunning')); for i,v in ipairs(saved) do assert(v[1]==i) end; collectgarbage('restart'); collectgarbage('incremental',200,100,13)");
    execute(s, "local old={}; collectgarbage('generational',20,100); collectgarbage(); local weak=setmetatable({}, {__mode='k'}); for i=1,80 do local k={i}; old.value={k}; weak[k]=old.value; collectgarbage('step',32); assert(old.value[1][1]==i and weak[k]==old.value) end; old.value=nil; collectgarbage(); assert(next(weak)==nil); assert(collectgarbage('incremental',200,100,13)=='generational'); assert(collectgarbage('setpause',200)==200 and collectgarbage('setstepmul',100)==100)");
    execute(s, "local text=utf8.char(65,0x20ac,0x1f600); assert(utf8.len(text)==3 and #text==8); assert(utf8.codepoint(text,2)==0x20ac and utf8.offset(text,-1)==5); local n=0; for pos,c in utf8.codes(text) do n=n+1 end; assert(n==3); assert(utf8.len('\\xff')==nil)");
    execute(s, "local f=assert(io.tmpfile()); assert(io.type(f)=='file'); assert(f:write('10 20\\nhello\\n')); assert(f:seek('set',0)==0); local a,b=f:read('n','n'); assert(a==10 and b==20); assert(f:read('l')==''); assert(f:read('l')=='hello'); assert(f:read('l')==nil); assert(f:close()); assert(io.type(f)=='closed file')");
    execute(s, "local f; do local x<close> =assert(io.tmpfile()); f=x; assert(getmetatable(x).__name=='FILE*' and type(getmetatable(x).__gc)=='function'); assert(not pcall(x.read,x,nil)); x:close(); assert(tostring(x)=='file (closed)') end; assert(not pcall(f.lines,f) and not pcall(f.close,f,nil)); do local x<close> =io.stdout end; assert(io.type(io.stdout)=='file')");
    execute(s, "assert(os.date('!%Y-%m-%d',0)=='1970-01-01'); local d=os.date('!*t',0); assert(d.year==1970 and d.month==1 and d.day==1); assert(os.difftime(20,10)==10); assert(type(os.clock())=='number')");
    execute(s, "package.loaded.test_lazy=nil; local n=0; package.preload.test_lazy=function(name,data) assert(name=='test_lazy' and data==':preload:'); n=n+1; return {value=42} end; local a,d=require('test_lazy'); assert(a.value==42 and d==':preload:'); assert(require('test_lazy')==a and n==1)");
    execute(s, "local co=coroutine.create(function(x) local y=coroutine.yield(x+1); return y*2 end); assert(coroutine.status(co)=='suspended'); local ok,v=coroutine.resume(co,20); assert(ok and v==21); collectgarbage(); ok,v=coroutine.resume(co,21); assert(ok and v==42 and coroutine.status(co)=='dead'); assert(not coroutine.resume(co))");
    execute(s, "local closed=false; local co=coroutine.create(function() local x<close> =setmetatable({}, {__close=function(x,e) assert(e==nil); closed=true end}); coroutine.yield('ready'); error('must not run') end); assert(coroutine.resume(co)); assert(coroutine.close(co)); assert(closed and coroutine.status(co)=='dead')");
    execute(s, "local f=coroutine.wrap(function() for i=1,3 do coroutine.yield(i) end return 4 end); assert(f()==1 and f()==2 and f()==3 and f()==4); assert(not pcall(f)); local c=coroutine.create(function() return pcall(function() coroutine.yield(12); return 42 end) end); local ok,v=coroutine.resume(c); assert(ok and v==12); local a,b,d=coroutine.resume(c); assert(a and b and d==42)");
    execute(s, "do local co=coroutine.create(function() local x={1,2,3}; coroutine.yield(); return x end); assert(coroutine.resume(co)) end; collectgarbage(); local t,main=coroutine.running(); assert(type(t)=='thread' and main and not coroutine.isyieldable())");
    execute(s, "local f; local weak=setmetatable({}, {__mode='v'}); do local x={}; weak[1]=x; local y=42; f=function() return y end end; collectgarbage(); assert(weak[1]==nil and f()==42); local ok,e=pcall(error,nil); assert(not ok and e==nil)");
    execute(s, "local s=string.pack('>i2I4c3zs1',-123,0xabcdef,'ab','hello','xy'); local a,b,c,d,e,p=string.unpack('>i2I4c3zs1',s); assert(a==-123 and b==0xabcdef and c=='ab\\0' and d=='hello' and e=='xy' and p==#s+1); assert(string.packsize('!4bi4')==8); assert(not pcall(string.pack,'I1',256))");
    execute(s, "assert(string.packsize('Xx')==0 and string.packsize('!3b')==1); assert(not pcall(string.packsize,'X i4')); assert(not pcall(string.unpack,'i9',string.pack('I8',-1)..'\\0')); local f=assert(load('return '..string.format('%q',math.mininteger))); assert(f()==math.mininteger and math.type(f())=='integer')");
    execute(s, "local f=assert(io.tmpfile()); f:write('123abc'); f:seek('set'); assert(f:read('n')==123 and f:read('a')=='abc'); f:seek('set'); f:write('1e+X'); f:seek('set'); assert(f:read('n')==nil and f:read(1)=='X'); f:close()");
    execute(s, "local f=function(a,b,...) return a+b,... end; local image=string.dump(f); local g=assert(load(image,nil,'b')); assert(g(20,22)==42); assert(not load(image,nil,'t')); local i=0; local h=assert(load(function() i=i+1; return ({'return ','40+2'})[i] end)); assert(h()==42)");
    execute(s, "local i=0; local f=assert(load(function() i=i+1; return ({'return ',42})[i] end)); assert(f()==42); assert(utf8.len(123)==3 and string.format(123)=='123'); assert(not pcall(string.sub,'abc')); assert(not pcall(rawget,{})); assert(not pcall(getmetatable)); assert(not pcall(ipairs)); assert(not pcall(math.type)); assert(not pcall(pcall))");
    execute(s, "assert(not pcall(debug.getmetatable)); assert(not pcall(debug.setmetatable,1)); assert(debug.getmetatable(nil)==nil); local ok,f,e=pcall(load,42); assert(ok and f==nil and type(e)=='string'); assert(not pcall(load,{}))");
    execute(s, "local x=41; local function f(a) return x+a end; local name,value=debug.getupvalue(f,1); assert(name=='x' and value==41); assert(debug.setupvalue(f,1,40)=='x' and f(2)==42); local g=(function() local y=1; return function() return y end end)(); debug.upvaluejoin(g,1,f,1); assert(g()==40 and debug.upvalueid(g,1)==debug.upvalueid(f,1)); local info=debug.getinfo(f,'uS'); assert(info.nups==1 and info.nparams==1 and info.what=='Lua')");
    execute(s, "local weak=setmetatable({}, {__mode='v'}); local id; do local x={}; weak[1]=x; local function f() return x end; id=debug.upvalueid(f,1) end; collectgarbage(); assert(weak[1]==nil); local w=setmetatable({id},{__mode='v'}); id=nil; collectgarbage(); assert(type(w[1])=='userdata'); local mt=debug.getmetatable(w[1]); debug.setmetatable(w[1],{__index={answer=42}}); assert(w[1].answer==42); local f=io.tmpfile(); assert(f.answer==nil); f:close(); debug.setmetatable(w[1],mt)");
    execute(s, "local events=0; debug.sethook(function(event) events=events+1 end,'cr',10); local f=function(x) return x+1 end; assert(f(41)==42); debug.sethook(); assert(events>0); assert(type(debug.traceback())=='string'); assert(debug.getmetatable('x')==getmetatable('x'))");
    execute(s, "local log={}; local ok,e=xpcall(function() local x<close> =setmetatable({}, {__close=function(x,e) assert(e==11); log[#log+1]='c' end}); error(10) end,function(e) assert(e==10); log[#log+1]='h'; return 11 end); assert(not ok and e==11 and table.concat(log)=='hc')");
    execute(s, "local log={}; local a,b={},{}; setmetatable(b,{__gc=function() log[#log+1]='b' end}); setmetatable(a,{__gc=function() log[#log+1]='a' end}); a,b=nil,nil; collectgarbage(); assert(table.concat(log)=='ab')");
    execute(s, "local done=false; local co=coroutine.create(function() local x<close> =setmetatable({}, {__close=function(_,e) assert(e==5); done=true end}); error(5) end); local ok,e=coroutine.resume(co); assert(not ok and e==5 and not done and coroutine.status(co)=='dead'); collectgarbage(); assert(debug.getinfo(co,1,'S').what=='Lua'); ok,e=coroutine.close(co); assert(not ok and e==5 and done)");
    execute(s, "local log=''; local co=coroutine.create(function() local x<close> =setmetatable({}, {__close=function() log=log..'c' end}); pcall(coroutine.yield); log=log..'x' end); coroutine.resume(co); assert(coroutine.close(co)); assert(log=='c')");
    execute(s, "local resurrected; do local c=coroutine.create(function() coroutine.yield(1); return 2 end); coroutine.resume(c); local t=setmetatable({c}, {__gc=function(x) resurrected=x[1] end}) end; collectgarbage(); local ok,v=coroutine.resume(resurrected); assert(ok and v==2)");
    execute(s, "local old=debug.getmetatable(''); debug.setmetatable('',{__index=string,__add=function() return 42 end}); assert('3'+2==42); debug.setmetatable('',old); assert('3'+2==5); for i='1',2 do assert(math.type(i)=='float') end; assert(require('_G')==_G)");
    execute(s, "local n=0; local p=setmetatable({}, {__index=function(_,k) return k end,__len=function() n=n+1; return 3 end}); table.move(p,1,2,2); assert(table.unpack(p,1,1)==1 and n==0); assert(not pcall(math.fmod,1,nil)); assert(not pcall(string.format,'%+s','x')); assert(not pcall(tostring)); assert(not pcall(tonumber)); assert(not pcall(rawequal)); assert(collectgarbage('collect')==0)");
    execute(s, "local f=assert(load('local function f()\\n error(\"boom\")\\nend\\nreturn f', '=location'))(); local ok,e=pcall(f); assert(not ok and e=='location:2: boom'); local g=assert(load('return function() error(\"plain\",0) end', '=ignored'))(); ok,e=pcall(g); assert(not ok and e=='plain'); assert(debug.getinfo(f,'S').source=='=location')");
    /* Every Lua 5.4 standard-library entry must be installed, not a stub. */
    execute(s, "assert(_VERSION=='Lua 5.4' and _G._G==_G); for name in ('assert collectgarbage dofile error getmetatable ipairs load loadfile next pairs pcall print rawequal rawget rawlen rawset require select setmetatable tonumber tostring type warn xpcall'):gmatch('%S+') do assert(type(_G[name])=='function',name) end");
    execute(s, "for name in ('abs acos asin atan ceil cos deg exp floor fmod log max min modf rad random randomseed sin sqrt tan tointeger type ult'):gmatch('%S+') do assert(type(math[name])=='function',name) end; for name in ('byte char dump find format gmatch gsub len lower match pack packsize rep reverse sub unpack upper'):gmatch('%S+') do assert(type(string[name])=='function',name) end");
    execute(s, "for name in ('concat insert move pack remove sort unpack'):gmatch('%S+') do assert(type(table[name])=='function',name) end; for name in ('char codes codepoint len offset'):gmatch('%S+') do assert(type(utf8[name])=='function',name) end; assert(type(utf8.charpattern)=='string'); for name in ('close create isyieldable resume running status wrap yield'):gmatch('%S+') do assert(type(coroutine[name])=='function',name) end");
    execute(s, "for name in ('close flush input lines open output popen read tmpfile type write'):gmatch('%S+') do assert(type(io[name])=='function',name) end; for name in ('clock date difftime execute exit getenv remove rename setlocale time tmpname'):gmatch('%S+') do assert(type(os[name])=='function',name) end; for name in ('debug gethook getinfo getlocal getmetatable getregistry getupvalue getuservalue sethook setlocal setmetatable setupvalue setuservalue traceback upvalueid upvaluejoin'):gmatch('%S+') do assert(type(debug[name])=='function',name) end; assert(type(package.loadlib)=='function' and type(package.searchpath)=='function' and #package.searchers==4)");
    execute(s, "local old=next; next=function() error('overridden next') end; local f,t,k=pairs({42}); next=old; assert(f==old and f(t,k)==1); local a=ipairs({}); local b=ipairs({}); assert(a==b); assert(select('#more',1,nil,3)==3); local ok,e=pcall(assert,false,nil); assert(not ok and e==nil); assert(not pcall(assert))");
    execute(s, "local t=setmetatable({}, {__tostring=function() return 42 end}); assert(tostring(t)=='42' and string.format('%s',t)=='42'); assert(tostring(setmetatable({}, {__name='widget'})):match('^widget: ')); assert(not pcall(tostring,setmetatable({}, {__tostring=function() return {} end}))); warn('@off'); assert(pcall(warn,42) and not pcall(warn,false))");
    execute(s, "assert(math.type(math.abs('3'))=='float' and math.type(math.fmod('3','2'))=='float'); assert(math.abs(math.mininteger)==math.mininteger and math.ult(0,-1)); assert(math.max('a','z','b')=='z' and math.min('a','z','b')=='a'); local i,f=math.modf(-math.huge); assert(i==-math.huge and f==0 and 1/f==math.huge); assert(math.sin(0)==0 and math.cos(0)==1 and math.tan(0)==0 and math.asin(0)==0 and math.acos(1)==0 and math.atan(0)==0); assert(math.sqrt(4)==2 and math.exp(0)==1 and math.log(8,2)==3 and math.ceil(-1.5)==-1); assert(math.abs(math.rad(180)-math.pi)<1e-14 and math.abs(math.deg(math.pi)-180)<1e-12)");
    execute(s, "local x,y=math.randomseed(1,2); assert(x==1 and y==2); local a,b,c=math.random(),math.random(0),math.random(-10,10); assert(a>=0 and a<1 and math.type(b)=='integer' and c>=-10 and c<=10); math.randomseed(1,2); assert(a==math.random() and b==math.random(0) and c==math.random(-10,10)); for i=1,50 do assert(math.random(1)==1); local n=math.random(math.mininteger,math.maxinteger); assert(math.type(n)=='integer') end; assert(not pcall(math.random,1,2,3) and not pcall(math.randomseed,nil) and not pcall(math.random,0,-1))");
    execute(s, "assert(not pcall(table.move,{}) and not pcall(table.move,{},1)); local t={3,1,4,2}; table.sort(t,function(a,b) collectgarbage(); return a>b end); assert(table.concat(t,',')=='4,3,2,1'); local a={10,20,30,40}; assert(table.move(a,2,4,1)==a and table.concat(a,',')=='20,30,40,40'); local b={}; assert(table.move(a,1,2,-2,b)==b and b[-2]==20 and b[-1]==30)");
    execute(s, "local t=setmetatable({}, {__index=function(_,i) return i*2 end,__newindex=function(t,i,v) rawset(t,i,v) end,__len=function() return 3 end}); assert(table.concat(t,':')=='2:4:6'); assert(table.unpack(t,2,2)==4); local n=0; for i,v in ipairs(setmetatable({}, {__index=function(_,i) if i<4 then return i end end})) do n=n+v end; assert(n==6); local f,state,k=pairs(setmetatable({}, {__pairs=function() return next,{answer=42},nil end})); assert(f(state,k)=='answer')");
    execute(s, "assert(string.find(')',')')==1 and string.find('abc',')')==nil); assert(not pcall(string.match,'abc','%0') and not pcall(string.find,'','%0')); assert(string.find('a.b','.b',1,true)==2); assert(string.find('a\\0b','\\0b')==2); assert(string.gsub('abc','(','x')=='xaxbxcx'); assert(string.gsub('abc','(a)(','%1')=='abc'); assert(string.gsub('abc','()','%1')=='1a2b3c4')");
    execute(s, "for _,v in ipairs({true,false}) do assert(not pcall(string.gsub,'abc','x',v,0)) end; assert(not pcall(string.gsub,'abc','x',nil)); local text,n=string.gsub('abc','.',function(c) if c=='a' then return false elseif c=='b' then return nil else return 42 end end); assert(text=='ab42' and n==3); assert(string.gsub('aba','.',setmetatable({}, {__index=function(_,c) return c:upper() end}))=='ABA'); assert(not pcall(string.gsub,'x','.',function() return {} end))");
    execute(s, "assert(('a(b)c(d)'):match('%b()')=='(b)'); assert(('xx'):match('%bxx')=='xx'); local a,b=('a1 b22'):match('(%a)(%d+)$'); assert(a=='b' and b=='22'); local t={}; for c in string.gmatch('abc','',2) do t[#t+1]=c end; assert(#t==3); assert(string.reverse('a\\0b')=='b\\0a'); assert(select('#',string.byte('abc',1,3))==3)");
    execute(s, "assert(string.format('%p',nil)=='(null)' and string.format('%10p',false)=='    (null)'); assert(string.format('%p',{}) :match('^0x')); assert(not pcall(string.format,'%+p',{}) and not pcall(string.format,'%.2p',{})); for _,v in ipairs({math.huge,-math.huge,1.25,math.mininteger,math.maxinteger}) do local f=assert(load('return '..string.format('%q',v))); assert(f()==v) end; assert(assert(load('return '..string.format('%q','a\\0b\\n\\255')))()=='a\\0b\\n\\255')");
    execute(s, "for _,order in ipairs({'<','>'}) do local image=string.pack(order..'!8bhi4jJfdnc5zs2',-12,-123,-12345,math.mininteger,-1,1.5,2.5,3.5,'ab','cd','ef'); local a,b,c,d,e,f,g,h,i,j,k,p=string.unpack(order..'!8bhi4jJfdnc5zs2',image); assert(a==-12 and b==-123 and c==-12345 and d==math.mininteger and e==-1 and f==1.5 and g==2.5 and h==3.5 and i=='ab\\0\\0\\0' and j=='cd' and k=='ef' and p==#image+1) end; assert(not pcall(string.pack,'z','a\\0b') and not pcall(string.unpack,'z','abc') and not pcall(string.packsize,'z'))");
    execute(s, "assert(utf8.len('abc',4)==0 and select('#',utf8.codepoint('abc',5,1))==0); for _,text in ipairs({'a\\128b','\\194\\162\\128','\\255','\\192\\128'}) do assert(not pcall(function() for p,c in utf8.codes(text) do end end)) end; local s=utf8.char(0xd800,0x110000,0x7fffffff); assert(utf8.len(s)==nil and utf8.len(s,1,-1,true)==3); local n=0; for p,c in utf8.codes(s,true) do n=n+1 end; assert(n==3); local s=utf8.char(65,0x20ac,66); assert(utf8.offset(s,0,3)==2 and utf8.offset(s,2)==2 and utf8.offset(s,-1)==5); assert(not pcall(utf8.offset,s,1,3))");
    execute(s, "assert(not pcall(io.type) and io.type(nil)==nil and not pcall(io.close,nil)); local f=assert(io.tmpfile()); for name in ('close flush lines read seek setvbuf write'):gmatch('%S+') do assert(type(f[name])=='function',name) end; assert(f:setvbuf('full',1024)); assert(f:write('10 20\\nhello\\nlast')==f and f:flush()); assert(f:seek('set')==0); assert(f:read(0)==''); local a,b=f:read('*number','number'); assert(a==10 and b==20); assert(f:read('*line')==''); assert(f:read('*L')=='hello\\n'); assert(f:read('*all')=='last' and f:read(0)==nil and f:read('a')==''); assert(f:close())");
    execute(s, "local path=os.tmpname(); local oldin,oldout=io.input(),io.output(); assert(io.output(path):write('one\\ntwo\\n')); assert(io.flush()); assert(io.close()); io.output(oldout); assert(io.input(path)==io.input()); assert(io.read('l')=='one'); io.close(io.input()); io.input(oldin); local iterator,state,k,file=io.lines(path); assert(io.type(file)=='file'); assert(iterator()=='one' and iterator()=='two' and iterator()==nil and io.type(file)=='closed file'); assert(os.remove(path))");
    execute(s, "local path=os.tmpname(); local f=assert(io.open(path,'w')); f:write('1 2\\n3 4\\n'); f:close(); for a,b in io.lines(path,'n','n') do assert(a+b==3); break end; local g=assert(io.open(path)); local it=g:lines('n','n'); local a,b=it(); assert(a==1 and b==2); assert(io.type(g)=='file'); g:close(); assert(os.remove(path)); assert(not pcall(io.open,'unused','r++') and not pcall(io.open,'unused','x'))");
    execute(s, "assert(os.date('!%Y-%m-%d %H:%M:%S',0)=='1970-01-01 00:00:00'); assert(os.date('a\\0b',0)=='a\\0b' and os.date('',0)==''); for _,fmt in ipairs({'%','%Q','%E','%O'}) do assert(not pcall(os.date,fmt,0)) end; assert(#os.date(string.rep('x',5000),0)==5000); local d=os.date('*t',os.time()); assert(os.time(d)==os.time(d) and type(d.isdst)=='boolean'); assert(type(os.setlocale(nil,'numeric'))=='string' and os.setlocale('C','numeric')); assert(not pcall(os.setlocale,'C','bad-category'))");
    execute(s, "local d=setmetatable({}, {__index={year=2000,month=2,day=30,hour=12,min=0,sec=0,isdst=false}}); assert(type(os.time(d))=='number' and d.month==3 and d.day==1); local from,to=os.tmpname(),os.tmpname(); assert(os.remove(to)); assert(os.rename(from,to) and os.remove(to)); local ok,msg,code=os.remove(to); assert(ok==nil and type(msg)=='string' and type(code)=='number')");
    execute(s, "local registry=debug.getregistry(); assert(registry._LOADED==package.loaded and registry._PRELOAD==package.preload and registry['FILE*']==getmetatable(io.stdout)); local p=package; package=nil; assert(require('math')==math); package=p; local loaded=p.loaded; p.loaded={}; assert(require('math')==math); p.loaded=loaded; local searchers=p.searchers; p.searchers=nil; assert(require('math')==math); p.searchers=searchers; local preload=p.preload; p.preload={}; registry._PRELOAD.stdlib_preload=function() return 42 end; assert(require('stdlib_preload')==42); p.loaded.stdlib_preload=nil; preload.stdlib_preload=nil; p.preload=preload");
    execute(s, "local old=package.searchers; package.searchers={function(name) return 'custom search failure' end}; local ok,msg=pcall(require,'stdlib_absent'); package.searchers=old; assert(not ok and msg:find('custom search failure',1,true)); local path,msg=package.searchpath('stdlib_absent','first/?.lua;second/?.lua'); assert(path==nil and msg:find('first/stdlib_absent.lua',1,true) and msg:find('second/stdlib_absent.lua',1,true)); assert(not pcall(package.searchpath,'x','?.lua',false) and not pcall(package.searchpath,'x','?.lua',nil,{}))");
    execute(s, "local old=package.searchers; package.searchers={function(name) return function(name,data) assert(data=='data'); return false end,'data' end}; local a,data=require('stdlib_false'); assert(a==false and data=='data'); assert(require('stdlib_false')==false); package.loaded.stdlib_false=nil; package.searchers={function() return function(name) package.loaded[name]=42 end end}; assert(require('stdlib_explicit')==42); package.loaded.stdlib_explicit=nil; package.searchers=old");
    execute(s, "local path=os.tmpname(); local f=assert(io.open(path,'wb')); f:write(string.dump(function() return {answer=42} end)); f:close(); assert(assert(loadfile(path,nil))().answer==42 and dofile(path).answer==42); local old=package.path; package.path=path; assert(require('stdlib_binary').answer==42); package.path=old; package.loaded.stdlib_binary=nil; assert(os.remove(path))");
    execute(s, "local f=assert(load('return function(a,...) return a,... end','=dump-name'))(); local image=string.dump(f); local g=assert(load(image,'=ignored','b')); assert(debug.getinfo(g,'S').source=='=dump-name' and debug.getlocal(g,1)=='a'); local stripped=string.dump(f,true); local h=assert(load(stripped)); assert(#stripped<#image and debug.getinfo(h,'S').source=='=?' and debug.getlocal(h,1)==nil and next(debug.getinfo(h,'L').activelines)==nil); assert(h(20,22)==20); for i=1,#stripped-1 do assert(not load(stripped:sub(1,i),nil,'b')) end; if image:sub(1,5)=='\\27MUNE' then assert(not load(image..'x',nil,'b')); local legacy=image:sub(1,6)..'\\1\\0'..image:sub(9,25)..image:sub(35+string.unpack('<I8',image,27)); assert(assert(load(legacy,nil,'b'))(42)==42) end");
    execute(s, "local x=42; local f=function() return x end; local g=assert(load(string.dump(f,true),nil,'b',17)); local name,v=debug.getupvalue(g,1); assert(name=='(no name)' and v==17 and g()==17); assert(debug.setupvalue(g,1,42)=='(no name)' and g()==42); assert(not pcall(debug.setupvalue,g,1) and not pcall(debug.setlocal,1,1) and not pcall(debug.setuservalue,nil,nil)); assert(select('#',debug.gethook())==1)");
    execute(s, "local name,p=debug.getupvalue(require,1); assert(name=='' and p==package); assert(debug.getinfo(require,'u').nups==1 and debug.getinfo(print,'u').nups==0); assert(debug.upvalueid(require,1)==debug.upvalueid(require,1)); assert(debug.setupvalue(require,1,package)==''); assert(not pcall(debug.upvaluejoin,require,1,require,1)); local name,v=debug.getlocal(0,1); assert(type(name)=='string' and v==0)");
    execute(s, "local function f(a,...) local name,v=debug.getlocal(1,1); assert(name=='a' and v==20); assert(debug.setlocal(1,1,40)=='a' and a==40); name,v=debug.getlocal(1,-1); assert(name=='(vararg)' and v==2); assert(debug.setlocal(1,-1,3)=='(vararg)'); return a,... end; local a,b=f(20,2); assert(a==40 and b==3); local marker={}; assert(debug.traceback(marker)==marker and type(debug.traceback('message'))=='string')");
    execute(s, "local calls,returns=0,0; local function target(a,b) return a+b,a-b end; debug.sethook(function(event) local info=debug.getinfo(2,'fr'); if info.func==target then assert(info.ftransfer>=1 and info.ntransfer==2); if event=='call' then calls=calls+1 elseif event=='return' then returns=returns+1 end end end,'cr'); local a,b=target(20,22); debug.sethook(); assert(a==42 and b==-2 and calls==1 and returns==1)");
    execute(s, "local main=coroutine.running(); local child; child=coroutine.create(function() assert(coroutine.status(child)=='running' and coroutine.status(main)=='normal' and coroutine.isyieldable()); local nested=coroutine.create(function() assert(coroutine.status(child)=='normal'); return 42 end); assert(select(2,coroutine.resume(nested))==42); return nil,42 end); local ok,a,b=coroutine.resume(child); assert(ok and a==nil and b==42 and coroutine.status(child)=='dead'); assert(coroutine.close(child) and coroutine.close(child)); assert(not pcall(coroutine.isyieldable,nil))");
    execute(s, "assert(('a'):match('()%1')==nil); for _,n in ipairs({199,200,201,500}) do local f=assert(io.tmpfile()); f:write(string.rep('1',n)..'X'); f:seek('set'); local value=f:read('n'); assert((value==nil)==(n>200) and f:seek()==math.min(n,200)); assert(#f:read('a')==n-math.min(n,200)+1); f:close() end");
    execute(s, "local function f() local info=debug.getinfo(1,'n'); return info.name,info.namewhat end; local alias=f; local name,what=alias(); assert(name=='alias' and what=='local'); local t={f=f}; name,what=t.f(); assert(name=='f' and what=='field'); t.m=f; name,what=t:m(); assert(name=='m' and what=='method'); local function caller() local n,w=f(); assert(n=='f' and w=='upvalue') end; caller(); mune_debug_name=f; name,what=mune_debug_name(); mune_debug_name=nil; assert(name=='mune_debug_name' and what=='global')");
    {
        const char limits[]="assert(type(debug.setcstacklimit)=='function'); "
            "assert(debug.setcstacklimit(8)==16); local function f(n) if n==0 then return 0 end return 1+f(n-1) end; "
            "assert(not pcall(f,20)); assert(debug.setcstacklimit(16)==8); "
            "assert(not debug.setcstacklimit(17) and not debug.setcstacklimit(0)); assert(f(2)==2)";
        mulib_mune_runtime_options_t options={0};
        mulib_mune_state_t *bounded;
        mulib_mune_result_t *r=NULL;
        mulib_mune_error_t e;
        options.call_depth=16; bounded=mulib_mune_state_create(&options); assert(bounded);
        assert(mulib_mune_run_source(bounded,limits,sizeof(limits)-1,&r,&e)==MULIB_MUNE_OK);
        mulib_mune_result_destroy(r); mulib_mune_state_destroy(bounded);
    }
    {
        static const char *const limited[]={"return string.rep('x',1000)",
            "return text:find('aaaaab')", "return table.unpack({},1,1000)", "return utf8.len(text)"};
        mulib_mune_runtime_options_t options={0};
        size_t i;
        int mode;
        char text[4096]; memset(text,'a',sizeof(text)); options.steps=100;
        for(mode=MULIB_MUNE_TREE;mode<=MULIB_MUNE_JIT;++mode) {
            if(mode==MULIB_MUNE_JIT && !mulib_jit_supported()) continue;
            for(i=0;i<sizeof(limited)/sizeof(limited[0]);++i) {
                mulib_mune_state_t *bounded=mulib_mune_state_create(&options);
                mulib_mune_result_t *r=NULL;
                mulib_mune_value_t value;
                mulib_mune_error_t e;
                assert(bounded && mulib_mune_set_mode(bounded,(mulib_mune_mode_t)mode)==MULIB_MUNE_OK);
                assert(mulib_mune_make_string(bounded,text,sizeof(text),&value)==MULIB_MUNE_OK);
                assert(mulib_mune_set_global(bounded,"text",value)==MULIB_MUNE_OK);
                assert(mulib_mune_run_source(bounded,limited[i],strlen(limited[i]),&r,&e)==MULIB_MUNE_LIMIT && !r);
                mulib_mune_state_destroy(bounded);
            }
        }
    }
    {
        const char module[]="return {answer=require('test_dep').value+2}";
        const char dependency[]="return {value=40}";
        mulib_mune_bundle_module_t items[]={{"test_bundle",module,sizeof(module)-1},{"test_dep",dependency,sizeof(dependency)-1}};
        mulib_mune_error_t e;
        unsigned char *bytes;
        long size;
        FILE *f=fopen("test_bundle.luac","w+b"); assert(f);
        assert(mulib_mune_bundle_write(items,2,writer,f,&e)==MULIB_MUNE_OK);
        size=ftell(f); assert(size>0); rewind(f); bytes=(unsigned char *)malloc((size_t)size); assert(bytes);
        assert(fread(bytes,1,(size_t)size,f)==(size_t)size && !memcmp(bytes,"MUNEBC\0\2",8));
        assert(mulib_mune_load_bundle(s,bytes,(size_t)size-1,&e)==MULIB_MUNE_INVALID);
        bytes[size-1]^=1;
        assert(mulib_mune_load_bundle(s,bytes,(size_t)size,&e)==MULIB_MUNE_INVALID);
        bytes[size-1]^=1;
        assert(mulib_mune_load_bundle(s,bytes,(size_t)size,&e)==MULIB_MUNE_OK);
        free(bytes);
        assert(fclose(f)==0);
        execute(s,"package.loaded.test_bundle=nil; package.loaded.test_dep=nil; package.preload.test_bundle=nil; local path=package.path; package.path='./?.luac'; local m=require('test_bundle'); package.path=path; assert(m.answer==42)");
        assert(remove("test_bundle.luac")==0);
    }
    execute(s, "saved=(function() local x=42; return function() return x end end)()");
    mulib_mune_collect(s);
    execute(s, "assert(saved()==42)");
    {
        mulib_mune_value_t fn, args[2]={{MULIB_MUNE_INTEGER,{0}},{MULIB_MUNE_INTEGER,{0}}}, object, key, v;
        mulib_mune_result_t *r=NULL, *roots=mulib_mune_result_create(s);
        mulib_mune_state_t *other=mulib_mune_state_create(NULL);
        mulib_mune_error_t e;
        void *data;
        size_t bytes;
        assert(roots && other);
        assert(mulib_mune_bind(s,"native_sum",native_sum,NULL)==MULIB_MUNE_OK);
        assert(mulib_mune_get_global(s,"native_sum",&fn)==MULIB_MUNE_OK);
        args[0].as.integer=20; args[1].as.integer=22;
        assert(mulib_mune_call(s,fn,args,2,&r,&e)==MULIB_MUNE_OK);
        assert(mulib_mune_result_get(r,0).type==MULIB_MUNE_INTEGER && mulib_mune_result_get(r,0).as.integer==42); mulib_mune_result_destroy(r);
        assert(mulib_mune_bind(s,"native_roundtrip",native_roundtrip,NULL)==MULIB_MUNE_OK);
        execute(s,"local co=coroutine.create(function() return native_roundtrip(function(x) collectgarbage(); coroutine.yield(x); return {answer=x.a} end,{a=42}) end); local ok,x=coroutine.resume(co); assert(ok and x.a==42); collectgarbage(); ok,x=coroutine.resume(co); assert(ok and x.answer==42)");
        assert(mulib_mune_make_userdata(s,16,&object,&data)==MULIB_MUNE_OK);
        assert(mulib_mune_value_userdata(object,&bytes)==data && bytes==16 && ((unsigned char *)data)[15]==0);
        assert(mulib_mune_result_push(roots,object));
        assert(mulib_mune_set_global(s,"host_data",object)==MULIB_MUNE_OK);
        assert(mulib_mune_set_global(other,"foreign",object)==MULIB_MUNE_INVALID);
        execute(s,"assert(io.type(host_data)==nil); assert(debug.setuservalue(host_data,42)==host_data); assert(debug.getuservalue(host_data)==42)");
        assert(mulib_mune_make_table(s,&object)==MULIB_MUNE_OK && mulib_mune_result_push(roots,object));
        assert(mulib_mune_make_string(s,"key",3,&key)==MULIB_MUNE_OK && mulib_mune_result_push(roots,key));
        assert(mulib_mune_raw_set(s,object,key,args[1])==MULIB_MUNE_OK);
        mulib_mune_collect(s);
        assert(mulib_mune_raw_get(s,object,key,&v)==MULIB_MUNE_OK && v.as.integer==22);
        mulib_mune_result_destroy(roots); mulib_mune_state_destroy(other);
    }
    mulib_mune_state_destroy(s);
    puts("Mune runtime tests passed"); return 0;
}
