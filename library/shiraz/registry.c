#include "internal.h"

/* Keys are normalized UTF-8/ANSI bytes with ASCII case folding. Values use
 * key NUL value-name as the KVDB key, and LE32 type followed by Windows bytes.
 * A key-only record (without NUL) preserves empty registry keys. */
static int shiraz_registry_name(const char *key, const char *name, char *out, size_t *size) {
    size_t i,n,k; const char *roots[]={"HKCU","HKLM","HKCR","HKU"}; int valid=0;
    if (!key || !name) return MULIB_SHIRAZ_EINVAL;
    k=strlen(key); n=strlen(name); if (!k || k>=SHIRAZ_TEXT || n>=SHIRAZ_TEXT || k+n+1>=SHIRAZ_TEXT*2) return MULIB_SHIRAZ_ELIMIT;
    for(i=0;i<4;++i) { size_t len=strlen(roots[i]); char root[5];
        if (k<len || (key[len] && key[len]!='\\' && key[len]!='/')) continue;
        memcpy(root,key,len); root[len]=0; if (shiraz_equal(root,roots[i])) valid=1;
    }
    if (!valid) return MULIB_SHIRAZ_EINVAL;
    while(k && (key[k-1]=='\\' || key[k-1]=='/')) --k;
    memcpy(out,key,k); out[k]=0; memcpy(out+k+1,name,n);
    for(i=0;i<k+n+1;++i) { if (out[i]>='A' && out[i]<='Z') out[i]+=32; if (i<k && out[i]=='/') out[i]='\\'; }
    *size=k+n+1; return 0;
}
int shiraz_registry_key(mulib_shiraz_runtime_t *r, const char *key, int create) {
    char normalized[SHIRAZ_TEXT*2]; size_t n,bytes; const void *data; int rc;
    rc=shiraz_registry_name(key,"",normalized,&n); if (rc) return rc;
    if (create) {
        size_t i;
        for(i=0;i<n;++i) if (!normalized[i] || normalized[i]=='\\')
            if (mulib_kvdb_set(r->registry,normalized,i,"",0)) return MULIB_SHIRAZ_ENOMEM;
        return 0;
    }
    if (!strchr(normalized,'\\')) return 0;
    return mulib_kvdb_get(r->registry,normalized,n-1,&data,&bytes)?MULIB_SHIRAZ_EIMPORT:0;
}
int mulib_shiraz_registry_set(mulib_shiraz_runtime_t *r, const char *key, const char *name, uint32_t type, const void *data, size_t bytes) {
    char normalized[SHIRAZ_TEXT*2]; unsigned char *value; size_t n; int rc;
    if (!r || (!data && bytes)) return MULIB_SHIRAZ_EINVAL;
    if (bytes>1024u*1024u) return MULIB_SHIRAZ_ELIMIT;
    if ((type!=1 && type!=3 && type!=4) || (type==4 && bytes!=4) || (type==1 && (bytes&1))) return MULIB_SHIRAZ_EUNSUPPORTED;
    rc=shiraz_registry_name(key,name,normalized,&n); if (rc) return rc;
    value=(unsigned char *)malloc(bytes+4); if (!value) return MULIB_SHIRAZ_ENOMEM;
    mulib_endian_store_le32(value,type); if (bytes) memcpy(value+4,data,bytes);
    rc=shiraz_registry_key(r,key,1);
    if (!rc && mulib_kvdb_set(r->registry,normalized,n,value,bytes+4)) rc=MULIB_SHIRAZ_ENOMEM;
    free(value); return rc;
}
int mulib_shiraz_registry_get(mulib_shiraz_runtime_t *r, const char *key, const char *name, uint32_t *type, void *data, size_t *bytes) {
    char normalized[SHIRAZ_TEXT*2]; const void *value; size_t n,length; int rc;
    if (!r || !type || !bytes) return MULIB_SHIRAZ_EINVAL;
    rc=shiraz_registry_name(key,name,normalized,&n); if (rc) return rc;
    rc=mulib_kvdb_get(r->registry,normalized,n,&value,&length); if (rc) return MULIB_SHIRAZ_EIMPORT;
    if (length<4) return MULIB_SHIRAZ_EFORMAT;
    *type=mulib_endian_load_le32((const unsigned char *)value); length-=4;
    if (data && *bytes<length) { *bytes=length; return MULIB_SHIRAZ_EBOUNDS; }
    if (data && length) memcpy(data,(const unsigned char *)value+4,length);
    *bytes=length; return 0;
}
int mulib_shiraz_registry_delete(mulib_shiraz_runtime_t *r, const char *key, const char *name) {
    char normalized[SHIRAZ_TEXT*2]; size_t n; int rc;
    if (!r) return MULIB_SHIRAZ_EINVAL;
    rc=shiraz_registry_name(key,name,normalized,&n); if (rc) return rc;
    return mulib_kvdb_remove(r->registry,normalized,n)?MULIB_SHIRAZ_EIMPORT:0;
}
static const char *shiraz_registry_handle(mulib_shiraz_runtime_t *r, uint64_t h) {
    if (h!=(uint32_t)h && h!=(UINT64_C(0xffffffff00000000)|(uint32_t)h)) return NULL;
    switch((uint32_t)h) { case 0x80000000:return "HKCR";case 0x80000001:return "HKCU";
        case 0x80000002:return "HKLM";case 0x80000003:return "HKU";default:break; }
    return h>=4 && h<SHIRAZ_HANDLES?r->handles[h].key:NULL;
}
static int shiraz_advapi32(mulib_shiraz_runtime_t *r, mulib_shiraz_t *v, const char *name) {
    uint64_t a[9]={0},size=0; uint32_t type=0; unsigned n,result=0; int rc,wide=name[strlen(name)-1]=='W';
    char key[SHIRAZ_TEXT],value[SHIRAZ_TEXT]; const char *parent;
    int create=!strncmp(name,"RegCreateKeyEx",14),open=!strncmp(name,"RegOpenKeyEx",12),set=!strncmp(name,"RegSetValueEx",13),query=!strncmp(name,"RegQueryValueEx",15);
    n=create?9:open?5:set||query?6:!strcmp(name,"RegCloseKey")?1:2;
    rc=shiraz_args(v,a,n); if (rc) return rc;
    if (create) { a[2]=(uint32_t)a[2]; a[4]=(uint32_t)a[4]; a[5]=(uint32_t)a[5]; }
    else if (open) { a[2]=(uint32_t)a[2]; a[3]=(uint32_t)a[3]; }
    else if (set) { a[2]=(uint32_t)a[2]; a[3]=(uint32_t)a[3]; a[5]=(uint32_t)a[5]; }
    parent=shiraz_registry_handle(r,a[0]); if (!parent) { mulib_shiraz_cpu(v)->reg[0]=6; return 0; }
    if (create || open) {
        unsigned h; size_t len=strlen(parent); uint64_t out=create?a[7]:a[4]; int existed;
        if ((create && (a[2] || a[3] || a[4] || a[6])) || (!create && a[2])) { result=87; goto done; }
        rc=shiraz_text(v,a[1],value,sizeof(value),wide); if (rc) return rc;
        if (len+strlen(value)+2>sizeof(key)) { result=206; goto done; }
        strcpy(key,parent); if (*value) { strcat(key,"\\"); strcat(key,value); }
        existed=!shiraz_registry_key(r,key,0);
        if (!create && !existed) { result=2; goto done; }
        h=shiraz_handle_new(r); if (!h) { result=4; goto done; }
        rc=shiraz_put(v,out,8,h); if (rc) return rc;
        if (create && a[8]) { rc=shiraz_put(v,a[8],4,existed?2:1); if (rc) return rc; }
        r->handles[h].key=(char *)malloc(strlen(key)+1); if (!r->handles[h].key) return MULIB_SHIRAZ_ENOMEM;
        strcpy(r->handles[h].key,key);
        if (create) { rc=shiraz_registry_key(r,key,1); if (rc) { free(r->handles[h].key); r->handles[h].key=NULL; return rc; } }
    } else if (!strcmp(name,"RegCloseKey")) {
        if (a[0]>=4 && a[0]<SHIRAZ_HANDLES) { free(r->handles[a[0]].key); r->handles[a[0]].key=NULL; }
    } else {
        if (a[1]) { rc=shiraz_text(v,a[1],value,sizeof(value),wide); if (rc) return rc; } else value[0]=0;
        if ((set || query) && a[2]) { result=87; goto done; }
        if (set) {
            unsigned char *data; size_t bytes=(uint32_t)a[5];
            if (bytes>1024u*1024u) { result=8; goto done; }
            data=(unsigned char *)malloc(bytes?bytes:1); if (!data) return MULIB_SHIRAZ_ENOMEM;
            rc=bytes?mulib_shiraz_read(v,a[4],data,bytes):0;
            /* REG_SZ is stored canonically as UTF-16LE so A/W share values. */
            if (!rc && a[3]==1 && !wide) {
                unsigned char *utf=(unsigned char *)calloc(bytes?bytes:1,2); size_t i;
                if (!utf) { free(data); return MULIB_SHIRAZ_ENOMEM; }
                for(i=0;i<bytes;++i) utf[2*i]=data[i];
                free(data); data=utf; bytes*=2;
            }
            if (!rc) rc=mulib_shiraz_registry_set(r,parent,value,(uint32_t)a[3],data,bytes);
            free(data); if (rc==MULIB_SHIRAZ_EUNSUPPORTED) result=87; else if (rc) return rc;
        } else if (query) {
            unsigned char *data=NULL; size_t bytes=0,i;
            if (!a[5]) { result=87; goto done; }
            rc=shiraz_get(v,a[5],4,&size); if (rc) return rc;
            rc=mulib_shiraz_registry_get(r,parent,value,&type,NULL,&bytes); if (rc==MULIB_SHIRAZ_EIMPORT) { result=2; goto done; } if (rc) return rc;
            data=(unsigned char *)malloc(bytes?bytes:1); if (!data) return MULIB_SHIRAZ_ENOMEM;
            rc=mulib_shiraz_registry_get(r,parent,value,&type,data,&bytes); if (rc) { free(data); return rc; }
            if (type==1 && !wide) {
                if (bytes&1) { free(data); return MULIB_SHIRAZ_EFORMAT; }
                for(i=0;i<bytes/2;++i) { if (data[2*i+1]) { free(data); return MULIB_SHIRAZ_EUNSUPPORTED; } data[i]=data[2*i]; }
                bytes/=2;
            }
            if (a[4] && size<bytes) result=234;
            else if (a[4]) rc=mulib_shiraz_write(v,a[4],data,bytes);
            if (!rc) rc=shiraz_put(v,a[5],4,bytes);
            if (!rc && a[3]) rc=shiraz_put(v,a[3],4,type);
            free(data); if (rc) return rc;
        } else {
            rc=mulib_shiraz_registry_delete(r,parent,value); if (rc==MULIB_SHIRAZ_EIMPORT) result=2; else if (rc) return rc;
        }
    }
done: mulib_shiraz_cpu(v)->reg[0]=result; return 0;
}
shiraz_api shiraz_advapi32_find(const char *name) {
    static const char *const names[]={"RegCreateKeyExA","RegCreateKeyExW","RegOpenKeyExA","RegOpenKeyExW",
        "RegSetValueExA","RegSetValueExW","RegQueryValueExA","RegQueryValueExW","RegCloseKey","RegDeleteValueA","RegDeleteValueW"};
    size_t i; for(i=0;i<sizeof(names)/sizeof(*names);++i) if (!strcmp(name,names[i])) return shiraz_advapi32;
    return NULL;
}
