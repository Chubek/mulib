/* Real server regression. Invoke only via run-x11-nested.sh, which starts its
 * own cookie-authenticated Xvfb; never uses the session's DISPLAY. */
#include <assert.h>
#include <stdio.h>
#include <xcb/xcb.h>
#include "mulib-x11-atoms.h"
#include "mulib-x11-wm.h"

static void checked(xcb_connection_t *c,xcb_void_cookie_t cookie) {
    xcb_generic_error_t *e=xcb_request_check(c,cookie);
    if(e)fprintf(stderr,"XCB error %u on major opcode %u\n",e->error_code,e->major_code);
    assert(!e&&!xcb_connection_has_error(c));
}
static uint8_t map_state(xcb_connection_t *c,xcb_window_t w) {
    xcb_get_window_attributes_reply_t *a=xcb_get_window_attributes_reply(c,xcb_get_window_attributes(c,w),NULL);
    assert(a);
    uint8_t value=a->map_state;
    free(a);
    return value;
}
static uint32_t focus(xcb_connection_t *c) {
    xcb_get_input_focus_reply_t *r=xcb_get_input_focus_reply(c,xcb_get_input_focus(c),NULL);
    assert(r);
    uint32_t w=r->focus;
    free(r);
    return w;
}
static void check_below(xcb_connection_t *c,xcb_window_t root,xcb_window_t w,xcb_window_t above) {
    xcb_query_tree_reply_t *r=xcb_query_tree_reply(c,xcb_query_tree(c,root),NULL);
    assert(r);
    xcb_window_t *children=xcb_query_tree_children(r);
    int found=0,n=xcb_query_tree_children_length(r);
    for(int i=0;i+1<n;i++)if(children[i]==w&&children[i+1]==above)found=1;
    free(r);
    assert(found); /* QueryTree lists bottom-most first. */
}
static xcb_window_t window(xcb_connection_t *c,xcb_window_t root,uint16_t cls,int override) {
    xcb_window_t w=xcb_generate_id(c);
    uint32_t value=(uint32_t)override;
    checked(c,xcb_create_window_checked(c,0,w,root,3,4,48,32,0,cls,0,XCB_CW_OVERRIDE_REDIRECT,&value));
    return w;
}
static void x11_open(const char *socket_path,unsigned char cookie[16],mx11_connection *c,mx11_stream *s,
        uint32_t root) {
    int fd=mulib_x11_wire_connect_unix(socket_path);
    unsigned char *setup;
    size_t n;
    assert(fd>=0&&mulib_x11_wire_setup(c,fd,cookie,16,&setup,&n));
    assert(mulib_x11_wire_root(setup,n,0)==root);
    assert(mulib_x11_wire_root(setup,n,1)==0);
    free(setup);
    *s=(mx11_stream){.connection=c,.max_frame=65536,.max_events=256,.max_event_bytes=65536};
}
/* Pump one event. The enclosing runner also provides an overall timeout. */
static int step(mulib_x11_wm *wm,mulib_x11_wm_event *out) {
    mx11_stream *s=wm->stream;
    if(!s->events) {
        struct pollfd p={s->connection->fd,POLLIN,0};
        assert(poll(&p,1,3000)==1&&(p.revents&POLLIN));
    }
    unsigned char *p;
    size_t n;
    assert(mulib_x11_wire_event(s,&p,&n)==1);
    *out=(mulib_x11_wm_event){0};
    assert(mulib_x11_events_wm(p,n,out)>=0);
    int r=mulib_x11_wm_dispatch(wm,p,n);
    if(r<0)fprintf(stderr,"WM event %u failed: %s (X11 error %u op %u)\n",
        out->type,strerror(errno),s->error.code,s->error.major);
    assert(r>=0);
    free(p);
    return r;
}
static int until(mulib_x11_wm *wm,unsigned type,uint32_t w,int synthetic) {
    for(unsigned i=0;i<64;i++) {
        mulib_x11_wm_event e;
        int r=step(wm,&e);
        if(e.type==type&&e.window==w&&e.synthetic==synthetic)return r;
    }
    assert(!"expected X11 event never received");
    return 0;
}
static xcb_atom_t atom(xcb_connection_t *c,const char *name) {
    xcb_intern_atom_reply_t *r=xcb_intern_atom_reply(c,xcb_intern_atom(c,0,(uint16_t)strlen(name),name),NULL);
    assert(r&&r->atom);
    xcb_atom_t id=r->atom;
    free(r);
    return id;
}
static void check_close(mulib_x11_wm *wm,xcb_connection_t *c,xcb_window_t w) {
    mulib_x11_atoms atoms={0};
    mulib_x11_wm_io io=mulib_x11_wm_wire_io(wm->stream);
    const char *protocols="WM_PROTOCOLS",*delete_window="WM_DELETE_WINDOW";
    uint32_t p=mulib_x11_atoms_get(&atoms,&io,protocols,0),d=mulib_x11_atoms_get(&atoms,&io,delete_window,0);
    assert(p==atom(c,protocols)&&d==atom(c,delete_window));
    assert(mulib_x11_wm_request_close(wm,w,p,d,0)==0);
    checked(c,xcb_change_property_checked(c,XCB_PROP_MODE_REPLACE,w,p,XCB_ATOM_ATOM,32,1,&d));
    assert(mulib_x11_wm_request_close(wm,w,p,d,42)==1);
    xcb_generic_event_t *event=xcb_wait_for_event(c);
    assert(event&&event->response_type==(128|XCB_CLIENT_MESSAGE));
    const xcb_client_message_event_t *msg=(const xcb_client_message_event_t *)event;
    assert(msg->format==32&&msg->window==w&&msg->type==p&&msg->data.data32[0]==d&&msg->data.data32[1]==42);
    free(event);
    assert(map_state(c,w)==XCB_MAP_STATE_VIEWABLE);
    mulib_x11_atoms_free(&atoms);
}
int main(int argc,char **argv) {
    assert(argc==3);
    unsigned char cookie[16];
    FILE *file=fopen(argv[2],"rb");
    assert(file&&fread(cookie,1,sizeof cookie,file)==sizeof cookie&&fgetc(file)==EOF);
    assert(fclose(file)==0);
    int fd=mulib_x11_wire_connect_unix(argv[1]);
    assert(fd>=0);
    unsigned char wrong[16],*setup;
    size_t n;
    memcpy(wrong,cookie,sizeof wrong);
    wrong[0]^=1;
    mx11_connection rejected={0};
    assert(!mulib_x11_wire_setup(&rejected,fd,wrong,sizeof wrong,&setup,&n));
    close(fd);
    fd=mulib_x11_wire_connect_unix(argv[1]);
    assert(fd>=0);
    xcb_auth_info_t auth={18,"MIT-MAGIC-COOKIE-1",16,(char *)cookie};
    xcb_connection_t *app=xcb_connect_to_fd(fd,&auth);
    assert(app&&!xcb_connection_has_error(app));
    xcb_screen_iterator_t screen=xcb_setup_roots_iterator(xcb_get_setup(app));
    assert(screen.rem==1);
    xcb_window_t root=screen.data->root;
    xcb_window_t a=window(app,root,XCB_WINDOW_CLASS_INPUT_OUTPUT,0);
    xcb_window_t popup=window(app,root,XCB_WINDOW_CLASS_INPUT_OUTPUT,1);
    xcb_window_t input=window(app,root,XCB_WINDOW_CLASS_INPUT_ONLY,0);
    xcb_window_t b=window(app,root,XCB_WINDOW_CLASS_INPUT_OUTPUT,0);
    checked(app,xcb_map_window_checked(app,a));
    checked(app,xcb_map_window_checked(app,popup));
    checked(app,xcb_map_window_checked(app,input));
    assert(map_state(app,a)==XCB_MAP_STATE_VIEWABLE);

    mx11_connection c,other;
    mx11_stream s,s2;
    mulib_x11_wm wm={0},second={0};
    x11_open(argv[1],cookie,&c,&s,root);
    assert(mulib_x11_wm_start(&wm,&s,root,2,10)==1);
    assert(wm.core.n==1&&wm.core.v[0].native==a&&focus(app)==a);
    assert(!mulib_x11_wm_find_native(&wm,popup)&&!mulib_x11_wm_find_native(&wm,input));
    uint64_t a_id=wm.core.v[0].id;

    x11_open(argv[1],cookie,&other,&s2,root);
    assert(mulib_x11_wm_start(&second,&s2,root,2,10)==0&&errno==EBUSY&&s2.error.code==XCB_ACCESS);
    assert(!second.stream&&!other.failed);

    /* Server redirects the request: accepted from app, not mapped yet. */
    uint32_t hints[9]={1,0};
    checked(app,xcb_change_property_checked(app,XCB_PROP_MODE_REPLACE,b,
        XCB_ATOM_WM_HINTS,XCB_ATOM_WM_HINTS,32,9,hints));
    checked(app,xcb_map_window_checked(app,b));
    assert(map_state(app,b)==XCB_MAP_STATE_UNMAPPED);
    assert(until(&wm,MULIB_X11_MAP_REQUEST,b,0)==1&&!mulib_x11_wm_find_native(&wm,b));
    assert(until(&wm,MULIB_X11_MAP_NOTIFY,b,0)==1);
    assert(wm.core.n==2&&map_state(app,b)==XCB_MAP_STATE_VIEWABLE&&focus(app)==a);
    assert(mulib_x11_wm_focus_window(&wm,b)==0);
    hints[1]=1;
    checked(app,xcb_change_property_checked(app,XCB_PROP_MODE_REPLACE,b,
        XCB_ATOM_WM_HINTS,XCB_ATOM_WM_HINTS,32,9,hints));
    assert(mulib_x11_wm_focus_window(&wm,b)==1&&focus(app)==b);

    /* Registry changes only on ConfigureNotify, not after the accepted
     * request. Signed coordinates and a nontrivial mask survive forwarding. */
    const uint32_t values[]={(uint32_t)-8,66,120,55,3,a,XCB_STACK_MODE_BELOW};
    checked(app,xcb_configure_window_checked(app,b,127,values));
    assert(until(&wm,MULIB_X11_CONFIGURE_REQUEST,b,0)==1);
    assert(mulib_x11_wm_find_native(&wm,b)->rect.x==3);
    assert(until(&wm,MULIB_X11_CONFIGURE_NOTIFY,b,0)==1);
    mulib_wm_rect rect=mulib_x11_wm_find_native(&wm,b)->rect;
    assert(rect.x==-8&&rect.y==66&&rect.w==120&&rect.h==55);
    xcb_get_geometry_reply_t *g=xcb_get_geometry_reply(app,xcb_get_geometry(app,b),NULL);
    assert(g&&g->x==rect.x&&g->y==rect.y&&g->width==rect.w&&g->height==rect.h&&g->border_width==3);
    free(g);
    check_below(app,root,b,a);
    check_close(&wm,app,b);

    uint32_t event[8]={0};
    xcb_unmap_notify_event_t unmap={.response_type=XCB_UNMAP_NOTIFY,.event=root,.window=b};
    memcpy(event,&unmap,sizeof unmap);
    checked(app,xcb_send_event_checked(app,0,root,XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,(const char *)event));
    assert(until(&wm,MULIB_X11_UNMAP_NOTIFY,b,1)==0&&mulib_x11_wm_find_native(&wm,b));

    /* Client limits never prevent a valid map. */
    xcb_window_t extra=window(app,root,XCB_WINDOW_CLASS_INPUT_OUTPUT,0);
    checked(app,xcb_map_window_checked(app,extra));
    assert(until(&wm,MULIB_X11_MAP_REQUEST,extra,0)==1);
    assert(until(&wm,MULIB_X11_MAP_NOTIFY,extra,0)==0);
    assert(map_state(app,extra)==XCB_MAP_STATE_VIEWABLE&&!mulib_x11_wm_find_native(&wm,extra));

    checked(app,xcb_unmap_window_checked(app,b));
    assert(until(&wm,MULIB_X11_UNMAP_NOTIFY,b,0)==1&&!mulib_x11_wm_find_native(&wm,b));
    assert(focus(app)==a&&wm.core.focused==a_id);
    checked(app,xcb_map_window_checked(app,b));
    assert(until(&wm,MULIB_X11_MAP_NOTIFY,b,0)==1&&focus(app)==b);
    checked(app,xcb_configure_window_checked(app,b,127,values));
    checked(app,xcb_reparent_window_checked(app,b,a,9,10));
    assert(until(&wm,MULIB_X11_CONFIGURE_REQUEST,b,0)==0); /* Now under a different parent. */
    assert(until(&wm,MULIB_X11_REPARENT_NOTIFY,b,0)==0&&!mulib_x11_wm_find_native(&wm,b));
    assert(focus(app)==a);
    checked(app,xcb_destroy_window_checked(app,a));
    (void)until(&wm,MULIB_X11_DESTROY_NOTIFY,a,0);
    assert(!wm.core.n&&!wm.core.focused&&focus(app)==XCB_INPUT_FOCUS_POINTER_ROOT);

    xcb_window_t doomed=window(app,root,XCB_WINDOW_CLASS_INPUT_OUTPUT,0);
    checked(app,xcb_map_window_checked(app,doomed));
    checked(app,xcb_destroy_window_checked(app,doomed));
    assert(until(&wm,MULIB_X11_MAP_REQUEST,doomed,0)==0&&!c.failed);
    assert(until(&wm,MULIB_X11_DESTROY_NOTIFY,doomed,0)==0);

    /* Disconnect releases ownership, leaving already mapped clients intact.
     * Wait for the server to process EOF (requests on other connections race). */
    mulib_x11_wm_clear(&wm);
    mulib_x11_wire_stream_clear(&s);
    close(c.fd);
    int r=0;
    for(unsigned i=0;i<60&&!r;i++) {
        r=mulib_x11_wm_start(&second,&s2,root,2,10);
        if(!r)assert(poll(NULL,0,10)==0);
    }
    assert(r==1&&second.core.n==1&&second.core.v[0].native==extra);
    assert(map_state(app,extra)==XCB_MAP_STATE_VIEWABLE&&focus(app)==extra);
    mulib_x11_wm_clear(&second);
    mulib_x11_wire_stream_clear(&s2);
    close(other.fd);
    xcb_disconnect(app);
    puts("X11 nested Xvfb lifecycle: PASS");
    return 0;
}
