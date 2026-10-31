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
        execute(s,"package.loaded.test_bundle=nil; package.loaded.test_dep=nil; package.preload.test_bundle=nil; local m=require('test_bundle'); assert(m.answer==42)");
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
