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

static void operations (void)
{
#define OP_TARGET "(archml 1 t (target t (bits 32) (endian little))"
#define OP_VALUE "(v (kind value) (bits 32) (signed false) (scale 1) (access write))"
#define OP_WRITE "(pcode-op write (arity 2 2) (semantics \"write virtual value\"))"
#define OP_CONST "(pcode-op const (arity 2 2) (semantics \"bitvector constant\"))"
#define OP_BODY "(pcode (write v (const 32 1)))"
  const char source[] = OP_TARGET OP_WRITE OP_CONST "(feature base)"
    "(operation x (requires (ref feature base)) (emission text) (operands " OP_VALUE ")" OP_BODY "))";
  mulib_archml_t *a = NULL;
  mulib_archml_error_t e;
  const char *features[] = {"base"};
  unsigned char out[32]; uint64_t v = 99; size_t length = 99;
  assert(mulib_archml_parse(source,sizeof(source)-1,NULL,&a,&e)==MULIB_ARCHML_OK);
  const mulib_archml_node_t *n = mulib_archml_find(a,"operation","x");
  assert(n && !mulib_archml_find(a,"instruction","x"));
  assert(mulib_archml_enabled(a,n,features,1)==1 && mulib_archml_enabled(a,n,NULL,0)==0);
  memset(out,0xa5,sizeof(out));
  assert(mulib_archml_encode(a,"x",&v,1,out,sizeof(out),&length)==MULIB_ARCHML_INVALID);
  assert(length==99 && out[0]==0xa5);
  assert(mulib_archml_decode(a,"x",out,sizeof(out),&v,1,&length)==MULIB_ARCHML_INVALID);
  assert(length==99 && v==99);
  mulib_archml_destroy(a);
  bad(OP_TARGET OP_WRITE OP_CONST "(operation x (operands " OP_VALUE ")" OP_BODY "))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET OP_WRITE OP_CONST "(operation x (emission bytes) (operands " OP_VALUE ")" OP_BODY "))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET OP_WRITE OP_CONST "(operation x (emission text extra) (operands " OP_VALUE ")" OP_BODY "))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET OP_WRITE OP_CONST "(operation x (emission text) (encoding (base \"00\")) (operands " OP_VALUE ")" OP_BODY "))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET OP_WRITE OP_CONST "(operation x (emission text) (operands " OP_VALUE OP_VALUE ")" OP_BODY "))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET OP_WRITE OP_CONST "(operation x (emission text) (operands " OP_VALUE ") (pcode (write absent (const 32 1)))))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET OP_WRITE OP_CONST "(operation x (emission text) (operands (v (kind value) (bits 32) (signed false) (scale 2) (access write)))" OP_BODY "))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET OP_WRITE OP_CONST "(operation x (emission text) (operands (v (kind value) (bits 32) (signed true) (scale 1) (access write)))" OP_BODY "))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET OP_WRITE OP_CONST "(operation x (emission text) (operands (v (kind value) (bits 32) (signed false) (scale 1) (access read)))" OP_BODY "))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET OP_WRITE OP_CONST "(instruction x (encoding (base \"00000000\") (field v 0 32 0)) (operands " OP_VALUE ")" OP_BODY "))",MULIB_ARCHML_SCHEMA);
  bad(OP_TARGET "(operation x (emission llvm-ir) (operands) (pcode (undeclared))))",MULIB_ARCHML_SCHEMA);
