#include "../stdppx/luaembed/runtime.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static bool evaluate(const char *source)
{
    return on1x_luaembed_eval(source, "(Loc \"fixture.on1x\" 0 1 1)", "LuaEmbed", "(Unit 1 ())");
}

int main(void)
{
    assert(evaluate("print('hello', 42); io.write('tail'); io.stdout:write('!')"));
    assert(!strcmp(on1x_luaembed_output(), "hello\t42\ntail!"));
    assert(evaluate("local L=require('llembed'); L.emit(L.integer(42))"));
    assert(!strcmp(on1x_luaembed_output(), "(DecimalLiteral \"42\" (Loc \"fixture.on1x\" 0 1 1))\n"));
    assert(evaluate("assert(os==nil and debug==nil and package==nil and load==nil);"
                    "assert(io.open==nil and math.random==nil); print(llembed.version)"));
    assert(!strcmp(on1x_luaembed_output(), "1\n"));
    assert(!evaluate("print('discard'); error('bad\\27[31m')"));
    assert(!*on1x_luaembed_output());
    assert(strstr(on1x_luaembed_error(), "\\x1b[31m"));
    assert(!strchr(on1x_luaembed_error(), '\033'));
    assert(!evaluate("local <const> x=1"));
    assert(!*on1x_luaembed_output());
    assert(!evaluate("print('discard'); pcall(function() io.write(string.char(0)) end)"));
    assert(!*on1x_luaembed_output());
    assert(!evaluate("local s=string.rep('x',8192);"
                     "pcall(function() for i=1,65 do io.write(s) end end)"));
    assert(!*on1x_luaembed_output());
    assert(!evaluate("while true do end"));
    assert(!*on1x_luaembed_output());
    assert(!evaluate("local x=string.rep('x',16777216)"));
    assert(!*on1x_luaembed_output());
    assert(!evaluate("require('untrusted.module')"));
    assert(!evaluate(NULL));
    assert(!on1x_luaembed_eval("", NULL, "", ""));
    return 0;
}
