/* From the source root, build and run the standalone real-Mune test:
 * cc -std=c11 -O2 -Wall -Wextra -pedantic -Iinclude tests/test_imtui_mune.c \
 *    -o /tmp/opencode/test_imtui_mune -lm -pthread -ldl
 * /tmp/opencode/test_imtui_mune [source-root]
 * The switches below are the complete implementation-TU recipe. */
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
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_EDBUF
#define MULIB_IMPL_TERMSYN
#define MULIB_IMPL_IMTUI_CORE
#define MULIB_IMPL_IMTUI_DRAW
#define MULIB_IMPL_IMTUI_LAYOUT
#define MULIB_IMPL_IMTUI_WIDGETS
#define MULIB_IMPL_IMTUI_SYNTAX
#define MULIB_IMPL_IMTUI_MUNE
#include "mulib-imtui-mune.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static mulib_term_cell_t cell(const mulib_imtui_t *ctx, size_t x, size_t y) {
    return mulib_imtui_cells(ctx)[y * (size_t)mulib_imtui_bounds(ctx).w + x];
}

static void test_syntax(void) {
    static const char source[] = "/* comment\r\n"
        "still a comment */\n"
        "\t\xc3\xa9 \x1b\xff!\n"
        "int Z\n";
    mulib_imtui_t ctx;
    mulib_edbuf_t *buf = mulib_edbuf_create(MULIB_EDBUF_PIECE_TABLE, source, sizeof source - 1);
    mulib_termsyn_t *syntax = mulib_termsyn_create(MULIB_TERMSYN_C);
    mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT];
    const mulib_imtui_rect_t rect = {-1, 1, 10, 3};
    assert(buf && syntax && mulib_imtui_init(&ctx, 12, 5));
    assert(mulib_imtui_syntax_draw(&ctx, rect, buf, syntax, 1, NULL) == MULIB_TERMSYN_EINVAL);
    assert(mulib_imtui_begin(&ctx, NULL, 0));
    assert(mulib_imtui_clip_push(&ctx, (mulib_imtui_rect_t){0, 0, 7, 5}));
    assert(mulib_imtui_syntax_draw(&ctx, rect, buf, syntax, 1, NULL) == MULIB_TERMSYN_OK);
    mulib_termsyn_default_theme(styles);
    assert(cell(&ctx, 0, 1).codepoint == 't');
    assert(cell(&ctx, 0, 1).style.foreground == styles[MULIB_TERMSYN_COMMENT].foreground);
    assert(cell(&ctx, 0, 2).codepoint == ' ' && cell(&ctx, 3, 2).codepoint == 0xe9);
    assert(cell(&ctx, 5, 2).codepoint == 0xfffd && cell(&ctx, 6, 2).codepoint == 0xfffd);
    assert(cell(&ctx, 7, 2).codepoint == ' '); /* clipped '!' */
    assert(cell(&ctx, 0, 3).style.foreground == styles[MULIB_TERMSYN_TYPE].foreground);
    assert(mulib_imtui_syntax_draw(&ctx, rect, buf, syntax, 1, styles) == MULIB_TERMSYN_OK);
    assert(mulib_termsyn_lexed_lines(syntax) == 0);
    assert(mulib_edbuf_replace(buf, 0, 2, "//", 2) == MULIB_EDBUF_OK);
    assert(mulib_imtui_syntax_draw(&ctx, rect, buf, syntax, 1, styles) == MULIB_TERMSYN_OK);
    assert(mulib_termsyn_lexed_lines(syntax) == 2);
    assert(cell(&ctx, 0, 1).style.foreground == styles[MULIB_TERMSYN_IDENTIFIER].foreground);
    assert(mulib_imtui_syntax_draw(&ctx, rect, buf, syntax, SIZE_MAX, styles) == MULIB_TERMSYN_OK);
    mulib_imtui_end(&ctx);
    mulib_termsyn_destroy(syntax);
    mulib_edbuf_destroy(buf);
    mulib_imtui_free(&ctx);
}

