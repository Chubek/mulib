#include "mulib-unidiff.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/* Run deterministic malformed inputs under ASAN/UBSAN; no external fuzzer. */
static uint64_t state=UINT64_C(0x4146beefabab0777);
static unsigned rnd(void){state^=state<<13;state^=state>>7;state^=state<<17;return (unsigned)state;}
int main(void){
  unsigned char payload[8192];
  static const char *const seeds[]={
    "--- a/file\n+++ b/file\n@@ -1 +1 @@\n-a\n+b\n",
    "--- a/file\r\n+++ b/file\r\n@@ -1 +1 @@\r\n-a\r\n+b\r\n",
    ("diff --git a/new b/new\nnew file mode 100644\nindex 0000..1000\n"
     "--- /dev/null\n+++ b/new\n@@ -0,0 +1,1 @@\n+value\n"),
    "--- a/x\tmeta\n+++ b/x\tmeta\n@@ -1 +1 @@\n-a\n+b\n"
  };
  for(int i=0;i<15000;i++){
    size_t n;
    if(i%2){
      const char *seed=seeds[rnd()%(sizeof seeds/sizeof *seeds)];
      n=strlen(seed);memcpy(payload,seed,n);
      unsigned edits=rnd()%5;
      for(unsigned edit=0;edit<edits;edit++){
        size_t offset=rnd()%n;
        payload[offset]=(unsigned char)rnd();
      }
      if(i%7==0)n=rnd()%n;
    }else{
      n=rnd()%sizeof payload;
      for(size_t j=0;j<n;j++)payload[j]=(unsigned char)rnd();
    }
    mulib_ud_patch_t patch={0};mulib_ud_limits_t limits={8192,32,256,1024,8192};
    int rc=mulib_ud_parse(payload,n,&limits,&patch,NULL);
    if(rc==0){
      for(size_t j=0;j<patch.file_count;j++){
        static const char *const bases[]={"","a\n","a\r\n"};
        const char *base=bases[rnd()%(sizeof bases/sizeof *bases)];
        mulib_ud_blob_t blob={0};(void)mulib_ud_apply(&patch.files[j],base,strlen(base),8192,&blob);
        mulib_ud_blob_free(&blob);
      }
      mulib_ud_free(&patch);
    }
  }
  puts("PASS: 15000 malformed/adversarial unified diff inputs");
  return 0;
}
