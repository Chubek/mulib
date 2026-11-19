#include "mulib-retgtk-event.h"
#include "mulib-retgtk-binding.h"
#include "mulib-retgtk-reactive.h"
#include "mulib-retgtk-mune.h"
#include "gfx-retgtk-core-types.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    mulib_retgtk_signal_t signal;
    uint64_t victim;
    int first,late,victims;
} signal_test;
static void count_cb(void *u,void *sender,unsigned sig,const void *data){
    (void)sender;(void)sig;(void)data;(*(int*)u)++;
}
static void mutate_cb(void *u,void *sender,unsigned sig,const void *data){
    signal_test *t=u;(void)sender;(void)data;
    if(++t->first!=1)return;
    assert(mulib_retgtk_signal_disconnect(&t->signal,t->victim));
    /* Force repeated reallocations, then recurse while the outer iterator lives. */
    for(unsigned i=0;i<80;i++)assert(mulib_retgtk_signal_connect(&t->signal,2,count_cb,&t->late));
    assert(mulib_retgtk_signal_connect(&t->signal,sig,count_cb,&t->late));
    mulib_retgtk_signal_emit(&t->signal,t,sig,NULL);
}
static void close_signal_cb(void *u,void *sender,unsigned sig,const void *data){
    (void)sender;(void)sig;(void)data;mulib_retgtk_signal_free(u);
}
static void test_signal_mutation(void){
    signal_test t={0};
    assert(mulib_retgtk_signal_connect(&t.signal,1,mutate_cb,&t));
    t.victim=mulib_retgtk_signal_connect(&t.signal,1,count_cb,&t.victims);
    mulib_retgtk_signal_emit(&t.signal,&t,1,NULL);
    assert(t.first==2&&t.late==1&&t.victims==0);
    mulib_retgtk_signal_emit(&t.signal,&t,1,NULL);
    assert(t.first==3&&t.late==2&&t.victims==0);
    mulib_retgtk_signal_free(&t.signal);
    assert(mulib_retgtk_signal_connect(&t.signal,1,close_signal_cb,&t.signal));
    assert(mulib_retgtk_signal_connect(&t.signal,1,count_cb,&t.victims));
    mulib_retgtk_signal_emit(&t.signal,&t,1,NULL);
    assert(!t.signal.v&&!t.signal.emitting&&!t.victims);
    mulib_retgtk_signal_free(&t.signal);
}

typedef struct {
    mulib_retgtk_object_t object;
    mulib_retgtk_signal_t signal;
    int *destroyed;
} owned_signal;
static void owned_finalize(mulib_retgtk_object_t *o){
    owned_signal *s=(owned_signal*)o;
    (*s->destroyed)++;mulib_retgtk_signal_free(&s->signal);free(s);
}
static owned_signal *owned_new(int *destroyed){
    owned_signal *s=calloc(1,sizeof *s);assert(s);
    mulib_retgtk_object_init(&s->object,7,owned_finalize);
    assert(mulib_retgtk_signal_bind_owner(&s->signal,&s->object));
    s->destroyed=destroyed;return s;
}
static void drop_sender_cb(void *u,void *sender,unsigned sig,const void *data){
    owned_signal *s=sender;(void)u;(void)sig;(void)data;
    mulib_retgtk_object_unref(&s->object);
    assert(!*s->destroyed); /* Emission owns the final temporary reference. */
}
static void drop_receiver_cb(void *u,void *sender,unsigned sig,const void *data){
    (void)sender;(void)sig;(void)data;
    owned_signal *receiver=u;
    mulib_retgtk_object_unref(&receiver->object);
    assert(!*receiver->destroyed); /* Receiver is pinned across this callback too. */
}
static void test_signal_lifetimes(void){
    int destroyed=0,calls=0;
    owned_signal *s=owned_new(&destroyed);
    assert(mulib_retgtk_signal_connect(&s->signal,1,count_cb,&calls));
    mulib_retgtk_signal_free(&s->signal);
    assert(s->signal.owner==&s->object); /* Subscription reset retains owner pinning. */
    mulib_retgtk_weak_t weak={0};assert(mulib_retgtk_weak_set(&weak,&s->object));
    assert(mulib_retgtk_signal_connect(&s->signal,1,drop_sender_cb,NULL));
    assert(mulib_retgtk_signal_connect(&s->signal,1,count_cb,&calls));
    mulib_retgtk_signal_emit(&s->signal,s,1,NULL);
    assert(destroyed==1&&calls==1&&!weak.object);

    mulib_retgtk_signal_t signal={0};
    destroyed=0;
    owned_signal *receiver=owned_new(&destroyed);
    assert(mulib_retgtk_signal_connect_object(&signal,1,drop_receiver_cb,receiver,&receiver->object));
    assert(mulib_retgtk_signal_connect_object(&signal,1,count_cb,&calls,&receiver->object));
    mulib_retgtk_signal_emit(&signal,NULL,1,NULL);
    assert(destroyed==1&&calls==1&&!signal.n);
    mulib_retgtk_signal_free(&signal);
}

