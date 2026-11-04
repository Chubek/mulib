#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#include "mulib-archml.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char tiny[] =
  "; transactional tiny target\n(archml 1 test (target test (bits 64) (endian little))"
  "(pcode-op nop (arity 0 0) (semantics \"advance\\nPC\"))"
  "(instruction nop (operands) (encoding (base \"90\")) (pcode (nop))))";

static void bad (const char *source, mulib_archml_result expected)
{
  mulib_archml_t *sentinel = NULL, *out;
  mulib_archml_error_t error;
  assert (mulib_archml_parse (tiny, sizeof (tiny)-1, NULL, &sentinel, &error) == MULIB_ARCHML_OK);
  out = sentinel;
  assert (mulib_archml_parse (source, strlen (source), NULL, &out, &error) == expected);
  assert (out == sentinel && error.code == expected && error.line && error.column && error.message);
  mulib_archml_destroy (sentinel);
}
static void syntax (void)
{
  mulib_archml_t *a = NULL;
  mulib_archml_error_t e;
  mulib_archml_options_t o = {0};
  bad ("", MULIB_ARCHML_SYNTAX);
  bad ("(archml 1 t", MULIB_ARCHML_SYNTAX);
  bad ("(archml 1 t) extra", MULIB_ARCHML_SYNTAX);
  bad ("(archml 2 t)", MULIB_ARCHML_SCHEMA);
  bad ("(archml 1 t)", MULIB_ARCHML_SCHEMA);
  bad ("(archml 1 t (target t (bits 64) (endian little) (bits 32)))", MULIB_ARCHML_SCHEMA);
  bad ("(archml 1 t (target t (bits 64) (endian little)) (feature a (requires (ref feature missing))))", MULIB_ARCHML_SCHEMA);
  bad ("(archml 1 t (target t (bits 64) (endian little)) (feature a) (feature a))", MULIB_ARCHML_SCHEMA);
  bad ("(archml 1 t (target t (bits 64) (endian little)) (feature a (requires (ref feature b))) (feature b (requires (ref feature a))))", MULIB_ARCHML_SCHEMA);
  bad ("(archml 1 t (target t (bits 64) (endian little)) (instruction x (operands) (encoding (base \"00\")) (pcode (oops))))", MULIB_ARCHML_SCHEMA);
  bad ("(archml 1 t (target t (bits 64) (endian little)) (instruction x (operands) (encoding (base \"00\") (base \"01\")) (pcode (nop))))", MULIB_ARCHML_SCHEMA);
  bad ("(archml 1 t (target t (bits 18446744073709551616) (endian little)))", MULIB_ARCHML_SCHEMA);
  bad ("(archml 1 t (target t (bits 64) (endian little) (doc \"bad\\q\")))", MULIB_ARCHML_SYNTAX);
  o.max_source = 10;
  assert (mulib_archml_parse (tiny, sizeof(tiny)-1, &o, &a, &e) == MULIB_ARCHML_LIMIT && !a);
  o.max_source = 0; o.max_nodes = 3;
  assert (mulib_archml_parse (tiny, sizeof(tiny)-1, &o, &a, &e) == MULIB_ARCHML_LIMIT && !a);
  o.max_nodes = 0; o.max_depth = 2;
  assert (mulib_archml_parse (tiny, sizeof(tiny)-1, &o, &a, &e) == MULIB_ARCHML_LIMIT && !a);
  o.max_depth = 0; o.max_work = 100;
  assert (mulib_archml_parse (tiny, sizeof(tiny)-1, &o, &a, &e) == MULIB_ARCHML_LIMIT && !a);
  assert (mulib_archml_parse (NULL, 1, NULL, &a, &e) == MULIB_ARCHML_INVALID);
  assert (mulib_archml_load ("missing-architecture.aml", NULL, &a, &e) == MULIB_ARCHML_IO);
  assert (mulib_archml_parse (tiny, sizeof(tiny)-1, NULL, &a, &e) == MULIB_ARCHML_OK);
  assert (mulib_archml_count (a,"instruction") == 1 && !strcmp (mulib_archml_name(a),"test"));
  assert (!strcmp (mulib_archml_child (mulib_archml_property (mulib_archml_find(a,"pcode-op","nop"),"semantics"),1)->text,"advance\nPC"));
  mulib_archml_destroy (a);
  /* Every prefix of a valid document remains a clean syntax error. */
  for (size_t i = 0; i < sizeof(tiny)-1; ++i) {
    a = NULL;
    assert (mulib_archml_parse (tiny,i,NULL,&a,&e) != MULIB_ARCHML_OK && !a);
  }
  {
    char owned_source[sizeof(tiny)];
    memcpy(owned_source,tiny,sizeof(tiny)); a=NULL;
    assert(mulib_archml_parse(owned_source,sizeof(tiny)-1,NULL,&a,&e)==MULIB_ARCHML_OK);
    memset(owned_source,'x',sizeof(owned_source));
    assert(!strcmp(mulib_archml_name(a),"test"));
    mulib_archml_destroy(a);
  }
  {
    mulib_archml_node_t number = {0}; uint64_t u=99; int64_t v=99;
    number.kind=MULIB_ARCHML_ATOM; number.text="-9223372036854775808";
    assert(mulib_archml_i64(&number,&v) && v==INT64_MIN);
    assert(!mulib_archml_u64(&number,&u) && u==99);
    number.text="9223372036854775808";
    assert(!mulib_archml_i64(&number,&v) && v==INT64_MIN);
    number.text="0xffffffffffffffff";
    assert(mulib_archml_u64(&number,&u) && u==UINT64_MAX);
  }
}

