/* Caller TU only: all kernel implementation switches belong to libmulib.a. */
#include "mulib-mune.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

struct mune_contract_bytes {
    char data[16384];
    size_t length;
};

static int mune_contract_write(void *user, const char *bytes, size_t length)
{
    struct mune_contract_bytes *out = (struct mune_contract_bytes *)user;
    if (length > sizeof(out->data) - out->length) return 0;
    memcpy(out->data + out->length, bytes, length);
    out->length += length;
    return 1;
}

static mulib_mune_status_t mune_contract_native(mulib_mune_state_t *state,
    const mulib_mune_value_t *args, size_t count, mulib_mune_result_t *out, void *user)
{
    int64_t *base = (int64_t *)user;
    if (count != 1 || args[0].type != MULIB_MUNE_INTEGER) return MULIB_MUNE_INVALID;
    /* The closure owner and callback arguments must survive explicit GC. */
    (void)mulib_mune_collect(state);
    return mulib_mune_result_push(out, mulib_mune_integer(*base + args[0].as.integer))
        ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}

int64_t on1x_mune_contract_semantics(void)
{
    static const char lua[] =
        "assert(_VERSION=='Lua 5.4' and #package.searchers==4); "
        "local r=debug.getregistry(); assert(r._LOADED==package.loaded and r._PRELOAD==package.preload); "
        "local p=package; package=nil; assert(require('math')==math); package=p; "
        "assert(math.type(math.abs('3'))=='float'); math.randomseed(1,2); "
        "assert(math.random(0)==8291693048688576641); "
        "local s=utf8.char(0x7ff,0x10ffff); assert(#s==6 and utf8.len(s)==2); "
        "assert(not pcall(function() for p,c in utf8.codes('a\\128') do end end)); "
        "assert((')'):find(')')==1 and ('a'):match('()%1')==nil); "
        "assert(string.gsub('abc','(','x')=='xaxbxcx'); "
        "assert(string.format('%p',nil)=='(null)'); "
        "assert(os.date('!%Y-%m-%d',0)=='1970-01-01'); "
        "local f=assert(load('return function(a) return a end','=contract')); "
        "local g=assert(load(string.dump(f(),true))); assert(g(42)==42 and debug.getlocal(g,1)==nil); "
        "local co=coroutine.wrap(function() coroutine.yield(native_sum(2)); return 7 end); "
        "assert(co()==42 and co()==7); "
        "assert(require('contract_bundle').answer==42); "
        "proxy=setmetatable({}, {__index=function(_,k) if k=='bad' then error('bad',0) end return 42 end}); "
        "local x=42; return native_sum(2),'a\\0b',function() return x end";
    static const char module[] = "return {answer=42}";
    mulib_mune_bundle_module_t item = {"contract_bundle", module, sizeof(module)-1};
    mulib_mune_error_t error;
    mulib_mune_t *legacy = mulib_mune_create();
    double number = -1;
    int mode;
    assert(legacy && mulib_mune_version() == 1);
    assert(mulib_mune_eval(legacy, "x=42;return x", 13, &number, &error) == MULIB_MUNE_OK && number == 42);
    number = -1;
    assert(mulib_mune_eval(legacy, "x=1;return missing", 18, &number, &error) != MULIB_MUNE_OK && number == -1);
    assert(mulib_mune_get(legacy, "x", &number) && number == 42);
    mulib_mune_destroy(legacy);
    for (mode = MULIB_MUNE_TREE; mode <= MULIB_MUNE_JIT; ++mode) {
        struct mune_contract_bytes bytes = {{0},0};
        mulib_mune_state_t *state, *other;
        mulib_mune_chunk_t *chunk;
        mulib_mune_result_t *roots, *result = NULL, *called = NULL;
        mulib_mune_value_t owner, function, value, table, key, found, proxy;
        void *data;
        size_t length, cursor = 0;
        const char *text;
        if (mode == MULIB_MUNE_JIT && !mulib_jit_supported()) continue;
        state = mulib_mune_state_create(NULL); other = mulib_mune_state_create(NULL);
        assert(state && other && mulib_mune_set_mode(state, (mulib_mune_mode_t)mode) == MULIB_MUNE_OK);
        roots = mulib_mune_result_create(state); assert(roots);
        assert(mulib_mune_make_userdata(state, sizeof(int64_t), &owner, &data) == MULIB_MUNE_OK);
        assert(*(int64_t *)data == 0); *(int64_t *)data = 40;
        assert(mulib_mune_value_userdata(owner, &length) == data && length == sizeof(int64_t));
        assert(mulib_mune_make_function(state, mune_contract_native, data, &function) == MULIB_MUNE_OK);
        assert(mulib_mune_function_owner(state, function, owner) == MULIB_MUNE_OK);
        assert(mulib_mune_result_push(roots, function));
        assert(mulib_mune_set_global(state, "native_sum", function) == MULIB_MUNE_OK);
        assert(mulib_mune_set_global(other, "foreign", function) == MULIB_MUNE_INVALID);
        assert(mulib_mune_set_uservalue(state, owner, 1, mulib_mune_integer(42)) == MULIB_MUNE_OK);
        assert(mulib_mune_get_uservalue(state, owner, 1, &value) == MULIB_MUNE_OK && value.as.integer == 42);
        assert(mulib_mune_make_table(state, &table) == MULIB_MUNE_OK && mulib_mune_result_push(roots, table));
        assert(mulib_mune_raw_set(state, table, mulib_mune_integer(1), mulib_mune_integer(42)) == MULIB_MUNE_OK);
        assert(mulib_mune_table_next(state, table, &cursor, &key, &found) == 1);
        assert(key.type == MULIB_MUNE_INTEGER && found.as.integer == 42);
        assert(mulib_mune_table_next(state, table, &cursor, &key, &found) == 0);
        value = mulib_mune_integer(99);
        assert(mulib_mune_make_string(state, NULL, 1, &value) == MULIB_MUNE_INVALID && value.as.integer == 99);
        assert(mulib_mune_bundle_write(&item, 1, mune_contract_write, &bytes, &error) == MULIB_MUNE_OK);
        assert(mulib_mune_load_bundle(state, bytes.data, bytes.length-1, &error) != MULIB_MUNE_OK);
        assert(mulib_mune_load_bundle(state, bytes.data, bytes.length, &error) == MULIB_MUNE_OK);
        chunk = mulib_mune_parse(lua, sizeof(lua)-1, NULL, &error); assert(chunk && mulib_mune_chunk_ast(chunk));
        assert(mulib_mune_run(state, chunk, &result, &error) == MULIB_MUNE_OK);
        mulib_mune_chunk_destroy(chunk); /* Returned closure must retain its AST. */
        assert(mulib_mune_result_count(result) == 3);
        value = mulib_mune_result_get(result, 0); assert(value.type == MULIB_MUNE_INTEGER && value.as.integer == 42);
        value = mulib_mune_result_get(result, 1); text = mulib_mune_value_string(value, &length);
        assert(text && length == 3 && !memcmp(text, "a\0b", 3));
        (void)mulib_mune_collect(state);
        function = mulib_mune_result_get(result, 2);
        assert(mulib_mune_call(state, function, NULL, 0, &called, &error) == MULIB_MUNE_OK);
        assert(mulib_mune_result_get(called, 0).as.integer == 42); mulib_mune_result_destroy(called);
        assert(mulib_mune_get_global(state, "proxy", &proxy) == MULIB_MUNE_OK);
        assert(mulib_mune_get_field(state, proxy, "ok", &value) == MULIB_MUNE_OK && value.as.integer == 42);
        value = mulib_mune_integer(99);
        assert(mulib_mune_get_field(state, proxy, "bad", &value) == MULIB_MUNE_RUNTIME && value.as.integer == 99);
        mulib_mune_result_destroy(result); mulib_mune_result_destroy(roots);
        mulib_mune_state_destroy(other); mulib_mune_state_destroy(state);
    }
    return 1;
}