typedef struct {int notifications,depth;char seen[3][16];} property_test;
static void replace_property_cb(void *u,void *sender,unsigned sig,const void *data){
    property_test *t=u;const mret_value *v=data;(void)sig;
    if(!t->depth&&v->kind==MRET_STRING&&!strcmp(v->as.s,"outer")){
        t->depth=1;
        assert(mulib_retgtk_property_set(sender,(mret_value){MRET_STRING,{.s="inner"}}));
        assert(!strcmp(v->as.s,"outer")); /* Not p->value or its freed old string. */
        t->depth=0;
    }
}
static void observe_property_cb(void *u,void *sender,unsigned sig,const void *data){
    property_test *t=u;const mret_value *v=data;(void)sender;(void)sig;
    assert(t->notifications<3);
    snprintf(t->seen[t->notifications++],16,"%s",v->as.s);
}
static void free_property_cb(void *u,void *sender,unsigned sig,const void *data){
    const mret_value *v=data;(void)u;(void)sig;
    mulib_retgtk_property_free(sender);
    assert(!strcmp(v->as.s,"alive in notification"));
}
static void free_binding_cb(void *u,void *sender,unsigned sig,const void *data){
    (void)sender;(void)sig;(void)data;mulib_retgtk_binding_free(u);
}
typedef struct {
    mulib_retgtk_object_t object;
    mulib_retgtk_property_t property;
    int *destroyed;
} owned_property;
static void property_owner_finalize(mulib_retgtk_object_t *o){
    owned_property *p=(owned_property*)o;
    (*p->destroyed)++;mulib_retgtk_property_free(&p->property);free(p);
}
static void drop_property_owner_cb(void *u,void *sender,unsigned sig,const void *data){
    owned_property *p=u;(void)sender;(void)sig;
    mulib_retgtk_object_unref(&p->object);
    assert(!*p->destroyed&&!strcmp(((const mret_value*)data)->as.s,"pinned"));
}
static void test_properties_bindings(void){
    mulib_retgtk_property_t p={0};
    property_test t={0};
    char source[]="original";
    assert(mulib_retgtk_property_set(&p,(mret_value){MRET_STRING,{.s=source}}));
    source[0]='X';assert(!strcmp(p.value.as.s,"original"));
    assert(mulib_retgtk_signal_connect(&p.signals,1,replace_property_cb,&t));
    assert(mulib_retgtk_signal_connect(&p.signals,1,observe_property_cb,&t));
    assert(mulib_retgtk_property_set(&p,(mret_value){MRET_STRING,{.s="outer"}}));
    assert(t.notifications==2&&!strcmp(t.seen[0],"inner")&&!strcmp(t.seen[1],"outer"));
    assert(!strcmp(p.value.as.s,"inner"));
    assert(mulib_retgtk_property_set(&p,p.value));assert(t.notifications==2);
    mulib_retgtk_property_free(&p);mulib_retgtk_property_free(&p);
    assert(!mulib_retgtk_property_set(&p,(mret_value){MRET_INT,{.i=4}}));

    mulib_retgtk_property_t q={0};
    int skipped=0;
    assert(mulib_retgtk_signal_connect(&q.signals,1,free_property_cb,NULL));
    assert(mulib_retgtk_signal_connect(&q.signals,1,count_cb,&skipped));
    assert(mulib_retgtk_property_set(&q,(mret_value){MRET_STRING,{.s="alive in notification"}}));
    assert(q.disposed&&!q.value.as.s&&!skipped&&!q.signals.v);

    mulib_retgtk_property_t a={0},b={0};
    mulib_retgtk_binding_t binding={0};
    assert(mulib_retgtk_property_set(&a,(mret_value){MRET_STRING,{.s="one"}}));
    assert(mulib_retgtk_binding_init(&binding,&a,&b,1));
    assert(!strcmp(b.value.as.s,"one")&&a.value.as.s!=b.value.as.s);
    assert(mulib_retgtk_property_set(&b,(mret_value){MRET_STRING,{.s="two"}}));
    assert(!strcmp(a.value.as.s,"two"));
    assert(mulib_retgtk_signal_connect(&b.signals,1,free_binding_cb,&binding));
    assert(mulib_retgtk_property_set(&a,(mret_value){MRET_STRING,{.s="three"}}));
    assert(!binding.a&&!binding.b&&!binding.updating);
    mulib_retgtk_binding_free(&binding);mulib_retgtk_property_free(&a);mulib_retgtk_property_free(&b);

    /* Endpoints can be destroyed before their binding; no dangling disconnect. */
    mulib_retgtk_property_t *x=calloc(1,sizeof *x),*y=calloc(1,sizeof *y);
    assert(x&&y);
    assert(mulib_retgtk_binding_init(&binding,x,y,1));
    mulib_retgtk_property_free(x);free(x);
    assert(mulib_retgtk_property_set(y,(mret_value){MRET_INT,{.i=9}}));
    mulib_retgtk_property_free(y);free(y);
    mulib_retgtk_binding_free(&binding);
    mulib_retgtk_property_t boolean={0};
    assert(mulib_retgtk_property_set(&boolean,(mret_value){MRET_BOOL,{.i=2}}));
    assert(mulib_retgtk_signal_connect(&boolean.signals,1,count_cb,&skipped));
    assert(mulib_retgtk_property_set(&boolean,(mret_value){MRET_BOOL,{.i=1}}));
    assert(!skipped&&boolean.value.kind==MRET_BOOL&&boolean.value.as.i==1);
    mulib_retgtk_property_free(&boolean);
    int destroyed=0,calls=0;
    owned_property *owned=calloc(1,sizeof *owned);assert(owned);
    mulib_retgtk_object_init(&owned->object,1,property_owner_finalize);
    owned->destroyed=&destroyed;
    assert(mulib_retgtk_signal_bind_owner(&owned->property.signals,&owned->object));
    assert(mulib_retgtk_signal_connect(&owned->property.signals,1,drop_property_owner_cb,owned));
    assert(mulib_retgtk_signal_connect(&owned->property.signals,1,count_cb,&calls));
    mulib_retgtk_weak_t weak={0};assert(mulib_retgtk_weak_set(&weak,&owned->object));
    assert(mulib_retgtk_property_set(&owned->property,(mret_value){MRET_STRING,{.s="pinned"}}));
    assert(destroyed==1&&calls==1&&!weak.object);
}

