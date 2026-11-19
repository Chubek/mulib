/* Linux, no external display or development libraries required:
 * cc -std=c11 -D_GNU_SOURCE -Iinclude examples/wl-headless-private{,-main}.c \
 *    -lm -o wl-headless-private
 * ./wl-headless-private [output.ppm|-] [seconds]
 * The program prints its private socket path, runs for 15 seconds by default,
 * then unlinks it and removes the directory. SIGINT/SIGTERM also clean up.
 */
#include "wl-headless-private.h"
#include <signal.h>
#include <time.h>

static volatile sig_atomic_t stop;

static void on_signal(int signal_number)
{
    (void)signal_number;
    stop=1;
}

static int write_ppm(const mgx_surface *surface,const char *path)
{
    FILE *file=fopen(path,"wb");
    if(!file)return 0;
    int ok=fprintf(file,"P6\n%d %d\n255\n",surface->w,surface->h)>0;
    for(int y=0;ok&&y<surface->h;y++) {
        for(int x=0;x<surface->w;x++) {
            uint32_t pixel=surface->pixels[(size_t)y*surface->stride+(size_t)x];
            unsigned char rgb[]={(unsigned char)(pixel>>16),
                                 (unsigned char)(pixel>>8),(unsigned char)pixel};
            if(fwrite(rgb,1,sizeof rgb,file)!=sizeof rgb) {
                ok=0;
                break;
            }
        }
    }
    return fclose(file)==0&&ok;
}

int main(int argc,char **argv)
{
    if(argc>3) {
        fprintf(stderr,"usage: %s [output.ppm|-] [seconds]\n",argv[0]);
        return 2;
    }
    long seconds=15;
    if(argc==3) {
        char *end;
        seconds=strtol(argv[2],&end,10);
        if(*end||seconds<1||seconds>600) {
            fputs("seconds must be between 1 and 600\n",stderr);
            return 2;
        }
    }
    const char *image=argc>=2&&strcmp(argv[1],"-")?argv[1]:NULL;
    const char *parent=getenv("TMPDIR");
    if(!parent||!*parent)parent=access("/tmp/opencode",W_OK)==0?"/tmp/opencode":"/tmp";
    struct sigaction action={0};
    action.sa_handler=on_signal;
    if(sigemptyset(&action.sa_mask)<0||sigaction(SIGINT,&action,NULL)<0||
       sigaction(SIGTERM,&action,NULL)<0) {
        perror("signal setup");
        return 1;
    }
    wl_headless_private host;
    if(!wl_headless_private_open(&host,parent)) {
        perror("private Wayland socket");
        return 1;
    }
    int status=0;
    struct timespec deadline;
    if(clock_gettime(CLOCK_MONOTONIC,&deadline)<0||
       puts(host.server.path)==EOF||fflush(stdout)!=0) {
        perror("start headless host");
        status=1;
        goto done;
    }
    deadline.tv_sec+=seconds;
    while(!stop) {
        struct timespec now;
        if(clock_gettime(CLOCK_MONOTONIC,&now)<0||!wl_headless_private_step(&host,50)) {
            perror("headless host step");
            status=1;
            break;
        }
        if(image&&host.mapped) {
            if(!write_ppm(&host.output,image)) {
                perror("capture frame");
                status=1;
                break;
            }
            image=NULL;
        }
        if(now.tv_sec>deadline.tv_sec||
           (now.tv_sec==deadline.tv_sec&&now.tv_nsec>=deadline.tv_nsec))break;
    }
done:
    wl_headless_private_close(&host);
    return status;
}
