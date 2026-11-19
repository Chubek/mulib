/* Optional actual-Mune integration executable (not a fake VM fixture). */
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
#define MULIB_GFX_ENABLE_MUNE
#include "mulib-immgtk-mune-bridge.h"
#include <assert.h>
#include <stdio.h>
static mulib_mune_status_t run(mulib_mune_state_t *vm, mulib_immgtk_mune_bridge_t *b,
        const char *script) {
    mulib_mune_result_t *result = NULL;
    mulib_mune_error_t error = {0};
    mulib_mune_status_t status = mulib_immgtk_mune_bridge_run(vm, b, script,
        strlen(script), &result, &error);
    mulib_mune_result_destroy(result);
    return status;
}
int main(void) {
    mgx_surface s = {0};
    mulib_glx_font_t font = {0};
    mulib_immgtk_native_t g;
    mulib_immgtk_mune_bridge_t b;
    mulib_mune_state_t *vm = mulib_mune_state_create(NULL);
    assert(vm && mgx_surface_init(&s, 64, 64) && mulib_glx_font_builtin(&font));
    mulib_immgtk_native_init(&g);
    g.font = &font;
    assert(mulib_immgtk_mune_bridge_bind(vm, &b, &g));
    assert(run(vm, &b, "imm_rect(0,0,4,4,4294967295)") == MULIB_MUNE_INVALID);
    mulib_immgtk_native_begin(&g, &s, 10, 10, 0, 0);
    assert(run(vm, &b, "local x,y=imm_pointer(); assert(x==10 and y==10); "
        "local h,a,c,f=imm_interact(1,4,4,24,20); assert(not h and not a and not c and not f); "
        "imm_rect(0,0,4,4,4294967295); imm_text('A',4,4,4294967295); "
        "imm_line(0,30,30,30,2,4294967295)") == MULIB_MUNE_OK);
    assert(run(vm, &b, "imm_clip_push(0,0,1,1); error('test')") == MULIB_MUNE_RUNTIME);
    assert(!b.n && g.clip.w == 64 && g.clip.h == 64);
    assert(run(vm, &b, "imm_clip_pop()") == MULIB_MUNE_RUNTIME);
    size_t count = g.draws.n;
    assert(run(vm, &b, "imm_rect(0,0,4,4,-1)") == MULIB_MUNE_RUNTIME);
    assert(g.draws.n == count);
    mulib_immgtk_native_end(&g);
    mgx_event events[3] = {{0}, {0}, {0}};
    events[0].type = MGX_EVENT_MOTION;
    events[0].x = events[0].y = 10;
    events[1].type = events[2].type = MGX_EVENT_BUTTON;
    events[1].down = 1;
    assert(mulib_immgtk_native_begin_events(&g, &s, events, 3));
    assert(run(vm, &b, "local h,a,c,f=imm_interact(1,4,4,24,20); "
        "assert(h and not a and c and f)") == MULIB_MUNE_OK);
    mulib_immgtk_native_end(&g);
    mulib_immgtk_native_begin(&g, &s, 0, 0, 0, 0);
    mulib_mune_result_t *result = NULL;
    mulib_mune_error_t error = {0};
    const char *unclosed = "imm_clip_push(0,0,1,1)";
    assert(mulib_mune_run_source(vm, unclosed, strlen(unclosed), &result, &error)
        == MULIB_MUNE_OK);
    mulib_mune_result_destroy(result);
    assert(b.n == 1);
    mulib_immgtk_native_end(&g); /* Simulate a caller forgetting direct-call cleanup. */
    s.clip = (mgx_rect){0, 0, 32, 32};
    mulib_immgtk_native_begin(&g, &s, 0, 0, 0, 0);
    mulib_immgtk_mune_bridge_reset(&b);
    assert(!b.n && g.clip.w == 32 && g.clip.h == 32); /* No stale-frame restoration. */
    mulib_immgtk_native_end(&g);
    mulib_mune_state_destroy(vm); /* Destroy callbacks before their borrowed bridge. */
    mulib_immgtk_native_free(&g);
    mulib_glx_font_free(&font);
    mgx_surface_free(&s);
    puts("PASS native ImmGTK real Mune callbacks and error clip cleanup");
    return 0;
}