typedef struct {
    mulib_retgtk_widget_t *parent,*sibling;
    mulib_retgtk_event_router_t *router;
    int events,clicks,destroyed,remove;
} widget_test;
static void test_destroy(mulib_retgtk_widget_t *w){widget_test *t=w->data;t->destroyed++;}
static int test_event(mulib_retgtk_widget_t *w,const mgx_event *e){
    widget_test *t=w->data;t->events++;
    if(t->remove){
        assert(mulib_retgtk_widget_remove(t->parent,w));
        assert(!t->router->capture&&!t->router->focus&&!t->router->hover);
        if(t->sibling)assert(mulib_retgtk_widget_remove(t->parent,t->sibling));
        assert(!t->destroyed); /* Route/path pins last until return. */
        return 0;
    }
    return mret_control_event(w,e);
}
static const mulib_retgtk_widget_ops test_ops={test_destroy,NULL,NULL,NULL,test_event};
static mulib_retgtk_widget_t *test_widget(uint64_t id,widget_test *t,mgx_rect bounds){
    mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&test_ops);assert(w);
    w->data=t;w->focusable=1;w->bounds=bounds;return w;
}
static mgx_event button(int down,unsigned code,double x,double y){
    mgx_event e={0};e.type=MGX_EVENT_BUTTON;e.code=code;e.down=down;e.x=x;e.y=y;return e;
}
static mgx_event key(unsigned code,unsigned mods){
    mgx_event e={0};e.type=MGX_EVENT_KEY;e.code=code;e.mods=mods;e.down=1;return e;
}
static void test_router(void){
    mulib_retgtk_event_router_t router={0};
    mulib_retgtk_widget_t *root=mulib_retgtk_widget_create(1,NULL);assert(root);
    root->bounds=(mgx_rect){0,0,200,100};
    widget_test ta={0},tb={0};
    mulib_retgtk_widget_t *a=test_widget(2,&ta,(mgx_rect){0,0,80,50});
    mulib_retgtk_widget_t *b=test_widget(3,&tb,(mgx_rect){100,0,80,50});
    assert(mulib_retgtk_widget_add(root,a)&&mulib_retgtk_widget_add(root,b));
    assert(mulib_retgtk_signal_connect(&a->signals,MRET_CLICK,count_cb,&ta.clicks));
    mgx_event e=button(1,0,10,10);
    assert(mulib_retgtk_event_dispatch(&router,root,&e));
    assert(router.capture==a&&router.focus==a&&a->pressed&&a->focused);
    e=(mgx_event){0};e.type=MGX_EVENT_MOTION;e.x=300;e.y=300;
    mulib_retgtk_event_dispatch(&router,root,&e);assert(ta.events==2&&router.capture==a);
    e=button(0,1,300,300);mulib_retgtk_event_dispatch(&router,root,&e);
    assert(router.capture==a); /* Other button cannot steal/release capture. */
    e=button(0,0,300,300);assert(mulib_retgtk_event_dispatch(&router,root,&e));
    assert(!router.capture&&!a->pressed&&!ta.clicks);
    assert(mulib_retgtk_event_capture(&router,root,a));
    e=button(0,0,150,10);mulib_retgtk_event_dispatch(&router,root,&e);
    assert(router.capture==a);
    mulib_retgtk_event_release_capture(&router);
    e=key(15,0);assert(mulib_retgtk_event_dispatch(&router,root,&e));assert(router.focus==b);
    assert(!a->focused&&b->focused);
    assert(mulib_retgtk_event_dispatch(&router,root,&e));assert(router.focus==a);
    e=key(15,1);assert(mulib_retgtk_event_dispatch(&router,root,&e));assert(router.focus==b);
    assert(mulib_retgtk_event_capture(&router,root,b));
    b->pressed=1;
    e=(mgx_event){0};e.type=MGX_EVENT_FOCUS;e.down=0;
    assert(mulib_retgtk_event_dispatch(&router,root,&e));
    assert(!router.capture&&!router.focus&&!router.hover&&!b->pressed&&!b->focused);
    assert(mulib_retgtk_event_focus(&router,root,b));
    mulib_retgtk_widget_set_enabled(root,0);
    assert(!router.focus&&!router.capture&&!router.hover&&!b->focused);
    int events=tb.events;e=key(28,0);
    assert(!mulib_retgtk_event_dispatch(&router,root,&e)&&tb.events==events);
    assert(!mulib_retgtk_tree_hit(b,110,10)); /* Disabled ancestor, subtree query. */
    mulib_retgtk_widget_set_enabled(root,1);
    assert(mulib_retgtk_event_focus(&router,root,b));
    root->visible=0; /* Legacy direct writes are reconciled at next dispatch. */
    assert(!mulib_retgtk_event_dispatch(&router,root,&e)&&!router.focus);
    root->visible=1;
    assert(mulib_retgtk_event_capture(&router,root,b));
    assert(mulib_retgtk_event_focus(&router,root,b));
    assert(mulib_retgtk_widget_remove(root,b));
    assert(!router.capture&&!router.focus&&!b->focused&&!b->pressed);
    assert(!mulib_retgtk_event_focus(&router,root,b));
    mulib_retgtk_widget_unref(b);assert(tb.destroyed==1);
    mulib_retgtk_event_router_free(&router);
    mulib_retgtk_widget_unref(a);mulib_retgtk_widget_unref(root);assert(ta.destroyed==1);
}

