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
#include <stdlib.h>
#include <string.h>

static const char *const valid[] = {
    "", " --comment\n ; ; ", "return ...", "return nil, true, false, 0x1.fp3, .5e-2",
    "return -2^2, 2^-2, 2^3^4, 1+2*3//4%5, a..b..c, a|b~c&d<<e>>f",
    "return a and b or not c, a==b, a~=b, a<=b, a>=b, #a, ~a",
    "local a, b<const>, c<close> = 1, {}, nil; b.x = a; return b",
    "local x<const> = 1; do local x=2; x=3 end; return x",
    "local f; function f(a,b,...) return a,b,... end; return f(1,2,3)",
    "local function fact(n) if n<=1 then return 1 else return n*fact(n-1) end end; return fact(10)",
    "function a.b:c(x,...) self.x=x; return self,... end; a.b:c{}",
    "local x<const> ={}; function x.f() return x end",
    "for i=1,10,2 do if i==5 then break end end",
    "for k,v in pairs({a=1,[2]=3,4;}) do print(k,v) end",
    "repeat local x=read() until x ~= nil",
    "while true do local function f(...) return ... end; break end",
    "if a then f() elseif b then g'hello' else h[[world]] end",
    "local a = { [f()] = g(), name = 'value', 3, f(); }; return a",
    "local f=(function(x) return function(y) return x+y end end)(1); return f(2)",
    "a,b[1],c.d=f(),g(),h(); (a).b=2; f()[x]=3",
    "return f'a':g{}[1].x(y), (f())",
    "goto done; local x=1; ::done:: ; ::last::",
    "::again:: local x=1; do goto again end",
    "local x=1; do local y=2; goto done end; ::done:: return x",
    "::same:: do ::inner:: goto same end; goto same",
    "do ::same:: end ::same::", "do ::same:: end do ::same:: end",
    "::same:: function f() ::same:: end",
    "return [==[abc ]=] xyz\n]==]",
    "--[==[ ignored \" ' ]=] ]==]\nreturn 'ok'",
    "return '\\a\\b\\f\\n\\r\\t\\v\\\\\\\"\\\'\\x00\\255\\u{7fffffff}\\z  \n x'",
    "return 'line\\\ncontinuation'",
    "local a,a=1,2; a=3; return a",
    "local x<const> =1; local function f() local x=2; x=3; return x end",
    "for i=1,3 do i=4 end",
    "repeat local x; goto l; ::l:: until true"
};
static const char *const invalid[] = {
    "local", "local = 1", "return 1 return 2", "return 1..2", "return 1e+", "return 0x",
    "return 1foo", "return 0x1p-", "return 'unterminated", "return [[unfinished",
    "return '\\q'", "return '\\x0g'", "return '\\256'", "return '\\u{}'", "return '\\u{80000000}'",
    "return 'raw\nnewline'", "return +1", "return 1 != 2", "return 1 && 2",
    "f()=1", "(a)=1", "a", "a.b", "break", "do break end",
    "while true do local function f() break end end",
    "local function f() return ... end", "local function f(...) return function() return ... end end",
    "function f(a,...,b) end", "function f(a,) end",
    "local x<const> =1; x=2", "local x<close> =nil; x=nil",
    "local x<const> =1; function f() x=2 end", "local x<const> =1; function x() end",
    "local a<bad> =1", "local a<close>,b<close> =nil,nil",
    "goto absent", "::x:: ::x::", "::x:: do ::x:: end", "goto inner; do ::inner:: end",
    "::outer:: function f() goto outer end", "goto done; local x; ::done:: return x",
    "repeat goto l; local x; ::l:: until x",
    "do goto done end; local x; ::done:: print(x)",
    "if true then end else end", "for x=1 do end", "for x in do end", "return {x=}",
    "return ... ...", "return a:", "function a:b.c() end", "return [=[missing ]]"
};
static void spans(const mulib_ast_t *n, const char *source, size_t length)
{
    const mulib_ast_t *c;
    assert(n->offset<=length && n->length<=length-n->offset);
    assert(!memcmp(n->text,source+n->offset,n->length));
    for(c=n->child;c;c=c->next) {
        assert(c->parent==n);
        spans(c,source,length);
    }
}
static void corpus(void)
{
    size_t i;
    for(i=0;i<sizeof(valid)/sizeof(valid[0]);++i) {
        mulib_mune_error_t e;
        mulib_mune_chunk_t *c=mulib_mune_parse(valid[i],strlen(valid[i]),NULL,&e);
        if(!c) fprintf(stderr,"valid[%zu]: %s at %zu: %s\n",i,e.message,e.offset,valid[i]);
        assert(c && e.code==MULIB_MUNE_OK);
        assert(mulib_mune_chunk_ast(c)->length==strlen(valid[i]));
        spans(mulib_mune_chunk_ast(c),valid[i],strlen(valid[i]));
        mulib_mune_chunk_destroy(c);
    }
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        mulib_mune_error_t e;
        mulib_mune_chunk_t *c=mulib_mune_parse(invalid[i],strlen(invalid[i]),NULL,&e);
        if(c) fprintf(stderr,"invalid[%zu] accepted: %s\n",i,invalid[i]);
        assert(!c && e.code!=MULIB_MUNE_OK && e.offset<=strlen(invalid[i]));
    }
}
static void limits_and_ownership(void)
{
    mulib_mune_parse_options_t o={0};
    mulib_mune_error_t e;
    mulib_mune_chunk_t *c;
    char source[]="  local x=1; return x  ";
    const char nul[]={ 'r','e','t','u','r','n',' ',0 };
    c=mulib_mune_parse(source,strlen(source),NULL,&e); assert(c);
    memset(source,'?',strlen(source));
    assert(strstr(mulib_mune_chunk_ast(c)->text,"local x=1")); mulib_mune_chunk_destroy(c);
    assert(!mulib_mune_parse(NULL,1,NULL,&e) && e.code==MULIB_MUNE_INVALID);
    assert(!mulib_mune_parse("",SIZE_MAX,NULL,&e) && e.code==MULIB_MUNE_INVALID);
    c=mulib_mune_parse(NULL,0,NULL,NULL); assert(c); mulib_mune_chunk_destroy(c);
    assert(!mulib_mune_parse(nul,sizeof(nul),NULL,&e) && e.code==MULIB_MUNE_SYNTAX && e.offset==7);
    o.source_bytes=2;
    assert(!mulib_mune_parse("return 1",8,&o,&e) && e.code==MULIB_MUNE_LIMIT);
    o.source_bytes=0; o.tokens=1;
    assert(!mulib_mune_parse("return 1",8,&o,&e) && e.code==MULIB_MUNE_LIMIT);
    o.tokens=0; o.nodes=1;
    assert(!mulib_mune_parse("return 1",8,&o,&e) && e.code==MULIB_MUNE_LIMIT);
    o.nodes=0; o.symbols=1;
    assert(!mulib_mune_parse("local a,b",9,&o,&e) && e.code==MULIB_MUNE_LIMIT);
    o.symbols=0; o.steps=1;
    assert(!mulib_mune_parse("return 1",8,&o,&e) && e.code==MULIB_MUNE_LIMIT);
    assert(!mulib_mune_chunk_ast(NULL)); mulib_mune_chunk_destroy(NULL);
}
static void files(void)
{
    const char text[]="\xef\xbb\xbf#!/usr/bin/lua\nlocal x=1; return x";
    mulib_mune_error_t e;
    mulib_mune_chunk_t *c;
    FILE *f=fopen("mune-frontend-test.lua","wb"); assert(f);
    assert(fwrite(text,1,sizeof(text)-1,f)==sizeof(text)-1 && fclose(f)==0);
    c=mulib_mune_parse_file("mune-frontend-test.lua",NULL,&e); assert(c);
    assert(mulib_mune_chunk_ast(c)->length==sizeof(text)-1); mulib_mune_chunk_destroy(c);
    assert(remove("mune-frontend-test.lua")==0);
    assert(!mulib_mune_parse_file("mune-frontend-missing.lua",NULL,&e) && e.code==MULIB_MUNE_IO);
    assert(!mulib_mune_parse_file(NULL,NULL,&e) && e.code==MULIB_MUNE_INVALID);
}
int main(int argc, char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--corpus")) {
        size_t i;
        for(i=0;i<sizeof(valid)/sizeof(valid[0]);++i) {
            putchar('1'); fwrite(valid[i],1,strlen(valid[i]),stdout); putchar(0);
        }
        for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
            putchar('0'); fwrite(invalid[i],1,strlen(invalid[i]),stdout); putchar(0);
        }
        return ferror(stdout)?1:0;
    }
    if(argc==3 && !strcmp(argv[1],"--check")) {
        mulib_mune_error_t e;
        mulib_mune_chunk_t *c=mulib_mune_parse_file(argv[2],NULL,&e);
        if(!c) { fprintf(stderr,"%zu: %s\n",e.offset,e.message); return 1; }
        mulib_mune_chunk_destroy(c); return 0;
    }
    corpus(); limits_and_ownership(); files();
    puts("Mune Lua frontend tests passed"); return 0;
}
