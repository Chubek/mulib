/* Device-free retained damage/scroll/swapchain regressions with fault injection. */
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long fail_after=-1;
static void *fault_malloc(size_t n)
{
    if (fail_after==0) { fail_after=-1; return NULL; }
    if (fail_after>0) fail_after--;
    return malloc(n);
}
static void *fault_calloc(size_t n,size_t z)
{
    if (fail_after==0) { fail_after=-1; return NULL; }
    if (fail_after>0) fail_after--;
    return calloc(n,z);
}
static void *fault_realloc(void *p,size_t n)
{
    if (fail_after==0) { fail_after=-1; return NULL; }
    if (fail_after>0) fail_after--;
    return realloc(p,n);
}
#define malloc fault_malloc
#define calloc fault_calloc
#define realloc fault_realloc
#include "mulib-retgtk-window.h"
#include "mulib-retgtk-invalidate.h"
#include "mulib-retgtk-scroll.h"
#include "mulib-retgtk-box.h"
#include "mulib-retgtk-grid.h"
#undef malloc
#undef calloc
#undef realloc

typedef struct {
    mgx_rect rect;
    uint32_t color;
    unsigned paints,arranges,destroyed;
    int invalidate,remove,replace;
    int mutate_arrange;
    mulib_retgtk_widget_t *victim;
    mulib_retgtk_widget_t *append_arrange;
    mulib_retgtk_scene_t *scene;
    mulib_retgtk_widget_t *replacement;
} leaf_state;
static void leaf_destroy(mulib_retgtk_widget_t *w)
{
    ((leaf_state *)w->data)->destroyed++;
}
static void leaf_paint(mulib_retgtk_widget_t *w,mret_paint *p)
{
    leaf_state *state=w->data;
    state->paints++;
    mgx_fill(p->surface,w->bounds,state->color);
    if (state->invalidate) {
        state->invalidate=0;
        state->color=0xff00ff00;
        mulib_retgtk_widget_invalidate(w);
        assert(mulib_retgtk_scene_repaint(state->scene,p,state->scene->bounds)==-1);
    }
    if (state->replace) {
        state->replace=0;
        mulib_retgtk_scene_set(state->scene,state->replacement);
    }
    if (state->remove) {
        state->remove=0;
        assert(mulib_retgtk_widget_remove(w->parent,w));
        assert(!state->destroyed); /* Traversal owns its callback reference. */
    }
}
static void leaf_arrange(mulib_retgtk_widget_t *w,mgx_rect r)
{
    (void)r;
    leaf_state *state=w->data;
    state->arranges++;
    if (state->append_arrange) {
        assert(mulib_retgtk_widget_add(w->parent,state->append_arrange));
        state->append_arrange=NULL;
    }
    if (state->mutate_arrange) {
        state->mutate_arrange=0;
        mulib_retgtk_widget_t *parent=w->parent;
        assert(mulib_retgtk_widget_remove(parent,state->victim));
        assert(mulib_retgtk_widget_remove(parent,w));
        assert(!state->destroyed);
    }
}
static const mulib_retgtk_widget_ops leaf_ops={
    leaf_destroy,NULL,leaf_arrange,leaf_paint,NULL
};
static mulib_retgtk_widget_t *make_leaf(uint64_t id,leaf_state *state)
{
    mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&leaf_ops);
    assert(w);
    w->data=state;
    return w;
}
static void fixed_arrange(mulib_retgtk_widget_t *w,mgx_rect r)
{
    (void)r;
    size_t n=0;
    mulib_retgtk_widget_t **v=mulib_retgtk_widget_children_snapshot(w,&n);
    if (w->n&&!v) mulib_retgtk_widget_layout_retry(w);
    for (size_t i=0;i<n;i++)
        if (v[i]->parent==w && v[i]->visible)
            mulib_retgtk_widget_arrange(v[i],((leaf_state *)v[i]->data)->rect);
    mulib_retgtk_widget_children_release(v,n);
}
static const mulib_retgtk_widget_ops fixed_ops={NULL,NULL,fixed_arrange,NULL,NULL};
static mulib_retgtk_widget_t *make_root(void)
{
    mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(1,&fixed_ops);
    assert(w); return w;
}
static void adopt(mulib_retgtk_widget_t *parent,mulib_retgtk_widget_t *child)
{
    assert(mulib_retgtk_widget_add(parent,child));
    mulib_retgtk_widget_unref(child);
}
static uint32_t pixel(const mgx_surface *s,int x,int y)
{
    return s->pixels[(size_t)y*s->stride+(size_t)x];
}
static void assert_same(const mgx_surface *a,const mgx_surface *b)
{
    assert(a->w==b->w && a->h==b->h);
    for (int y=0;y<a->h;y++)
        assert(!memcmp(a->pixels+(size_t)y*a->stride,
                       b->pixels+(size_t)y*b->stride,(size_t)a->w*4));
}
static void test_damage(void)
{
    mgx_surface target={0},reference={0};
    assert(mgx_surface_init(&target,96,64) && mgx_surface_init(&reference,96,64));
    mret_paint p={&target,NULL,mgui_default_theme()};
    p.theme.background=0x40000000; /* SOURCE clear must handle nonopaque themes. */
    mulib_retgtk_scene_t scene={0};
    mulib_retgtk_widget_t *root=make_root();
    leaf_state a={.rect={4,4,16,16},.color=0xffff0000};
    leaf_state b={.rect={70,4,16,16},.color=0xff0000ff};
    leaf_state overlay={.rect={12,4,16,16},.color=0x808080ff};
    mulib_retgtk_widget_t *wa=make_leaf(2,&a),*wb=make_leaf(3,&b);
    adopt(root,wa); adopt(root,wb); adopt(root,make_leaf(4,&overlay));
    mulib_retgtk_scene_set(&scene,root); mulib_retgtk_widget_unref(root);
    mgx_rect bounds={0,0,96,64};
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(a.paints==1 && b.paints==1 && overlay.paints==1);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==0);
    assert(a.arranges==1 && b.arranges==1);
    a.color=0xff00ff00; mulib_retgtk_widget_invalidate(wa);
    mulib_glx_region_t damage={0};
    fail_after=0;
    assert(!mulib_retgtk_invalidate_collect(root,&damage));
    assert(root->damage_pending); /* Collection never consumes damage on OOM. */
    assert(mulib_retgtk_invalidate_collect(root,&damage) && damage.n==1);
    assert(damage.v[0].x==4 && damage.v[0].w==16);
    mulib_glx_region_free(&damage);
    uint32_t far=pixel(&target,80,10);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(a.paints==2 && b.paints==1 && overlay.paints==2);
    assert(pixel(&target,80,10)==far && pixel(&target,5,5)==a.color);
    /* Compare against a complete recomposition, including translucent overlap. */
    mret_paint ref={&reference,NULL,p.theme};
    mgx_clear(&reference,p.theme.background);
    mulib_retgtk_widget_paint(root,&ref);
    assert_same(&target,&reference);
    for (unsigned i=0;i<4;i++) {
        mulib_retgtk_widget_invalidate(wa);
        assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
        assert_same(&target,&reference);
    }
    /* Move/shrink erases old pixels; unaffected far widget does not repaint. */
    unsigned old_b=b.paints;
    a.rect=(mgx_rect){35,32,8,8};
    mulib_retgtk_widget_invalidate(wa);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(pixel(&target,5,5)==p.theme.background && pixel(&target,36,33)==a.color);
    assert(b.paints==old_b);
    mulib_retgtk_widget_set_visible(wa,0);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(pixel(&target,36,33)==p.theme.background);
    mulib_retgtk_widget_set_visible(wa,1);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(pixel(&target,36,33)==a.color);
    a.rect=(mgx_rect){200,200,8,8}; mulib_retgtk_widget_invalidate(wa);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    unsigned offscreen=a.paints;
    mulib_retgtk_widget_invalidate(wa);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==0 && a.paints==offscreen);
    p.theme.background=0xff121212;
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(pixel(&target,0,0)==p.theme.background);
    mulib_retgtk_scene_invalidate(&scene);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    target.clip=(mgx_rect){0,0,4,4};
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==-1);
    target.clip=bounds;
    mulib_retgtk_scene_set(&scene,NULL);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(pixel(&target,75,10)==p.theme.background);
    assert(a.destroyed==1 && b.destroyed==1 && overlay.destroyed==1);
    mgx_surface_free(&target); mgx_surface_free(&reference);
}
static void test_callback_lifetime(void)
{
    mgx_surface target={0};
    assert(mgx_surface_init(&target,64,64));
    mret_paint p={&target,NULL,mgui_default_theme()};
    mulib_retgtk_scene_t scene={0};
    leaf_state state={.rect={0,0,32,32},.color=0xffff0000,.invalidate=1,.scene=&scene};
    mulib_retgtk_widget_t *root=make_root(),*w=make_leaf(2,&state);
    adopt(root,w); mulib_retgtk_scene_set(&scene,root);
    mulib_retgtk_widget_unref(root);
    mgx_rect bounds={0,0,64,64};
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(pixel(&target,10,10)==0xffff0000 && root->damage_pending);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(pixel(&target,10,10)==0xff00ff00);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==0);
    state.remove=1; mulib_retgtk_widget_invalidate(w);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1 && state.destroyed==1);
    assert(root->damage_pending);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(pixel(&target,10,10)==p.theme.background);
    mulib_retgtk_scene_set(&scene,NULL);
    leaf_state replacement={.color=0xff0000ff};
    mulib_retgtk_widget_t *next=make_leaf(7,&replacement);
    state=(leaf_state){.color=0xffff0000,.replace=1,.scene=&scene,.replacement=next};
    w=make_leaf(6,&state);
    mulib_retgtk_scene_set(&scene,w); mulib_retgtk_widget_unref(w);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1 && state.destroyed==1);
    assert(scene.root==next);
    mulib_retgtk_widget_unref(next);
    assert(mulib_retgtk_scene_repaint(&scene,&p,bounds)==1);
    assert(pixel(&target,10,10)==replacement.color);
    mulib_retgtk_scene_set(&scene,NULL);
    assert(replacement.destroyed==1);
    mgx_surface_free(&target);
}
static void test_layout_callback_retry(void)
{
    mgx_surface target={0};
    assert(mgx_surface_init(&target,64,64));
    mret_paint p={&target,NULL,mgui_default_theme()};
    mulib_retgtk_scene_t scene={0};
    leaf_state a={.rect={0,0,16,16},.color=0xffff0000};
    leaf_state b={.rect={32,32,16,16},.color=0xff0000ff};
    mulib_retgtk_widget_t *root=make_root(),*added=make_leaf(3,&b);
    a.append_arrange=added;
    adopt(root,make_leaf(2,&a));
    mulib_retgtk_scene_set(&scene,root); mulib_retgtk_widget_unref(root);
    assert(mulib_retgtk_scene_repaint(&scene,&p,(mgx_rect){0,0,64,64})==1);
    assert(root->layout_retry && !b.arranges && !b.paints);
    mulib_retgtk_widget_unref(added);
    assert(mulib_retgtk_scene_repaint(&scene,&p,(mgx_rect){0,0,64,64})==1);
    assert(b.arranges==1 && b.paints==1 && pixel(&target,40,40)==b.color);
    assert(mulib_retgtk_scene_repaint(&scene,&p,(mgx_rect){0,0,64,64})==0);
    mulib_retgtk_scene_set(&scene,NULL);
    assert(a.destroyed==1 && b.destroyed==1);
    mgx_surface_free(&target);
}
static void test_scroll(void)
{
    mulib_retgtk_widget_t *scroll=mulib_retgtk_scroll(1,200);
    leaf_state state={.color=0xff123456};
    mulib_retgtk_widget_t *child=make_leaf(2,&state);
    assert(scroll); adopt(scroll,child);
    mulib_retgtk_widget_arrange(scroll,(mgx_rect){10,20,80,50});
    assert(mulib_retgtk_scroll_to(scroll,75));
    assert(child->bounds.y==-55 && child->bounds.h==200);
    assert(!mulib_retgtk_scroll_to(scroll,NAN));
    assert(!mulib_retgtk_scroll_to(scroll,INFINITY));
    mgx_event e={0}; e.type=MGX_EVENT_SCROLL; e.dy=-DBL_MAX;
    assert(mret_scroll_event(scroll,&e) && scroll->value==150);
    assert(!mret_scroll_event(scroll,&e)); /* Bubble at boundary. */
    e.dy=NAN; assert(!mret_scroll_event(scroll,&e));
    e.dy=1; mulib_retgtk_widget_set_enabled(scroll,0);
    assert(!mret_scroll_event(scroll,&e));
    mulib_retgtk_widget_set_enabled(scroll,1);
    assert(mulib_retgtk_scroll_content(scroll,30));
    assert(scroll->value==0 && child->bounds.y==20 && child->bounds.h==50);
    assert(!mulib_retgtk_scroll_content(scroll,-1));
    assert(!mulib_retgtk_scroll_to(child,1));
    mulib_retgtk_widget_unref(scroll); assert(state.destroyed==1);
    scroll=mulib_retgtk_scroll(3,200); assert(scroll);
    state=(leaf_state){0};
    leaf_state victim={0};
    child=make_leaf(4,&state);
    state.victim=make_leaf(5,&victim);
    adopt(scroll,child); adopt(scroll,state.victim);
    mulib_retgtk_widget_arrange(scroll,(mgx_rect){0,0,50,50});
    state.mutate_arrange=1;
    assert(mulib_retgtk_scroll_to(scroll,1));
    assert(!scroll->n && state.destroyed==1 && victim.destroyed==1);
    assert(victim.arranges==1); /* Removed snapshot child is not visited again. */
    mulib_retgtk_widget_unref(scroll);
}
static uint32_t random_state=0x29a3b9d4u;
static unsigned random_next(void)
{
    random_state^=random_state<<13;
    random_state^=random_state>>17;
    random_state^=random_state<<5;
    return random_state;
}
static void test_random_repaint(void)
{
    mgx_surface target={0},reference={0};
    assert(mgx_surface_init(&target,80,60) && mgx_surface_init(&reference,80,60));
    mret_paint p={&target,NULL,mgui_default_theme()},ref={&reference,NULL,p.theme};
    mulib_retgtk_scene_t scene={0};
    mulib_retgtk_widget_t *root=make_root(),*leaves[12];
    leaf_state states[12]={0};
    for (unsigned i=0;i<12;i++) {
        states[i].rect=(mgx_rect){(int)(i*5),(int)(i*3),20,20};
        states[i].color=0x80000000|random_next();
        leaves[i]=make_leaf(i+2,&states[i]); adopt(root,leaves[i]);
    }
    mulib_retgtk_scene_set(&scene,root); mulib_retgtk_widget_unref(root);
    for (unsigned frame=0;frame<500;frame++) {
        unsigned at=random_next()%12;
        switch (random_next()%3) {
        case 0:
            states[at].rect=(mgx_rect){(int)(random_next()%150)-30,
                (int)(random_next()%130)-30,(int)(random_next()%40),(int)(random_next()%40)};
            mulib_retgtk_widget_invalidate(leaves[at]);
            break;
        case 1:
            states[at].color=random_next();
            mulib_retgtk_widget_invalidate(leaves[at]);
            break;
        default:
            mulib_retgtk_widget_set_visible(leaves[at],!leaves[at]->visible);
            break;
        }
        assert(mulib_retgtk_scene_repaint(&scene,&p,(mgx_rect){0,0,80,60})>=0);
        mgx_clear(&reference,p.theme.background);
        mulib_retgtk_widget_paint(root,&ref);
        assert_same(&target,&reference);
    }
    mulib_retgtk_scene_set(&scene,NULL);
    for (unsigned i=0;i<12;i++) assert(states[i].destroyed==1);
    mgx_surface_free(&target); mgx_surface_free(&reference);
}
static void test_retry(void)
{
    for (int mode=0;mode<4;mode++) for (long failure=0;failure<8;failure++) {
        mgx_surface target={0};
        assert(mgx_surface_init(&target,64,64));
        mret_paint p={&target,NULL,mgui_default_theme()};
        mulib_retgtk_scene_t scene={0};
        mulib_retgtk_widget_t *root=mode==0?mulib_retgtk_widget_create(1,NULL):
            mode==1?mulib_retgtk_box(1,1,0,0):mode==2?mulib_retgtk_grid(1,1,0):
            mulib_retgtk_constraint_box(1,(mulib_gui_constraints_t){0,0,64,64});
        leaf_state state={.color=0xff112233};
        assert(root); adopt(root,make_leaf(2,&state));
        mulib_retgtk_scene_set(&scene,root); mulib_retgtk_widget_unref(root);
        fail_after=failure;
        assert(mulib_retgtk_scene_repaint(&scene,&p,(mgx_rect){0,0,64,64})==1);
        fail_after=-1;
        /* One failed allocation can defer layout or paint but never loses work. */
        for (unsigned retry=0;retry<3;retry++)
            (void)mulib_retgtk_scene_repaint(&scene,&p,(mgx_rect){0,0,64,64});
        assert(pixel(&target,10,10)==state.color && !root->layout_retry);
        assert(mulib_retgtk_scene_repaint(&scene,&p,(mgx_rect){0,0,64,64})==0);
        mulib_retgtk_scene_set(&scene,NULL);
        assert(state.destroyed==1);
        mgx_surface_free(&target);
    }
}
static int reject_present(void *u,const mgx_surface *s,const mgx_rect *r,size_t n)
{
    (void)u; (void)s; (void)r; (void)n; return 0;
}
static int accept_present(void *u,const mgx_surface *s,const mgx_rect *r,size_t n)
{
    (void)u; (void)s; (void)r; (void)n; return 1;
}
static void test_window(void)
{
    mulib_glx_platform_t platform={0};
    mulib_glx_platform_headless(&platform);
    mulib_retgtk_window_t window={0};
    assert(mulib_retgtk_window_init(&window,&platform,1,"retained",48,32));
    leaf_state state={.color=0xffabcdef};
    mulib_retgtk_widget_t *w=make_leaf(1,&state);
    mulib_retgtk_scene_set(&window.scene,w); mulib_retgtk_widget_unref(w);
    fail_after=0;
    assert(!mulib_retgtk_window_draw(&window) && !window.window.swap.acquired);
    assert(mulib_retgtk_window_draw(&window));
    assert(state.paints==1);
    for (unsigned i=0;i<5;i++) {
        assert(mulib_retgtk_window_draw(&window));
        assert(state.paints==1);
        assert_same(&window.retained,&window.window.swap.image[window.window.swap.front]);
    }
    platform.ops.present=reject_present;
    state.color=0xff001122; mulib_retgtk_widget_invalidate(window.scene.root);
    assert(!mulib_retgtk_window_draw(&window) && !window.window.swap.acquired);
    platform.ops.present=NULL;
    assert(mulib_retgtk_window_draw(&window) && state.paints==2);
    mgx_event resize={0}; resize.type=MGX_EVENT_RESIZE; resize.x=32; resize.y=24;
    assert(mulib_retgtk_window_event(&window,&resize));
    fail_after=0;
    assert(!mulib_retgtk_window_draw(&window) && !window.window.swap.acquired);
    assert(window.retained.w==48); /* Old retained allocation survives failure. */
    assert(mulib_retgtk_window_draw(&window) && window.retained.w==32);
    assert(state.paints==3);
    platform.ops.present=accept_present;
    platform.capabilities|=MGX_CAP_ASYNC_PRESENT;
    assert(mulib_retgtk_window_draw(&window) && window.window.present.pending);
    unsigned front=window.window.swap.front;
    uint32_t old=pixel(&window.window.swap.image[front],1,1);
    state.color=0xff334455; mulib_retgtk_widget_invalidate(window.scene.root);
    assert(!mulib_retgtk_window_draw(&window) && !window.window.swap.acquired);
    assert(pixel(&window.window.swap.image[front],1,1)==old);
    assert(mulib_glx_window_complete(&window.window,window.window.present.serial));
    assert(mulib_retgtk_window_draw(&window));
    assert(pixel(&window.window.swap.image[window.window.swap.front],1,1)==state.color);
    assert(mulib_glx_window_complete(&window.window,window.window.present.serial));
    mulib_retgtk_window_free(&window);
    assert(state.destroyed==1);
}
int main(void)
{
    test_damage(); test_callback_lifetime(); test_layout_callback_retry();
    test_scroll(); test_random_repaint();
    test_retry(); test_window();
    puts("RetGTK retained damage/scroll follow-up passed");
    return 0;
}