static void test_remove_during_event(void){
    mulib_retgtk_event_router_t router={0};
    mulib_retgtk_widget_t *root=mulib_retgtk_widget_create(1,NULL);assert(root);
    root->bounds=(mgx_rect){0,0,200,100};
    widget_test ta={0},tb={0};
    ta.parent=root;ta.router=&router;ta.remove=1;
    mulib_retgtk_widget_t *a=test_widget(2,&ta,(mgx_rect){0,0,80,50});
    mulib_retgtk_widget_t *b=test_widget(3,&tb,(mgx_rect){100,0,80,50});
    ta.sibling=b;
    assert(mulib_retgtk_widget_add(root,a)&&mulib_retgtk_widget_add(root,b));
    mulib_retgtk_widget_unref(a);mulib_retgtk_widget_unref(b); /* Tree owns both. */
    mgx_event e={0};e.type=MGX_EVENT_MOTION;e.x=10;e.y=10;
    assert(!mulib_retgtk_event_dispatch(&router,root,&e));
    assert(ta.events==1&&ta.destroyed==1&&tb.destroyed==1&&!root->n);
    assert(!router.focus&&!router.capture&&!router.hover);
    mulib_retgtk_event_router_free(&router);mulib_retgtk_widget_unref(root);
}
static void remove_click_cb(void *u,void *sender,unsigned sig,const void *data){
    widget_test *t=u;mulib_retgtk_widget_t *w=sender;(void)sig;(void)data;
    t->clicks++;
    assert(mulib_retgtk_widget_remove(t->parent,w));assert(!t->destroyed);
}
static void test_remove_during_click(void){
    mulib_retgtk_event_router_t r={0};widget_test t={0};
    mulib_retgtk_widget_t *root=mulib_retgtk_widget_create(1,NULL);assert(root);
    root->bounds=(mgx_rect){0,0,100,100};t.parent=root;
    mulib_retgtk_widget_t *w=test_widget(2,&t,(mgx_rect){0,0,80,50});
    assert(mulib_retgtk_widget_add(root,w));
    assert(mulib_retgtk_signal_connect(&w->signals,MRET_CLICK,remove_click_cb,&t));
    assert(mulib_retgtk_signal_connect(&w->signals,MRET_CLICK,count_cb,&t.clicks));
    mulib_retgtk_widget_unref(w);
    mgx_event e=button(1,0,10,10);assert(mulib_retgtk_event_dispatch(&r,root,&e));
    e.down=0;assert(mulib_retgtk_event_dispatch(&r,root,&e));
    assert(t.destroyed==1&&t.clicks==2&&!r.capture&&!r.focus);
    mulib_retgtk_event_router_free(&r);mulib_retgtk_widget_unref(root);
}

