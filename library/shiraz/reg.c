#include "mulib-shiraz.h"

int main(int argc, char **argv) {
    mulib_shiraz_runtime_t *r; int rc=0; uint32_t type=0; size_t n=0; unsigned char *data=NULL;
    if (argc<5) {
        fputs("usage: shiraz-reg HIVE get|delete KEY NAME\n"
              "       shiraz-reg HIVE set KEY NAME sz|dword|binary VALUE\n"
              "REG_SZ is ASCII converted to UTF-16LE; binary VALUE is hexadecimal.\n",stderr); return 2;
    }
    r=mulib_shiraz_runtime_create(argv[1]); if (!r) { fputs("Shiraz: cannot open registry hive\n",stderr); return 1; }
    if (!strcmp(argv[2],"get") && argc==5) {
        rc=mulib_shiraz_registry_get(r,argv[3],argv[4],&type,NULL,&n);
        if (!rc) {
            size_t i; data=(unsigned char *)malloc(n?n:1); if (!data) rc=MULIB_SHIRAZ_ENOMEM;
            else {
                rc=mulib_shiraz_registry_get(r,argv[3],argv[4],&type,data,&n);
                if (!rc) {
                    if (type==4 && n==4) printf("%" PRIu32 "\n",mulib_endian_load_le32(data));
                    else if (type==1 && !(n&1)) { for(i=0;i+1<n && (data[i] || data[i+1]);i+=2) { if (data[i+1]) { rc=MULIB_SHIRAZ_EUNSUPPORTED; break; } putchar(data[i]); } putchar('\n'); }
                    else { for(i=0;i<n;++i) printf("%02x",data[i]); putchar('\n'); }
                }
            }
        }
    } else if (!strcmp(argv[2],"delete") && argc==5) rc=mulib_shiraz_registry_delete(r,argv[3],argv[4]);
    else if (!strcmp(argv[2],"set") && argc==7) {
        size_t i; n=strlen(argv[6]);
        if (!strcmp(argv[5],"sz")) {
            type=1; if (n>512u*1024u-1) rc=MULIB_SHIRAZ_ELIMIT;
            else { data=(unsigned char *)calloc(n+1,2); if (data) { for(i=0;i<n;++i) data[i*2]=(unsigned char)argv[6][i]; n=(n+1)*2; } }
        } else if (!strcmp(argv[5],"dword")) {
            char *end; unsigned long long value; errno=0; value=strtoull(argv[6],&end,0);
            type=4; n=4; if (errno || !*argv[6] || *end || value>UINT32_MAX) rc=MULIB_SHIRAZ_EINVAL;
            else { data=(unsigned char *)malloc(4); if (data) mulib_endian_store_le32(data,(uint32_t)value); }
        } else if (!strcmp(argv[5],"binary")) {
            type=3; if ((n&1) || n>2u*1024u*1024u) rc=MULIB_SHIRAZ_EINVAL;
            else { n/=2; data=(unsigned char *)malloc(n?n:1); if (data) for(i=0;i<n;++i) {
                unsigned byte=0,j; for(j=0;j<2;++j) { unsigned char c=(unsigned char)argv[6][i*2+j]; unsigned digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:16; if(digit==16) rc=MULIB_SHIRAZ_EINVAL; byte=byte*16+digit; } data[i]=(unsigned char)byte;
            } }
        } else rc=MULIB_SHIRAZ_EINVAL;
        if (!rc && !data) rc=MULIB_SHIRAZ_ENOMEM;
        if (!rc) rc=mulib_shiraz_registry_set(r,argv[3],argv[4],type,data,n);
    } else rc=MULIB_SHIRAZ_EINVAL;
    if (!rc) rc=mulib_shiraz_runtime_save(r);
    if (rc) fprintf(stderr,"Shiraz registry: error %d\n",rc);
    free(data); mulib_shiraz_runtime_destroy(r); return rc?1:0;
}