static void test_syntax_boundary(void) {
    char source[517];
    mulib_imtui_t ctx;
    mulib_edbuf_t *buf;
    mulib_termsyn_t *syntax = mulib_termsyn_create(MULIB_TERMSYN_PLAIN);
    const mulib_imtui_rect_t r = {-510, -1, 516, 3};
    memset(source, 'a', sizeof source);
    source[0] = '\n'; /* This line is clipped above the canvas. */
    source[510] = 'Z';
    memcpy(source + 511, "\xf0\x9f\x99\x82\r\n", 6); /* Refill within the first scalar. */
    buf = mulib_edbuf_create(MULIB_EDBUF_ROPE, source, sizeof source);
    assert(buf && syntax && mulib_imtui_init(&ctx, 6, 2));
    assert(mulib_imtui_begin(&ctx, NULL, 0));
    assert(mulib_imtui_syntax_draw(&ctx, r, buf, syntax, 0, NULL) == MULIB_TERMSYN_OK);
    assert(cell(&ctx, 0, 0).codepoint == 0x1f642);
    assert(cell(&ctx, 1, 0).codepoint == ' ' && cell(&ctx, 0, 1).codepoint == ' ');
    mulib_imtui_end(&ctx);
    mulib_termsyn_destroy(syntax);
    mulib_edbuf_destroy(buf);
    mulib_imtui_free(&ctx);
}

static const char setup[] =
    "local lib = imtui_test_root .. '/library'\n"
    "package.path = lib .. '/?.lua;' .. lib .. '/?/init.lua'\n"
    "local ui = require('imtui')\n"
    "assert(require('imtui.widgets') == ui and require('imtui.code') == ui)\n"
    "assert(type(require('imtui.demo')) == 'function')\n"
    "outside = imtui_native.bounds\n"
    "assert(not pcall(outside))\n"
    "return function(expect_click, expect_check, expect_choice)\n"
    "  local b = ui.bounds()\n"
    "  assert(b.x == 0 and b.y == 0 and b.w == 18 and b.h == 7)\n"
    "  local layout = ui.rect(0, 0, b.w, 3)\n"
    "  local a = ui.column(layout, 1, 3)\n"
    "  local left, right = ui.row(a, 1, 2, 2), ui.row(a, 2, 2, 2)\n"
    "  assert(left.x == 0 and left.w == 8 and right.x == 10 and right.w == 8)\n"
    "  assert(ui.button('go\\0key', left, 'Go') == expect_click)\n"
    "  local checked, changed = ui.checkbox('show', right, 'Show', false)\n"
    "  assert(checked == expect_check and changed == expect_check)\n"
    "  local index, cycled = ui.choice('choice', ui.column(layout, 2, 3), {'One', 'Two'}, 1)\n"
    "  assert(index == (expect_choice and 2 or 1) and cycled == expect_choice)\n"
    "  ui.rule(ui.column(layout, 3, 3), { foreground = 0x01112233 })\n"
    "  ui.code(ui.rect(1, 3, 10, 3), 'c', '/* first\\nsecond */\\nint x;\\n', 2)\n"
    "  ui.text(1, 6, 'A\\x00B\\x1bC', { attributes = 1 })\n"
    "  assert(not pcall(imtui_native.row, 0, 0, 4, 4, 0, 1, 2))\n"
    "  assert(not pcall(imtui_native.column, 2147483647, 0, 4, 4, 0, 1, 1))\n"
    "  assert(not pcall(imtui_native.text, 0/0, 0, 'x'))\n"
    "  assert(not pcall(imtui_native.text, 0, 0, '\\xff'))\n"
    "  assert(not pcall(imtui_native.text, 0, 0, 'x', 0x02000000))\n"
    "  assert(not pcall(imtui_native.button, 'id', 0, 0, 1, 1, 'x', 0))\n"
    "  assert(not pcall(imtui_native.checkbox, 'id', 0, 0, 1, 1, 'x', 0))\n"
    "  assert(not pcall(imtui_native.code, 0, 0, 1, 1, 'c\\0', 'x'))\n"
    "  assert(not pcall(imtui_native.code, 0, 0, 1, 1, 'c', 'x', 0))\n"
    "  assert(not ui.button('zero', ui.rect(0, 0, 0, 0), 'zero'))\n"
    "  assert(not ui.button('disabled', ui.rect(12, 4, 6, 1), 'No', false))\n"
    "  return checked\n"
    "end\n";