static void test_subtree_invalidation(void){
    mulib_retgtk_event_router_t r={0};widget_test t={0};
    mulib_retgtk_widget_t *root=mulib_retgtk_widget_create(1,NULL);
    mulib_retgtk_widget_t *branch=mulib_retgtk_widget_create(2,NULL);
    assert(root&&branch);
    root->bounds=branch->bounds=(mgx_rect){0,0,100,100};
    mulib_retgtk_widget_t *leaf=test_widget(3,&t,(mgx_rect){0,0,50,50});
    assert(mulib_retgtk_widget_add(root,branch)&&mulib_retgtk_widget_add(branch,leaf));
    mulib_retgtk_widget_unref(leaf);
    mgx_event e=button(1,0,10,10);
    assert(mulib_retgtk_event_dispatch(&r,root,&e)&&r.capture==leaf&&r.focus==leaf);
    mulib_retgtk_widget_set_visible(branch,0);
    assert(!r.capture&&!r.focus&&!leaf->pressed&&!leaf->focused);
    mulib_retgtk_widget_set_visible(branch,1);
    e=key(15,0);assert(mulib_retgtk_event_dispatch(&r,root,&e)&&r.focus==leaf);
    assert(mulib_retgtk_widget_remove(root,branch)&&!r.focus);
    assert(!t.destroyed); /* Detached branch still owns its leaf. */
    assert(mulib_retgtk_widget_add(root,branch));
    assert(mulib_retgtk_event_focus(&r,root,leaf));
    assert(mulib_retgtk_event_capture(&r,root,leaf));
    mulib_retgtk_widget_unref(branch);
    /* Parent finalization invalidates routers even when their leaf refs would
       otherwise keep the detached subtree alive. */
    mulib_retgtk_widget_unref(root);
    assert(!r.capture&&!r.focus&&!r.hover&&t.destroyed==1);
    mulib_retgtk_event_router_free(&r);
}