#undef OP_TARGET
#undef OP_VALUE
#undef OP_WRITE
#undef OP_CONST
#undef OP_BODY
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
  const char *names[] = {"amd64", "arm64", "riscv64", "ppc64", "mips", "avr32", "wasm",
    "amdgpu", "arm", "nvptx", "spirv", "xtensa", "bpf", "directx", "m68k", "hexagon", "dex"};
  const size_t concrete[] = {124,80,92,23,29,9,36,8,19,0,12,20,57,0,20,7,98};
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
  operations ();
  for (size_t i = 0; i < sizeof(names)/sizeof(*names); ++i) {
    mulib_archml_t *a = NULL;
    mulib_archml_error_t e;
    assert (snprintf(path,sizeof(path),"%s/%s.%s",directory,names[i],i<7 ? "aml" : "am") < (int)sizeof(path));
    int rc = mulib_archml_load(path,NULL,&a,&e);
    if (rc != MULIB_ARCHML_OK) fprintf(stderr,"%s:%zu:%zu %s\n",path,e.line,e.column,e.message);
    assert (rc == MULIB_ARCHML_OK && !strcmp(mulib_archml_name(a),names[i]));
    if (i < 3) assert (mulib_archml_count(a,"instruction") >= 60 && mulib_archml_count(a,"register") > 100);
    assert (mulib_archml_count(a,"instruction") == concrete[i]);
    const char *kinds[] = {"regclass","instruction","mode","memory","atomic","exception","vector","predication","feature","legalize","selection","schedule","rewrite","layout","syntax","extension","smt","abi"};
    if (i < 3) for(size_t k=0;k<sizeof(kinds)/sizeof(*kinds);++k) assert (mulib_archml_count(a,kinds[k]));
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
    } else if (i == 2) {
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
    if (i == 3) {
      const uint64_t rr[] = {3,4,5}, load[] = {3,0,(uint64_t)-8}, branch[] = {(uint64_t)-4};
      golden(a,"add",rr,3,"7c642a14");
      golden(a,"or",rr,3,"7c832b78");
      golden(a,"ld",load,3,"e860fff8");
      golden(a,"b",branch,1,"4bfffffc");
      uint64_t unaligned[] = {3,4,3}; unsigned char out[32]; size_t used=99;
      assert(mulib_archml_encode(a,"ld",unaligned,3,out,sizeof(out),&used)==MULIB_ARCHML_RANGE && used==99);
    } else if (i == 4) {
      const uint64_t rr[] = {2,4,5}, load[] = {2,29,(uint64_t)-8}, branch[] = {4,5,(uint64_t)-4};
      golden(a,"addu",rr,3,"00851021");
      golden(a,"lw",load,3,"8fa2fff8");
      golden(a,"beq",branch,3,"1085ffff");
      assert(mulib_archml_register_overlap(a,"zero","r0")==1);
      assert(!strcmp(mulib_archml_child(mulib_archml_property(mulib_archml_find(a,"register","r0"),"write"),1)->text,"discard"));
    } else if (i == 5) {
      /* AVR32 SLEIGH patterns, not 8-bit AVR assembler fixtures. */
      const uint64_t rr[] = {2,3}, imm[] = {12,(uint64_t)-1};
      golden(a,"add",rr,2,"0602");
      golden(a,"and",rr,2,"0662");
      golden(a,"or",rr,2,"0642");
      golden(a,"eor",rr,2,"0652");
      golden(a,"mov-rr",rr,2,"0692");
      golden(a,"com",rr,1,"5cd2");
      golden(a,"mov-ri",imm,2,"3ffc");
      golden(a,"nop",NULL,0,"d703");
      golden(a,"breakpoint",NULL,0,"d673");
      assert(mulib_archml_register_overlap(a,"sp","r13")==1);
      uint64_t pc[] = {15,3}; unsigned char out[32]; size_t used=99;
      assert(mulib_archml_encode(a,"add",pc,2,out,sizeof(out),&used)==MULIB_ARCHML_RANGE && used==99);
    } else if (i == 6) {
      const uint64_t minus64[] = {(uint64_t)-64}, plus63[] = {63}, too_large[] = {64};
      golden(a,"i32.add",NULL,0,"6a");
      golden(a,"i64.xor",NULL,0,"85");
      golden(a,"i32.const-sleb7",minus64,1,"4140");
      golden(a,"i64.const-sleb7",plus63,1,"423f");
      /* Direct byte positions in Capstone's WASM opcode table. */
      const char *stack_forms[] = {"clz","ctz","popcnt","add","sub","mul",
        "and","or","xor","shl","shr_s","shr_u","rotl","rotr","eqz"};
      const unsigned char opcodes[] = {0x67,0x68,0x69,0x6a,0x6b,0x6c,
        0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x45};
      for(size_t k=0;k<sizeof(opcodes);++k) {
        char form[64], hex[8];
        snprintf(form,sizeof(form),"i32.%s",stack_forms[k]);
        snprintf(hex,sizeof(hex),"%02x",opcodes[k]);
        golden(a,form,NULL,0,hex);
        snprintf(form,sizeof(form),"i64.%s",stack_forms[k]);
        snprintf(hex,sizeof(hex),"%02x",k==14 ? 0x50 : opcodes[k]+0x12);
        golden(a,form,NULL,0,hex);
      }
      golden(a,"nop",NULL,0,"01");
      golden(a,"unreachable",NULL,0,"00");
      golden(a,"drop",NULL,0,"1a");
      golden(a,"select",NULL,0,"1b");
      assert(mulib_archml_count(a,"register")==0);
      unsigned char out[32]; size_t used=99;
      assert(mulib_archml_encode(a,"i32.const-sleb7",too_large,1,out,sizeof(out),&used)==MULIB_ARCHML_RANGE && used==99);
      const unsigned char continuation[] = {0x41,0x80}; uint64_t v=99;
      assert(mulib_archml_decode(a,"i32.const-sleb7",continuation,2,&v,1,&used)==MULIB_ARCHML_MISMATCH && v==99 && used==99);
    }
    if (i == 7) {
      const uint64_t rr[] = {0,1,2}, move[] = {103,0};
      golden(a,"s_add_u32",rr,3,"01020080");
      golden(a,"s_sub_u32",rr,3,"01028080");
      golden(a,"s_and_b32",rr,3,"01020087");
      golden(a,"s_mov_b32",move,2,"0003e7be");
      golden(a,"s_nop0",NULL,0,"000080bf");
      golden(a,"s_endpgm",NULL,0,"000081bf");
      unsigned char out[32]; size_t used=99; uint64_t literal[] = {0,255};
      assert(mulib_archml_encode(a,"s_mov_b32",literal,2,out,sizeof(out),&used)==MULIB_ARCHML_RANGE && used==99);
      const unsigned char hole[] = {104,3,128,190}; uint64_t decoded[2] = {99,99};
      assert(mulib_archml_decode(a,"s_mov_b32",hole,4,decoded,2,&used)==MULIB_ARCHML_MISMATCH && decoded[0]==99);
    } else if (i == 8) {
      const uint64_t rr[] = {0,1,2}, load[] = {3,13,16}, branch[] = {(uint64_t)-8};
      golden(a,"add-rr",rr,3,"020081e0");
      golden(a,"ldr",load,3,"10309de5");
      golden(a,"b",branch,1,"feffffea");
      golden(a,"nop",NULL,0,"00f020e3");
      assert(mulib_archml_register_overlap(a,"sp","r13")==1);
      uint64_t pc[] = {15,1,2}; unsigned char out[32]; size_t used=99;
      assert(mulib_archml_encode(a,"add-rr",pc,3,out,sizeof(out),&used)==MULIB_ARCHML_RANGE && used==99);
    } else if (i == 9 || i == 13) {
      assert(mulib_archml_count(a,"operation")== (i==9 ? 15 : 8));
      assert(mulib_archml_count(a,"register")==0 && mulib_archml_count(a,"regclass")==0);
      const char *features[] = {"base"};
      for(size_t k=0;k<mulib_archml_count(a,"operation");++k) {
        const mulib_archml_node_t *operation=mulib_archml_at(a,"operation",k);
        const char *name=mulib_archml_child(operation,1)->text;
        assert(mulib_archml_enabled(a,operation,features,1)==1);
        assert(mulib_archml_property(operation,"pcode") && !mulib_archml_property(operation,"encoding"));
        unsigned char out[32]; uint64_t decoded=99; size_t used=99;
        memset(out,0xa5,sizeof(out));
        assert(mulib_archml_encode(a,name,NULL,0,out,sizeof(out),&used)==MULIB_ARCHML_INVALID && used==99 && out[0]==0xa5);
        assert(mulib_archml_decode(a,name,out,sizeof(out),&decoded,1,&used)==MULIB_ARCHML_INVALID && used==99 && decoded==99);
      }
      if(i==13) {
        uint64_t opcode=0;
        assert(mulib_archml_u64(mulib_archml_child(mulib_archml_property(mulib_archml_find(a,"operation","threadId.i32"),"dxil-opcode"),1),&opcode) && opcode==93);
      }
    } else if (i == 10) {
      const uint64_t ids[] = {1,2,3,4};
      golden(a,"OpIAdd",ids,4,"8000050001000000020000000300000004000000");
      golden(a,"OpBitwiseAnd",ids,4,"c700050001000000020000000300000004000000");
      golden(a,"OpReturn",NULL,0,"fd000100");
      assert(mulib_archml_count(a,"register")==0);
      uint64_t zero_id[] = {1,0,3,4}; unsigned char out[32]; size_t used=99;
      assert(mulib_archml_encode(a,"OpIAdd",zero_id,4,out,sizeof(out),&used)==MULIB_ARCHML_RANGE && used==99);
    } else if (i == 11) {
      /* Xtensa architectural RRR/RRI8 source-derived byte fixtures. */
      const uint64_t rr[] = {2,3,4}, imm[] = {2,3,(uint64_t)-1}, movi[] = {15,(uint64_t)-2048}, load[] = {2,3,1020};
      golden(a,"add",rr,3,"402380");
      golden(a,"sub",rr,3,"4023c0");
      golden(a,"xor",rr,3,"402330");
      golden(a,"addi",imm,3,"22c3ff");
      golden(a,"movi",movi,2,"f2a800");
      golden(a,"l32i",load,3,"2223ff");
      assert(mulib_archml_register_overlap(a,"a1","sp")==1);
      unsigned char out[32]; size_t used=99; uint64_t unaligned[] = {2,3,3};
      assert(mulib_archml_encode(a,"l32i",unaligned,3,out,sizeof(out),&used)==MULIB_ARCHML_RANGE && used==99);
    } else if (i == 12) {
      const uint64_t rr[] = {1,2}, imm[] = {1,(uint64_t)-1}, lddw[] = {1,UINT64_C(0x1122334455667788)}, load[] = {3,10,(uint64_t)-8}, branch[] = {1,2,(uint64_t)-8};
      golden(a,"add64-rr",rr,2,"0f21000000000000");
      golden(a,"mov64-ri",imm,2,"b7010000ffffffff");
      golden(a,"lddw",lddw,2,"18010000887766550000000044332211");
      golden(a,"ldx64",load,3,"79a3f8ff00000000");
      golden(a,"jeq-rr",branch,3,"1d21ffff00000000");
      golden(a,"exit",NULL,0,"9500000000000000");
      assert(mulib_archml_register_overlap(a,"r1","w1")==1);
      uint64_t frame[] = {10,1}, shift[] = {1,64}; unsigned char out[32]; size_t used=99;
      assert(mulib_archml_encode(a,"mov64-rr",frame,2,out,sizeof(out),&used)==MULIB_ARCHML_RANGE && used==99);
      assert(mulib_archml_encode(a,"lsh64-ri",shift,2,out,sizeof(out),&used)==MULIB_ARCHML_RANGE && used==99);
    } else if (i == 14) {
      /* M68k TD register-direct opcode/mode fields; no local MC target. */
      const uint64_t rr[] = {2,3};
      golden(a,"add32",rr,2,"d483");
      golden(a,"sub16",rr,2,"9443");
      golden(a,"eor8",rr,2,"b702");
      golden(a,"move32",rr,2,"2403");
      golden(a,"nop",NULL,0,"4e71");
      golden(a,"rts",NULL,0,"4e75");
      assert(mulib_archml_register_overlap(a,"d2","d2w")==1);
      assert(mulib_archml_register_overlap(a,"d2b","d3b")==0);
      assert(mulib_archml_register_overlap(a,"sp","a7")==1);
    } else if (i == 15) {
      const uint64_t rr[] = {0,1,2};
      golden(a,"add",rr,3,"00c201f3");
      golden(a,"sub",rr,3,"00c221f3");
      golden(a,"tfr",rr,2,"00c06170");
      golden(a,"nop",NULL,0,"00c0007f");
      const unsigned char not_packet_end[] = {0,0,0,0x7f}; size_t used=99;
      assert(mulib_archml_decode(a,"nop",not_packet_end,4,NULL,0,&used)==MULIB_ARCHML_MISMATCH && used==99);
    } else if (i == 16) {
      const uint64_t rr[] = {0,1,2}, constant[] = {3,(uint64_t)-1}, jump[] = {(uint64_t)-4};
      const uint64_t move[] = {256,65534}, wide[] = {2,UINT64_C(0x1122334455667788)};
      golden(a,"add-int",rr,3,"90000102");
      golden(a,"const/4",constant,2,"12f3");
      golden(a,"goto/16",jump,1,"2900feff");
      golden(a,"move/16",move,2,"03000001feff");
      golden(a,"const-wide",wide,2,"18028877665544332211");
      golden(a,"return-void",NULL,0,"0e00");
      assert(mulib_archml_count(a,"register")==0);
      assert(mulib_archml_find(a,"abi","dex-frame") && mulib_archml_find(a,"extension","art-gc"));
    }
    mulib_archml_destroy(a);
  }
  puts("ArchML: schema, ownership, profiles and independent encoding fixtures OK");
  return 0;
}