static void ranges (void)
{
  const char source[] = "(archml 1 t (target t (bits 64) (endian little))"
    "(pcode-op nop (arity 0 0) (semantics \"advance\"))"
    "(instruction i (operands (imm (kind immediate) (access read) (bits 6) (signed true) (scale 4)))"
    "(encoding (base \"40\") (field imm 0 3 3) (field imm 3 3 0)) (pcode (nop))))";
  mulib_archml_t *a=NULL; mulib_archml_error_t e;
  unsigned char bytes[2]; size_t used;
  assert(mulib_archml_parse(source,sizeof(source)-1,NULL,&a,&e)==MULIB_ARCHML_OK);
  for(int n=-140;n<=140;++n) {
    uint64_t value=(uint64_t)(int64_t)n, result=99; bytes[0]=0xa5; used=99;
    int rc=mulib_archml_encode(a,"i",&value,1,bytes,1,&used);
    if(n>=-128 && n<=124 && n%4==0) {
      assert(rc==MULIB_ARCHML_OK && used==1);
      assert(mulib_archml_decode(a,"i",bytes,1,&result,1,&used)==MULIB_ARCHML_OK && result==value);
      bytes[0]^=0x80; used=99; result=99;
      assert(mulib_archml_decode(a,"i",bytes,1,&result,1,&used)==MULIB_ARCHML_MISMATCH && used==99 && result==99);
    } else assert(rc==MULIB_ARCHML_RANGE && bytes[0]==0xa5 && used==99);
  }
  mulib_archml_destroy(a);
  /* A field may not overwrite a fixed bit, duplicate a source bit, or omit it. */
  char broken[sizeof(source)];
  memcpy(broken,source,sizeof(source));
  char *at=strstr(broken,"imm 0 3 3"); assert(at); at[8]='0';
  bad(broken,MULIB_ARCHML_SCHEMA);
}