static void test_bridge(const char *source_root, mulib_mune_mode_t mode) {
    static const char workload[] =
        "local s = string.rep('x', 4096); for i=1,33 do imtui_native.text(0,999,s) end";
    static const char clipped[] =
        "imtui_native.text(0, 0, 'ABCDEF'); imtui_native.code(0, 1, 6, 1, 'plain', 'GHIJKL')";
    static const char dense[] =
        "imtui_native.code(0,0,1024,1,'c',string.rep('int+1 ',10000))";
    mulib_mune_state_t *vm = mulib_mune_state_create(NULL);
    mulib_imtui_mune_t bridge = {0};
    mulib_imtui_t ctx;
    mulib_mune_result_t *retained = NULL, *result = NULL;
    mulib_mune_value_t path, args[3], frame, saved;
    mulib_mune_error_t error = {0};
    mulib_mune_status_t status;
    mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT];
    assert(vm && mulib_imtui_init(&ctx, 18, 7));
    assert(mulib_mune_set_mode(vm, mode) == MULIB_MUNE_OK);
    assert(mulib_mune_make_string(vm, source_root, strlen(source_root), &path) == MULIB_MUNE_OK);
    assert(mulib_mune_set_global(vm, "imtui_test_root", path) == MULIB_MUNE_OK);
    assert(mulib_mune_set_global(vm, "imtui_native", mulib_mune_boolean(1)) == MULIB_MUNE_OK);
    assert(mulib_imtui_mune_bind(vm, &bridge) == MULIB_MUNE_INVALID);
    assert(mulib_mune_set_global(vm, "imtui_native", mulib_mune_nil()) == MULIB_MUNE_OK);
    assert(mulib_imtui_mune_bind(vm, &bridge) == MULIB_MUNE_OK);
    assert(mulib_imtui_mune_bind(vm, &bridge) == MULIB_MUNE_INVALID);
    assert(mulib_imtui_mune_run(&bridge, &ctx, "", 0, &result, &error) == MULIB_MUNE_INVALID);
    status = mulib_mune_run_source(vm, setup, sizeof setup - 1, &retained, &error);
    if (status) fprintf(stderr, "setup (%d) at %zu: %s\n", mode, error.offset, error.message);
    assert(status == MULIB_MUNE_OK);
    frame = mulib_mune_result_get(retained, 0);
    assert(frame.type == MULIB_MUNE_FUNCTION);
    assert(mulib_mune_get_global(vm, "outside", &saved) == MULIB_MUNE_OK);
    assert(saved.type == MULIB_MUNE_FUNCTION);
    mulib_termsyn_default_theme(styles);
    for (int target = -1; target < 3; ++target) {
        mulib_termkey_event_t events[2] = {{0}, {0}};
        for (size_t i = 0; i < 2; ++i) {
            events[i].type = MULIB_TERMKEY_MOUSE;
            events[i].mouse_action = i ? MULIB_TERMKEY_RELEASE : MULIB_TERMKEY_PRESS;
            events[i].mouse_button = 1;
            events[i].x = target == 1 ? 10u : 1u;
            events[i].y = target == 2 ? 1u : 0u;
        }
        for (int i = 0; i < 3; ++i) args[i] = mulib_mune_boolean(i == target);
        assert(mulib_imtui_begin(&ctx, events, target < 0 ? 0 : 2));
        assert(mulib_mune_call(vm, saved, NULL, 0, &result, NULL) == MULIB_MUNE_RUNTIME);
        status = mulib_imtui_mune_call(&bridge, &ctx, frame, args, 3, &result, &error);
        if (status) fprintf(stderr, "frame (%d) at %zu: %s\n", mode, error.offset, error.message);
        assert(status == MULIB_MUNE_OK);
        assert(mulib_mune_result_get(result, 0).type == MULIB_MUNE_BOOLEAN);
        assert(mulib_mune_result_get(result, 0).as.boolean == (target == 1));
        mulib_mune_result_destroy(result);
        result = NULL;
        assert(cell(&ctx, 0, 0).codepoint == '[' && cell(&ctx, 11, 0).codepoint == (target == 1 ? 'x' : ' '));
        assert(cell(&ctx, 0, 2).style.foreground == UINT32_C(0x01112233));
        assert(cell(&ctx, 1, 3).codepoint == 's');
        assert(cell(&ctx, 1, 3).style.foreground == styles[MULIB_TERMSYN_COMMENT].foreground);
        assert(cell(&ctx, 1, 4).style.foreground == styles[MULIB_TERMSYN_TYPE].foreground);
        assert(cell(&ctx, 2, 6).codepoint == 0xfffd && cell(&ctx, 4, 6).codepoint == 0xfffd);
        assert(cell(&ctx, 3, 6).style.attributes == MULIB_TERM_BOLD);
        assert(ctx.items[0].id == mulib_imtui_id(&ctx, "go\0key", 6) && ctx.item_count == 3);
        mulib_imtui_end(&ctx);
    }
    assert(mulib_imtui_begin(&ctx, NULL, 0));
    assert(mulib_imtui_mune_run(&bridge, &ctx, workload, sizeof workload-1, &result, &error) == MULIB_MUNE_LIMIT);
    assert(!result && !bridge.frame && !bridge.bytes_left);
    assert(mulib_mune_call(vm, saved, NULL, 0, &result, NULL) == MULIB_MUNE_RUNTIME);
    assert(mulib_imtui_clip_push(&ctx, (mulib_imtui_rect_t){2, 0, 2, 2}));
    assert(mulib_imtui_mune_run(&bridge, &ctx, clipped, sizeof clipped-1, &result, &error) == MULIB_MUNE_OK);
    assert(cell(&ctx, 1, 0).codepoint == ' ' && cell(&ctx, 2, 0).codepoint == 'C' && cell(&ctx, 4, 0).codepoint == ' ');
    assert(cell(&ctx, 1, 1).codepoint == ' ' && cell(&ctx, 2, 1).codepoint == 'I' && cell(&ctx, 4, 1).codepoint == ' ');
    assert(mulib_imtui_clip(&ctx).x == 2 && mulib_imtui_clip(&ctx).w == 2);
    mulib_mune_result_destroy(result);
    result = NULL;
    mulib_imtui_clip_pop(&ctx);
    mulib_imtui_end(&ctx);
    assert(mulib_imtui_resize(&ctx, 1024, 1) && mulib_imtui_begin(&ctx, NULL, 0));
    assert(mulib_imtui_mune_run(&bridge, &ctx, dense, sizeof dense-1, &result, &error) == MULIB_MUNE_LIMIT);
    assert(!result && !bridge.frame && !bridge.work_left && cell(&ctx, 0, 0).codepoint == ' ');
    mulib_imtui_end(&ctx);
    mulib_mune_result_destroy(retained);
    mulib_mune_state_destroy(vm);
    mulib_imtui_free(&ctx);
}

int main(int argc, char **argv) {
    const char *source_root = argc > 1 ? argv[1] : ".";
    test_syntax();
    test_syntax_boundary();
    test_bridge(source_root, MULIB_MUNE_TREE);
    test_bridge(source_root, MULIB_MUNE_VM);
    if (mulib_jit_supported()) test_bridge(source_root, MULIB_MUNE_JIT);
    puts("PASS ImTUI syntax and real Mune bindings");
    return 0;
}