typedef struct {
    mulib_retgtk_widget_t *root,*branch;
    mulib_retgtk_event_router_t *router;
    int destroyed[3],bubble_calls;
} branch_test;
static void branch_destroy(mulib_retgtk_widget_t *w){
    branch_test *t=w->data;
    assert(w->object.id<3);t->destroyed[w->object.id]++;
    assert(!mulib_retgtk_signal_connect(&w->signals,1,count_cb,&t->bubble_calls));
}
static int branch_event(mulib_retgtk_widget_t *w,const mgx_event *e){
    branch_test *t=w->data;(void)e;
    if(w->object.id!=2){t->bubble_calls++;return 0;}
    assert(mulib_retgtk_widget_remove(t->root,t->branch));
    assert(!t->router->capture&&!t->router->focus);
    mulib_retgtk_widget_unref(t->root); /* Release the final caller ownership. */
    assert(!t->destroyed[0]&&!t->destroyed[1]&&!t->destroyed[2]);
    return 0;
}
static const mulib_retgtk_widget_ops branch_ops={branch_destroy,NULL,NULL,NULL,branch_event};
static void test_destroy_branch_during_event(void){
    branch_test t={0};mulib_retgtk_event_router_t r={0};t.router=&r;
    t.root=mulib_retgtk_widget_create(0,&branch_ops);
    t.branch=mulib_retgtk_widget_create(1,&branch_ops);
    mulib_retgtk_widget_t *leaf=mulib_retgtk_widget_create(2,&branch_ops);
    assert(t.root&&t.branch&&leaf);
    t.root->data=t.branch->data=leaf->data=&t;
    t.root->bounds=t.branch->bounds=leaf->bounds=(mgx_rect){0,0,100,100};
    leaf->focusable=1;
    assert(mulib_retgtk_widget_add(t.root,t.branch)&&mulib_retgtk_widget_add(t.branch,leaf));
    mulib_retgtk_widget_unref(t.branch);mulib_retgtk_widget_unref(leaf);
    mulib_retgtk_weak_t root_weak={0};assert(mulib_retgtk_weak_set(&root_weak,&t.root->object));
    mgx_event e=button(1,0,10,10);
    assert(!mulib_retgtk_event_dispatch(&r,t.root,&e));
    assert(!t.bubble_calls&&!root_weak.object);
    assert(t.destroyed[0]==1&&t.destroyed[1]==1&&t.destroyed[2]==1);
    mulib_retgtk_event_router_free(&r);
}