static void golden (mulib_archml_t *a, const char *name, const uint64_t *values, size_t count, const char *hex)
{
  unsigned char out[32], expected[32];
  uint64_t decoded[16] = {0};
  size_t n = strlen(hex)/2, used = 99;
  for (size_t i = 0; i < n; ++i) {
    unsigned b;
    assert (sscanf (hex+2*i,"%2x",&b) == 1); expected[i] = (unsigned char)b;
  }
  assert (mulib_archml_encode(a,name,values,count,out,sizeof(out),&used) == MULIB_ARCHML_OK);
  if (used != n || memcmp(out,expected,n)) {
    fprintf(stderr,"bad encoding %s/%s: ",mulib_archml_name(a),name);
    for(size_t i=0;i<used;++i) fprintf(stderr,"%02x",out[i]);
    fprintf(stderr," expected %s\n",hex); assert(0);
  }
  assert (mulib_archml_decode(a,name,out,n,decoded,count,&used) == MULIB_ARCHML_OK && used == n);
  assert (!count || !memcmp(values,decoded,count*sizeof(*values)));
  memset(out,0xa5,sizeof(out)); used = 99;
  assert (mulib_archml_encode(a,name,values,count,out,n-1,&used) == MULIB_ARCHML_LIMIT && used == 99 && out[0] == 0xa5);
}

