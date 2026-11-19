/* Device-free wire fixtures for the checked X11 WM path. */
#include <assert.h>
#include <stdio.h>
#include "mulib-x11-wm.h"

static void open_fixture(int fds[2],mx11_connection *c,mx11_stream *s) {
    assert(socketpair(AF_UNIX,SOCK_STREAM,0,fds)==0);
    *c=(mx11_connection){.fd=fds[0],.max_request=128};
    *s=(mx11_stream){.connection=c,.max_frame=128,.max_events=4,.max_event_bytes=128};
}
static void close_fixture(int fds[2],mx11_stream *s) {
    mulib_x11_wire_stream_clear(s);
    assert(!s->head&&!s->tail&&!s->events);
    close(fds[0]);
    close(fds[1]);
}
static void packet(int fd,uint8_t type,uint8_t data,uint16_t seq,uint32_t length,uint8_t major) {
    unsigned char p[32]={type,data};
    mx11_put16(p+2,seq);
    mx11_put32(p+4,length);
    p[10]=major;
    assert(mx11_write(fd,p,sizeof p));
}
static void sequence_and_events(void) {
    int fds[2];
    mx11_connection c;
    mx11_stream s;
    open_fixture(fds,&c,&s);
    c.sequence=65534;
    packet(fds[1],20,0,65534,10,0);            /* MapRequest */
    packet(fds[1],0,10,65535,10,2);           /* BadAccess on SelectInput */
    packet(fds[1],35,77,65535,3,0);           /* XGE with a body */
    unsigned char payload[12]={42};
    assert(mx11_write(fds[1],payload,sizeof payload));
    packet(fds[1],1,0,0,0,0);                 /* Fence reply, wraps to 0 */
    mulib_x11_wm wm={0};
    assert(mulib_x11_wm_start(&wm,&s,10,2,2)==0&&errno==EBUSY);
    assert(!wm.stream&&!c.failed&&c.sequence==0&&s.events==2&&s.event_bytes==76);
    assert(s.error.code==10&&s.error.sequence==65535&&s.error.resource==10&&s.error.major==2);
    unsigned char request[20];
    const unsigned char expected[20]={2,0,4,0,10,0,0,0,0,8,0,0,0,0,24,0,43,0,1,0};
    assert(mx11_read(fds[1],request,sizeof request)&&!memcmp(request,expected,sizeof expected));

    packet(fds[1],1,1,1,0,0);                 /* Subsequent reply is still routed. */
    unsigned char *p;
    size_t n;
    assert(mulib_x11_wire_reply(&s,43,0,NULL,0,&p,&n)==1&&n==32&&p[1]==1&&!s.error.code);
    free(p);
    assert(mulib_x11_wire_event(&s,&p,&n)==1&&n==32&&p[0]==20);
    free(p);
    assert(mulib_x11_wire_event(&s,&p,&n)==1&&n==44&&p[0]==35&&p[1]==77&&p[32]==42);
    free(p);
    assert(!s.events&&!s.event_bytes&&!s.head&&!s.tail);
    close_fixture(fds,&s);
}
static void bounded_failures(void) {
    int fds[2];
    mx11_connection c;
    mx11_stream s;
    unsigned char *p;
    size_t n;
    open_fixture(fds,&c,&s);
    packet(fds[1],1,0,1,UINT32_MAX,0); /* Reject before allocating or reading a body. */
    assert(mulib_x11_wire_reply(&s,43,0,NULL,0,&p,&n)==-1&&errno==EMSGSIZE&&c.failed&&!p&&!n);
    close_fixture(fds,&s);

    open_fixture(fds,&c,&s);
    s.max_event_bytes=33;
    packet(fds[1],20,0,0,5,0);
    packet(fds[1],20,0,0,5,0);
    assert(mulib_x11_wire_reply(&s,43,0,NULL,0,&p,&n)==-1&&errno==ENOBUFS&&c.failed&&s.events==1);
    close_fixture(fds,&s);

    open_fixture(fds,&c,&s);
    packet(fds[1],1,0,2,0,0);
    assert(mulib_x11_wire_reply(&s,43,0,NULL,0,&p,&n)==-1&&errno==EPROTO&&c.failed);
    close_fixture(fds,&s);

    open_fixture(fds,&c,&s);
    packet(fds[1],0,10,1,10,2); /* BadAccess, but barrier sequence is invalid. */
    packet(fds[1],1,0,3,0,0);
    mulib_x11_wm wm={0};
    assert(mulib_x11_wm_start(&wm,&s,10,2,2)==-1&&errno==EPROTO&&c.failed&&!s.error.code);
    close_fixture(fds,&s);

    open_fixture(fds,&c,&s);
    packet(fds[1],0,5,1,0,43);
    assert(mulib_x11_wire_reply(&s,43,0,NULL,0,&p,&n)==0&&!c.failed&&s.error.code==5);
    packet(fds[1],1,0,2,0,0);
    assert(mulib_x11_wire_reply(&s,43,0,NULL,0,&p,&n)==1&&n==32);
    free(p);
    close_fixture(fds,&s);

    open_fixture(fds,&c,&s);
    unsigned char tree[32]={1,0,1};
    mx11_put32(tree+8,10); mx11_put16(tree+16,1); /* Promises absent child. */
    assert(mx11_write(fds[1],tree,sizeof tree));
    mulib_x11_window_tree t={0};
    assert(mulib_x11_window_query_tree(&s,10,2,&t)==-1&&errno==EPROTO&&c.failed&&!t.reply);
    close_fixture(fds,&s);

    open_fixture(fds,&c,&s);
    unsigned char valid_tree[40]={1,0,1};
    mx11_put32(valid_tree+4,2); mx11_put32(valid_tree+8,10);
    mx11_put16(valid_tree+16,2); mx11_put32(valid_tree+32,11); mx11_put32(valid_tree+36,12);
    assert(mx11_write(fds[1],valid_tree,sizeof valid_tree));
    assert(mulib_x11_window_query_tree(&s,10,1,&t)==-1&&errno==E2BIG&&!c.failed&&!t.reply);
    mx11_put16(valid_tree+2,2);
    assert(mx11_write(fds[1],valid_tree,sizeof valid_tree));
    assert(mulib_x11_window_query_tree(&s,10,2,&t)==1&&t.root==10&&!t.parent&&t.children==2);
    assert(mulib_x11_window_tree_child(&t,0)==11&&mulib_x11_window_tree_child(&t,1)==12);
    assert(mulib_x11_window_tree_child(&t,2)==0);
    mulib_x11_window_tree_free(&t);
    close_fixture(fds,&s);
}
static void event_layouts(void) {
    unsigned char p[32]={23,4};
    mx11_put32(p+4,10); mx11_put32(p+8,20); mx11_put32(p+12,30);
    mx11_put16(p+16,(uint16_t)-20); mx11_put16(p+18,75);
    mx11_put16(p+20,100); mx11_put16(p+22,70);
    mx11_put16(p+24,2); mx11_put16(p+26,127);
    mulib_x11_wm_event e={.type=100};
    assert(mulib_x11_events_wm(p,31,&e)==-1&&e.type==100);
    assert(mulib_x11_events_wm(p,32,&e)==1&&e.type==MULIB_X11_CONFIGURE_REQUEST);
    assert(e.event==10&&e.window==20&&e.sibling==30&&e.x==-20&&e.y==75);
    assert(e.width==100&&e.height==70&&e.border==2&&e.mask==127&&e.detail==4);
    p[0]=128|MULIB_X11_UNMAP_NOTIFY;
    p[12]=1;
    assert(mulib_x11_events_wm(p,32,&e)==1&&e.synthetic&&e.window==20&&e.detail==1);
    p[0]=MULIB_X11_CLIENT_MESSAGE;
    p[1]=32;
    assert(mulib_x11_events_wm(p,32,&e)==1&&e.window==10&&e.atom==20&&e.data[0]==1);
}
int main(void) {
    sequence_and_events();
    bounded_failures();
    event_layouts();
    puts("X11 nested wire fixtures: PASS");
    return 0;
}