typedef struct {
    mulib_retgtk_widget_t *parent,*sibling;
    int calls,destroyed,remove;
} traverse_test;
static void traversal_destroy(mulib_retgtk_widget_t *w){
    traverse_test *t=w->data;t->destroyed++;
}
static void traversal_paint(mulib_retgtk_widget_t *w,mret_paint *p){
    traverse_test *t=w->data;(void)p;t->calls++;
    if(t->remove){
        assert(mulib_retgtk_widget_remove(t->parent,t->sibling));
        assert(mulib_retgtk_widget_remove(t->parent,w));
        assert(!t->destroyed);
    }
}
static const mulib_retgtk_widget_ops traversal_ops={traversal_destroy,NULL,NULL,traversal_paint,NULL};
static void measure_drops_self(mulib_retgtk_widget_t *w,mulib_gui_constraints_t c,mulib_gui_size_t *s){
    traverse_test *t=w->data;(void)c;
    mulib_retgtk_widget_unref(w);assert(!t->destroyed);
    *s=(mulib_gui_size_t){11,12};
}
static const mulib_retgtk_widget_ops measure_ops={traversal_destroy,measure_drops_self,NULL,NULL,NULL};
static void test_traversal_mutation(void){
    traverse_test a={0},b={0};
    mgx_surface surface={0};assert(mgx_surface_init(&surface,64,64));
    mret_paint painter={0};painter.surface=&surface;
    mulib_retgtk_widget_t *root=mulib_retgtk_widget_create(1,NULL);assert(root);
    root->bounds=(mgx_rect){0,0,64,64};
    mulib_retgtk_widget_t *first=mulib_retgtk_widget_create(2,&traversal_ops);
    mulib_retgtk_widget_t *second=mulib_retgtk_widget_create(3,&traversal_ops);assert(first&&second);
    first->data=&a;second->data=&b;first->bounds=second->bounds=root->bounds;
    a.parent=root;a.sibling=second;a.remove=1;
    assert(mulib_retgtk_widget_add(root,first)&&mulib_retgtk_widget_add(root,second));
    mulib_retgtk_widget_unref(first);mulib_retgtk_widget_unref(second);
    mgx_rect clip=surface.clip;
    mulib_retgtk_widget_paint(root,&painter);
    assert(a.calls==1&&!b.calls&&a.destroyed==1&&b.destroyed==1&&!root->n);
    assert(!memcmp(&clip,&surface.clip,sizeof clip));
    mulib_retgtk_widget_unref(root);mgx_surface_free(&surface);
    traverse_test measure={0};
    mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(4,&measure_ops);assert(w);
    w->data=&measure;
    mulib_gui_size_t size=mulib_retgtk_widget_measure(w,(mulib_gui_constraints_t){0,0,100,100});
    assert(size.w==11&&size.h==12&&measure.destroyed==1);
}

