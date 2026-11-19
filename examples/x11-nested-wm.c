/* Reference WM for a caller-created private Xvfb/Xephyr server only.
 * cc -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude examples/x11-nested-wm.c -o nested-wm
 * Usage: nested-wm --private-socket /absolute/socket /path/to/raw-16-byte-cookie
 * No DISPLAY fallback. This does not prove the supplied server is nested. */
#include <stdio.h>
#include "mulib-x11-wm.h"

static int read_cookie(const char *file,unsigned char cookie[16]) {
    FILE *f=fopen(file,"rb");
    if(!f)return 0;
    int valid=fread(cookie,1,16,f)==16&&fgetc(f)==EOF&&!ferror(f);
    if(fclose(f)!=0)return 0;
    if(!valid)errno=EINVAL;
    return valid;
}

int main(int argc,char **argv) {
    if(argc!=4||strcmp(argv[1],"--private-socket")) {
        fprintf(stderr,"usage (private nested X server only): %s --private-socket "
            "ABSOLUTE_SOCKET RAW_16_BYTE_COOKIE_FILE\n",argv[0]);
        return 2;
    }
    unsigned char cookie[16];
    if(!read_cookie(argv[3],cookie)) { perror("reading cookie"); return 2; }
    int fd=mulib_x11_wire_connect_unix(argv[2]);
    if(fd<0) { perror("connecting to private X11 socket"); return 1; }

    int status=1;
    mx11_connection c={0};
    mx11_stream stream={.connection=&c,.max_frame=64*1024,
        .max_events=512,.max_event_bytes=128*1024};
    mulib_x11_wm wm={0};
    unsigned char *setup;
    size_t n;
    if(!mulib_x11_wire_setup(&c,fd,cookie,sizeof cookie,&setup,&n)) {
        perror("X11 setup");
        goto done;
    }
    uint32_t root=mulib_x11_wire_root(setup,n,0);
    free(setup);
    if(!root) { perror("X11 screen 0"); goto done; }
    int r=mulib_x11_wm_start(&wm,&stream,root,256,1024);
    if(r==0) {
        fprintf(stderr,"another window manager owns screen 0 (BadAccess)\n");
        status=3;
        goto done;
    }
    if(r<0) { perror("claiming and scanning X11 screen 0"); goto done; }
    fprintf(stderr,"managing private X11 root 0x%08lx (%zu existing clients)\n",
        (unsigned long)root,wm.core.n);
    /* Foreground demo. The OS closes the dedicated fd on SIGINT/SIGTERM,
     * releasing SubstructureRedirect without reparenting any client. */
    for(;;) {
        unsigned char *packet;
        if(mulib_x11_wire_event(&stream,&packet,&n)<0) {
            if(errno==ECONNRESET) { status=0; break; }
            perror("reading X11 event");
            break;
        }
        r=mulib_x11_wm_dispatch(&wm,packet,n);
        free(packet);
        if(r<0) { perror("dispatching X11 event"); break; }
    }
done:
    mulib_x11_wm_clear(&wm);
    mulib_x11_wire_stream_clear(&stream);
    close(fd);
    return status;
}
