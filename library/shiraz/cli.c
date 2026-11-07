#define MULIB_IMPL_PROCESS
#include "mulib-process.h"
#include "mulib-shiraz.h"
#include <unistd.h>
#ifndef SHIRAZ_INCLUDE_DIR
#define SHIRAZ_INCLUDE_DIR "../../include"
#endif
#ifndef SHIRAZ_LIBRARY_DIR
#define SHIRAZ_LIBRARY_DIR "."
#endif
static void usage(void) {
    fputs("usage: shiraz run INPUT [--entry SYMBOL] [--steps COUNT]\n"
          "       shiraz emit-c INPUT OUTPUT.c [--entry SYMBOL]\n"
          "       shiraz compile INPUT OUTPUT [--entry SYMBOL] [--static|--shared-runtime]\n"
          "all modes also accept --base ADDRESS for relocation\n"
          "environment: SHIRAZ_REGISTRY, SHIRAZ_CC, SHIRAZ_INCLUDE_DIR, SHIRAZ_LIBRARY_DIR\n",stderr);
}
static int compile_image(const mulib_strbuf_t *source, const char *output, int mode) {
    char temp[]="/tmp/shiraz-XXXXXX",path[128],inc[4096],lib[4096],core[4096],win32[4096];
    const char *include=getenv("SHIRAZ_INCLUDE_DIR"),*library=getenv("SHIRAZ_LIBRARY_DIR"),*cc=getenv("SHIRAZ_CC");
    const char *args[24]; size_t n=0; FILE *f; mulib_process_t p={0}; mulib_process_status_t status; int rc;
    if (!include) include=SHIRAZ_INCLUDE_DIR;
    if (!library) library=SHIRAZ_LIBRARY_DIR;
    if (!cc) cc="cc";
    if (snprintf(inc,sizeof(inc),"-I%s",include)>=(int)sizeof(inc) ||
        snprintf(lib,sizeof(lib),"-L%s",library)>=(int)sizeof(lib) ||
        snprintf(core,sizeof(core),"%s/libshiraz.a",library)>=(int)sizeof(core) ||
        snprintf(win32,sizeof(win32),"%s/libshiraz-win32.a",library)>=(int)sizeof(win32)) return MULIB_SHIRAZ_ELIMIT;
    if (!mkdtemp(temp)) return MULIB_SHIRAZ_EIO;
    (void)snprintf(path,sizeof(path),"%s/program.c",temp);
    f=fopen(path,"wb"); if (!f) { rmdir(temp); return MULIB_SHIRAZ_EIO; }
    rc=fwrite(source->data,1,source->length,f)==source->length?0:MULIB_SHIRAZ_EIO;
    if (fclose(f)) rc=MULIB_SHIRAZ_EIO;
    if (!rc) {
        args[n++]=cc; args[n++]="-std=c11"; args[n++]="-O2"; args[n++]=inc; args[n++]=path;
        args[n++]="-o"; args[n++]=output;
        if (mode==1) args[n++]="-static";
        if (mode==2) {
            args[n++]=lib; args[n++]="-Xlinker"; args[n++]="-rpath"; args[n++]="-Xlinker"; args[n++]=library;
            args[n++]="-lshiraz-win32"; args[n++]="-lshiraz";
        } else { args[n++]=win32; args[n++]=core; }
        rc=mulib_process_spawn(&p,cc,args,n,NULL);
        if (!rc) { rc=mulib_process_wait(&p,&status,0); if (!rc && (!status.exited || status.exit_code)) rc=MULIB_SHIRAZ_EIO; }
        if (!rc) { unsigned char magic[4]; f=fopen(output,"rb"); if (!f) rc=MULIB_SHIRAZ_EIO;
            else { if (fread(magic,1,4,f)!=4 || memcmp(magic,"\177ELF",4)) rc=MULIB_SHIRAZ_EFORMAT; fclose(f); } }
    }
    unlink(path); rmdir(temp); return rc;
}
int main(int argc, char **argv) {
    mulib_shiraz_t *v=NULL; mulib_shiraz_options_t options={0}; mulib_shiraz_error_t error={0};
    mulib_shiraz_runtime_t *runtime=NULL; mulib_strbuf_t source={0}; uint64_t steps=100000000; int rc,mode=0,i,run;
    if (argc<3) { usage(); return 2; }
    run=!strcmp(argv[1],"run");
    if (!run && strcmp(argv[1],"emit-c") && strcmp(argv[1],"compile")) { usage(); return 2; }
    if (!run && argc<4) { usage(); return 2; }
    for(i=run?3:4;i<argc;++i) {
        if (!strcmp(argv[i],"--entry") && i+1<argc) options.entry_symbol=argv[++i];
        else if (!strcmp(argv[i],"--base") && i+1<argc) {
            char *end; const char *s=argv[++i]; errno=0; options.image_base=strtoull(s,&end,0);
            if (errno || !*s || *end || *s=='-' || !options.image_base) { usage(); return 2; }
        }
        else if (!strcmp(argv[i],"--static")) mode=1;
        else if (!strcmp(argv[i],"--shared-runtime")) mode=2;
        else if (run && !strcmp(argv[i],"--steps") && i+1<argc) {
            char *end; const char *s=argv[++i]; errno=0; steps=strtoull(s,&end,10);
            if (errno || !*s || *end || *s=='-' || !steps) { usage(); return 2; }
        } else { usage(); return 2; }
    }
    rc=mulib_shiraz_load_file(&v,argv[2],&options,&error);
    if (rc) { fprintf(stderr,"Shiraz: load failed (%d): %s\n",rc,error.message); return 125; }
    runtime=mulib_shiraz_runtime_create(run?getenv("SHIRAZ_REGISTRY"):NULL);
    rc=runtime?mulib_shiraz_runtime_attach(runtime,v):MULIB_SHIRAZ_EIO;
    if (rc) goto finish;
    if (run) {
        while (!rc || rc==MULIB_SHIRAZ_AGAIN) {
            size_t budget=steps>100000?100000:(size_t)steps;
            if (!budget) { rc=MULIB_SHIRAZ_ELIMIT; break; }
            rc=mulib_shiraz_run(v,budget); steps-=budget;
        }
        if (rc==MULIB_SHIRAZ_DONE) rc=(int)(mulib_shiraz_cpu(v)->exit_code&255);
        if (runtime && mulib_shiraz_runtime_save(runtime)) rc=MULIB_SHIRAZ_EIO;
    } else {
        rc=mulib_shiraz_emit_c(v,0,&source);
        if (!rc && !strcmp(argv[1],"compile")) rc=compile_image(&source,argv[3],mode);
        else if (!rc) {
            FILE *f=fopen(argv[3],"wb"); if (!f) rc=MULIB_SHIRAZ_EIO;
            else { if (fwrite(source.data,1,source.length,f)!=source.length) rc=MULIB_SHIRAZ_EIO; if (fclose(f)) rc=MULIB_SHIRAZ_EIO; }
        }
    }
finish:
    if (rc<0) fprintf(stderr,"Shiraz: error %d at 0x%llx: %s\n",rc,(unsigned long long)mulib_shiraz_error(v)->address,mulib_shiraz_error(v)->message);
    mulib_strbuf_free(&source); mulib_shiraz_runtime_destroy(runtime); mulib_shiraz_destroy(v);
    return rc<0?125:rc;
}