static void test_cross_tu_types(void){
    const mulib_retgtk_type_t *a=gfx_retgtk_core_type_a(),*b=gfx_retgtk_core_type_b();
    assert(a!=b&&mulib_retgtk_type_equal(a,b)&&mulib_retgtk_type_is(a,b->parent));
    mulib_retgtk_widget_t *w=gfx_retgtk_core_widget_a();assert(w);
    assert(mulib_retgtk_widget_is(w,b));
    mulib_retgtk_type_registry_t registry={0};
    assert(mulib_retgtk_type_register(&registry,a)==a);
    assert(mulib_retgtk_type_register(&registry,b)==a);
    mulib_retgtk_type_t conflict=*b;conflict.size++;
    assert(!mulib_retgtk_type_register(&registry,&conflict));
    mulib_retgtk_type_t cycle={"cycle",NULL,1,NULL};cycle.parent=&cycle;
    assert(!mulib_retgtk_type_equal(&cycle,&cycle));
    assert(!mulib_retgtk_type_create(&cycle));
    mulib_retgtk_type_registry_free(&registry);mulib_retgtk_widget_unref(w);
}
typedef struct {
    mulib_retgtk_mune_t *table;
    mulib_retgtk_widget_t *spare;
    uint64_t handle;
    int destroyed,closed_add_rejected;
} handle_test;
static void handle_destroy(mulib_retgtk_widget_t *w){
    handle_test *t=w->data;t->destroyed++;
    assert(!mulib_retgtk_mune_get(t->table,t->handle));
    assert(!mulib_retgtk_mune_drop(t->table,t->handle));
    if(t->table->closing){
        assert(!mulib_retgtk_mune_add(t->table,t->spare));
        t->closed_add_rejected++;
    }
    /* Reenter free while another drop/free is releasing this widget. */
    mulib_retgtk_mune_free(t->table);
}
static const mulib_retgtk_widget_ops handle_ops={handle_destroy,NULL,NULL,NULL,NULL};
static void test_reentrant_handle_table(void){
    mulib_retgtk_mune_t table={0};handle_test a={0},b={0};
    mulib_retgtk_widget_t *spare=mulib_retgtk_widget_create(3,NULL);assert(spare);
    a.table=b.table=&table;a.spare=b.spare=spare;
    mulib_retgtk_widget_t *first=mulib_retgtk_widget_create(1,&handle_ops);
    mulib_retgtk_widget_t *second=mulib_retgtk_widget_create(2,&handle_ops);assert(first&&second);
    first->data=&a;second->data=&b;
    a.handle=mulib_retgtk_mune_add(&table,first);b.handle=mulib_retgtk_mune_add(&table,second);
    assert(a.handle&&b.handle);
    mulib_retgtk_widget_unref(first);mulib_retgtk_widget_unref(second);
    assert(mulib_retgtk_mune_drop(&table,a.handle));
    assert(a.destroyed==1&&b.destroyed==1&&b.closed_add_rejected==1&&!table.n&&!table.v);
    mulib_retgtk_mune_free(&table);mulib_retgtk_widget_unref(spare);
}
static void test_reactive_handles(void){
    mulib_retgtk_reactive_t graph={0};
    size_t a=mulib_retgtk_reactive_add(&graph,0,0,0,6);
    size_t b=mulib_retgtk_reactive_add(&graph,0,0,0,2);
    size_t c=mulib_retgtk_reactive_add(&graph,4,a,b,0);
    assert(a==0&&b==1&&c==2);
    assert(mulib_retgtk_reactive_add(&graph,1,c,99,0)==SIZE_MAX);
    assert(mulib_retgtk_reactive_update(&graph)==3&&graph.v[c].value==3);
    assert(!mulib_retgtk_reactive_update(&graph));
    assert(mulib_retgtk_reactive_set(&graph,b,0));
    assert(mulib_retgtk_reactive_update(&graph)==2&&isnan(graph.v[c].value));
    assert(!mulib_retgtk_reactive_update(&graph));
    assert(!mulib_retgtk_reactive_set(&graph,a,NAN));
    graph.v[c].a=99;assert(!mulib_retgtk_reactive_update(&graph));
    mulib_retgtk_reactive_free(&graph);
    mulib_retgtk_mune_t handles={0};widget_test t={0};
    mulib_retgtk_widget_t *w=test_widget(1,&t,(mgx_rect){0,0,1,1});
    uint64_t handle=mulib_retgtk_mune_add(&handles,w);assert(handle);
    mulib_retgtk_widget_unref(w);w=mulib_retgtk_mune_acquire(&handles,handle);assert(w);
    assert(mulib_retgtk_mune_drop(&handles,handle));assert(!t.destroyed);
    assert(!mulib_retgtk_mune_get(&handles,handle));
    mulib_retgtk_widget_unref(w);assert(t.destroyed==1);
    mulib_retgtk_mune_free(&handles);
    w=test_widget(2,&t,(mgx_rect){0,0,1,1});
    uint64_t next=mulib_retgtk_mune_add(&handles,w);assert(next>handle);
    mulib_retgtk_widget_unref(w);mulib_retgtk_mune_free(&handles);assert(t.destroyed==2);
}
int main(void){
    test_signal_mutation();test_signal_lifetimes();test_properties_bindings();
    test_router();test_remove_during_event();test_remove_during_click();
    test_subtree_invalidation();test_destroy_branch_during_event();test_traversal_mutation();
    test_cross_tu_types();test_reactive_handles();test_reentrant_handle_table();
    puts("RetGTK core lifetime/event tests passed");return 0;
}
