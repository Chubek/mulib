#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int shiraz_kernel32(mulib_shiraz_runtime_t *r, mulib_shiraz_t *v, const char *name) {
    mulib_shiraz_cpu_t *cpu=mulib_shiraz_cpu(v); uint64_t a[7]={0},result=0; int rc;
    unsigned n=0;
    if (!strcmp(name,"ExitProcess") || !strcmp(name,"SetLastError") || !strcmp(name,"GetStdHandle") || !strcmp(name,"CloseHandle")) n=1;
    else if (!strcmp(name,"WriteFile") || !strcmp(name,"ReadFile")) n=5;
    else if (!strcmp(name,"CreateFileA") || !strcmp(name,"CreateFileW")) n=7;
    else if (!strcmp(name,"HeapAlloc") || !strcmp(name,"HeapFree")) n=3;
    else if (!strcmp(name,"VirtualAlloc")) n=4;
    rc=shiraz_args(v,a,n); if (rc) return rc;
    /* Windows DWORD stack parameters occupy eight-byte slots, but only the
       low four bytes are defined. Do not interpret neighboring stack data as
       flags/dispositions (register arguments have the same width contract). */
    if (!strcmp(name,"CreateFileA") || !strcmp(name,"CreateFileW")) {
        a[1]=(uint32_t)a[1]; a[2]=(uint32_t)a[2]; a[4]=(uint32_t)a[4]; a[5]=(uint32_t)a[5];
    } else if (!strcmp(name,"HeapAlloc") || !strcmp(name,"HeapFree")) a[1]=(uint32_t)a[1];
    else if (!strcmp(name,"VirtualAlloc")) { a[2]=(uint32_t)a[2]; a[3]=(uint32_t)a[3]; }
    if (!strcmp(name,"ExitProcess")) { mulib_shiraz_exit(v,(uint32_t)a[0]); return 0; }
    if (!strcmp(name,"GetLastError")) result=r->last_error;
    else if (!strcmp(name,"SetLastError")) r->last_error=(uint32_t)a[0];
    else if (!strcmp(name,"GetCurrentProcessId")) result=(uint32_t)getpid();
    else if (!strcmp(name,"GetProcessHeap")) result=1;
    else if (!strcmp(name,"GetStdHandle")) {
        uint32_t which=(uint32_t)a[0]; result=which==UINT32_C(0xfffffff6)?1:which==UINT32_C(0xfffffff5)?2:which==UINT32_C(0xfffffff4)?3:UINT64_MAX;
        if (result==UINT64_MAX) r->last_error=87;
    } else if (!strcmp(name,"HeapAlloc")) {
        if (a[0]!=1 || (a[1]&~UINT64_C(8)) || a[2]>SIZE_MAX) r->last_error=87;
        else { result=mulib_shiraz_allocate(v,a[2]?(size_t)a[2]:1); if (!result) r->last_error=8; }
    } else if (!strcmp(name,"HeapFree")) {
        if (a[0]==1 && !a[1] && (!a[2] || !mulib_shiraz_release(v,a[2]))) result=1;
        else r->last_error=87;
    } else if (!strcmp(name,"VirtualAlloc")) {
        if (a[0] || a[2]!=0x3000 || a[3]!=4 || !a[1] || a[1]>SIZE_MAX-65535-4095) r->last_error=87;
        else { result=mulib_shiraz_allocate(v,(((size_t)a[1]+4095)&~(size_t)4095)+65535);
            if (!result) r->last_error=8; else result=(result+65535)&~UINT64_C(65535); }
    } else if (!strcmp(name,"CloseHandle")) {
        if (a[0]>=4 && a[0]<SHIRAZ_HANDLES && r->handles[a[0]].file) {
            result=fclose(r->handles[a[0]].file)==0; r->handles[a[0]].file=NULL; if (!result) r->last_error=5;
        } else r->last_error=6;
    } else if (!strcmp(name,"CreateFileA") || !strcmp(name,"CreateFileW")) {
        char path[SHIRAZ_TEXT]; unsigned i; int fd,flags=0; FILE *file;
        result=UINT64_MAX;
        rc=shiraz_text(v,a[0],path,sizeof(path),name[10]=='W'); if (rc) return rc;
        if (a[1]&~UINT64_C(0xc0000000) || !(a[1]&0xc0000000) || a[3] || a[6] || (a[5]&~UINT64_C(0x80)) || a[2]>7) r->last_error=87;
        else {
            for(i=0;path[i];++i) if (path[i]=='\\') path[i]='/';
            if (path[0] && path[1]==':') { if (path[0]!='C' && path[0]!='c') { r->last_error=3; cpu->reg[0]=result; return 0; } memmove(path,path+2,strlen(path+2)+1); }
            flags=(a[1]&0x80000000)?(a[1]&0x40000000)?O_RDWR:O_RDONLY:O_WRONLY;
            switch(a[4]) { case 1:flags|=O_CREAT|O_EXCL;break;case 2:flags|=O_CREAT|O_TRUNC;break;case 3:break;case 4:flags|=O_CREAT;break;case 5:flags|=O_TRUNC;break;default:flags=-1;break; }
            i=shiraz_handle_new(r);
            if (!i) r->last_error=4;
            else if (flags<0) r->last_error=87;
            else {
                fd=open(path,flags,0666);
                if (fd<0) r->last_error=errno==ENOENT?2:errno==EEXIST?80:5;
                else {
                    file=fdopen(fd,(a[1]&0x80000000)?(a[1]&0x40000000)?"r+b":"rb":"wb");
                    if (!file) { close(fd); r->last_error=5; }
                    else { r->handles[i].file=file; result=i; }
                }
            }
        }
    } else if (!strcmp(name,"ReadFile") || !strcmp(name,"WriteFile")) {
        unsigned char *buffer; size_t bytes=(uint32_t)a[2],done; int reading=!strcmp(name,"ReadFile");
        if (a[0]>=SHIRAZ_HANDLES || !r->handles[a[0]].file) r->last_error=6;
        else if (a[4] || !a[3] || bytes>16u*1024u*1024u) r->last_error=87;
        else {
            buffer=(unsigned char *)malloc(bytes?bytes:1); if (!buffer) return MULIB_SHIRAZ_ENOMEM;
            /* Validate the complete destination and count pointer before host I/O. */
            rc=bytes?mulib_shiraz_read(v,a[1],buffer,bytes):0;
            if (!rc && reading && bytes) rc=mulib_shiraz_write(v,a[1],buffer,bytes);
            if (!rc) rc=shiraz_put(v,a[3],4,0);
            if (rc) { free(buffer); return rc; }
            done=reading?fread(buffer,1,bytes,r->handles[a[0]].file):fwrite(buffer,1,bytes,r->handles[a[0]].file);
            if (reading && done) rc=mulib_shiraz_write(v,a[1],buffer,done);
            if (!rc) rc=shiraz_put(v,a[3],4,done);
            if (!reading && fflush(r->handles[a[0]].file)) r->last_error=5;
            else if (ferror(r->handles[a[0]].file)) r->last_error=5;
            else result=1;
            free(buffer); if (rc) return rc;
        }
    } else return MULIB_SHIRAZ_EIMPORT;
    cpu->reg[0]=result; return 0;
}
shiraz_api shiraz_kernel32_find(const char *name) {
    static const char *const names[]={"ExitProcess","GetLastError","SetLastError","GetStdHandle",
        "WriteFile","ReadFile","CreateFileA","CreateFileW","CloseHandle","GetProcessHeap",
        "HeapAlloc","HeapFree","VirtualAlloc","GetCurrentProcessId"};
    size_t i; for(i=0;i<sizeof(names)/sizeof(*names);++i) if (!strcmp(name,names[i])) return shiraz_kernel32;
    return NULL;
}