static void all_forms (mulib_archml_t *a)
{
  for (size_t i = 0; i < mulib_archml_count(a,"instruction"); ++i) {
    const mulib_archml_node_t *n = mulib_archml_at(a,"instruction",i);
    const char *name = mulib_archml_child(n,1)->text;
    const mulib_archml_node_t *ops = mulib_archml_property(n,"operands");
    uint64_t values[16] = {0}, copy[16] = {0};
    size_t count = 0, used = 0;
    unsigned char bytes[32];
    for (const mulib_archml_node_t *op = mulib_archml_child(ops,1); op; op = op->next) {
      const mulib_archml_node_t *cls = mulib_archml_property(op,"class");
      uint64_t bits, scale;
      assert (mulib_archml_u64(mulib_archml_child(mulib_archml_property(op,"bits"),1),&bits));
      assert (mulib_archml_u64(mulib_archml_child(mulib_archml_property(op,"scale"),1),&scale));
      values[count] = (bits > 3 ? 5 : 0) * scale;
      if (cls) values[count] = 1;
      ++count;
    }
    assert (mulib_archml_encode(a,name,values,count,bytes,sizeof(bytes),&used) == MULIB_ARCHML_OK);
    assert (mulib_archml_decode(a,name,bytes,used,copy,count,&used) == MULIB_ARCHML_OK);
    assert (!memcmp(values,copy,count*sizeof(*values)));
    for (size_t j=0;j<used;++j) {
      size_t consumed=99; uint64_t before[16]; memset(before,0xa5,sizeof(before)); memcpy(copy,before,sizeof(copy));
      assert (mulib_archml_decode(a,name,bytes,j,copy,count,&consumed) == MULIB_ARCHML_LIMIT && consumed==99);
      assert (!memcmp(copy,before,sizeof(copy)));
    }
  }
}
int main (int argc, char **argv)
{
  const char *names[] = {"amd64", "arm64", "risv"};
  char directory[4096], path[4096];
  /* Machine-readable encoder for the independent LLVM MC fixture check. */
  if (argc >= 4 && !strcmp(argv[1],"--encode")) {
    mulib_archml_t *a = NULL; mulib_archml_error_t e;
    uint64_t values[16]; unsigned char bytes[32]; size_t length = 0;
    assert (argc-4 <= 16);
    for (int i=4;i<argc;++i) values[i-4] = strtoull(argv[i],NULL,0);
    if (mulib_archml_load(argv[2],NULL,&a,&e) != MULIB_ARCHML_OK) {
      fprintf(stderr,"%s:%zu: %s\n",argv[2],e.line,e.message); return 1;
    }
    assert (mulib_archml_encode(a,argv[3],values,(size_t)(argc-4),bytes,sizeof(bytes),&length) == MULIB_ARCHML_OK);
    for (size_t i=0;i<length;++i) printf("%02x",bytes[i]);
    puts(""); mulib_archml_destroy(a); return 0;
  }
  if (argc > 1) assert (snprintf(directory,sizeof(directory),"%s",argv[1]) < (int)sizeof(directory));
  else {
    const char *slash = strrchr(__FILE__,'/');
    int n = slash ? (int)(slash-__FILE__) : 0;
    assert (snprintf(directory,sizeof(directory),"%.*s%s",n,__FILE__,n ? "/../library/archml" : "library/archml") < (int)sizeof(directory));
  }
  syntax ();
  ranges ();
  for (size_t i = 0; i < 3; ++i) {
    mulib_archml_t *a = NULL;
    mulib_archml_error_t e;
    assert (snprintf(path,sizeof(path),"%s/%s.aml",directory,names[i]) < (int)sizeof(path));
    int rc = mulib_archml_load(path,NULL,&a,&e);
    if (rc != MULIB_ARCHML_OK) fprintf(stderr,"%s:%zu:%zu %s\n",path,e.line,e.column,e.message);
    assert (rc == MULIB_ARCHML_OK && !strcmp(mulib_archml_name(a),names[i]));
    assert (mulib_archml_count(a,"instruction") >= 60 && mulib_archml_count(a,"register") > 100);
    const char *kinds[] = {"regclass","instruction","mode","memory","atomic","exception","vector","predication","feature","legalize","selection","schedule","rewrite","layout","syntax","extension","smt","abi"};
    for(size_t k=0;k<sizeof(kinds)/sizeof(*kinds);++k) assert (mulib_archml_count(a,kinds[k]));
    all_forms(a);
    if (!i) {
      const uint64_t rr[] = {9,10}, mem[] = {9,12,(uint64_t)-16}, imm[] = {8,UINT64_C(0x1122334455667788)};
      golden(a,"add64-rr",rr,2,"4d01d1");
      golden(a,"load64-base-disp32",mem,3,"4d8b8c24f0ffffff");
      golden(a,"mov64-ri",imm,2,"49b88877665544332211");
      golden(a,"ret",NULL,0,"c3");
      assert (mulib_archml_register_overlap(a,"rax","ah") == 1);
      assert (mulib_archml_register_overlap(a,"al","ah") == 0);
      assert (mulib_archml_register_overlap(a,"rax","rcx") == 0);
      const char *features[] = {"base","sse","sse2"};
      assert (mulib_archml_enabled(a,mulib_archml_find(a,"instruction","addsd"),features,3) == 1);
      assert (mulib_archml_enabled(a,mulib_archml_find(a,"instruction","addsd"),features,1) == 0);
    } else if (i == 1) {
      const uint64_t rr[] = {0,1,2}, load[] = {3,31,16}, branch[] = {16}, adr[] = {0,4096};
      golden(a,"add64",rr,3,"2000028b");
      golden(a,"ldr64",load,3,"e30b40f9");
      golden(a,"b",branch,1,"04000014");
      golden(a,"adrp",adr,2,"000000b0");
      assert (mulib_archml_register_overlap(a,"x0","w0") == 1);
      assert (mulib_archml_register_overlap(a,"sp","xzr") == 0);
    } else {
      const uint64_t rr[] = {10,11,12}, load[] = {10,2,(uint64_t)-8}, branch[] = {10,11,8}, jal[] = {1,16};
      golden(a,"add",rr,3,"3385c500");
      golden(a,"ld",load,3,"033581ff");
      golden(a,"beq",branch,3,"6304b500");
      golden(a,"jal",jal,2,"ef000001");
      assert (mulib_archml_register_overlap(a,"a0","x10") == 1);
      uint64_t bad_disp[] = {1,3}; unsigned char output[32]; size_t used=99;
      memset(output,0xa5,sizeof(output));
      assert (mulib_archml_encode(a,"jal",bad_disp,2,output,sizeof(output),&used) == MULIB_ARCHML_RANGE);
      assert (used == 99 && output[0] == 0xa5);
      uint64_t reserved_rm[] = {10,11,12,5};
      assert(mulib_archml_encode(a,"fadd.d",reserved_rm,4,output,sizeof(output),&used)==MULIB_ARCHML_RANGE);
    }
    mulib_archml_destroy(a);
  }
  puts("ArchML: schema, ownership, profiles and independent encoding fixtures OK");
  return 0;
}
