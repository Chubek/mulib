#define MULIB_IMPL_MIR
#define MULIB_IMPL_GSSAIR
#define MULIB_IMPL_CLICKIR
#define MULIB_IMPL_ANFIR
#define MULIB_IMPL_SUPERBLOCKS
#define MULIB_IMPL_DEPIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#define MULIB_IMPL_ISCHED
#define MULIB_IMPL_LOWOPT
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_MICROCC
#include "mulib-microcc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define N MULIB_MIR_NONE
#define OP(op,d,a,b,t,v) {MULIB_MIR_NATIVE_##op,d,a,b,t,v}
#define NODE(op,type,block,a,b,v,m) {MULIB_MIR_NATIVE_##op,MULIB_MIR_##type,block,a,b,v,m}
static const mulib_mir_type_t int_params[] = {MULIB_MIR_I64};
static const mulib_gssair_node_t loop_nodes[] = {
  NODE(PARAM,I64,0,N,N,0,N), NODE(CONST,I64,0,N,N,0,N),
  NODE(CONST,I64,0,N,N,1,N), NODE(CONST,I64,0,N,N,11,N), NODE(CONST,I64,0,N,N,22,N),
  {MULIB_GSSAIR_PHI,MULIB_MIR_I64,1,0,2,0,N},
  {MULIB_GSSAIR_PHI,MULIB_MIR_I64,1,2,2,0,N},
  {MULIB_GSSAIR_PHI,MULIB_MIR_I64,1,4,2,0,N},
  NODE(ULT,BOOL,1,5,0,0,N), NODE(ADD,I64,2,5,2,0,N)
};
static const mulib_gssair_input_t loop_inputs[] = {{0,1},{2,9},{0,3},{2,7},{0,4},{2,6}};
static const mulib_gssair_block_t loop_blocks[] = {
  {MULIB_MIR_NATIVE_JUMP,N,1,N}, {MULIB_MIR_NATIVE_BRANCH,8,2,3},
  {MULIB_MIR_NATIVE_JUMP,N,1,N}, {MULIB_MIR_NATIVE_RETURN,6,N,N}
};
static const mulib_gssair_program_t loop = {
  loop_nodes,10,loop_blocks,4,loop_inputs,6,NULL,0,{int_params,1,MULIB_MIR_I64},NULL,0
};
static const mulib_mir_type_t bool_params[] = {MULIB_MIR_BOOL};
static const mulib_gssair_node_t diamond_nodes[] = {
  NODE(PARAM,BOOL,0,N,N,0,N), NODE(CONST,I64,0,N,N,10,N),
  NODE(CONST,I64,1,N,N,20,N), {MULIB_GSSAIR_PHI,MULIB_MIR_I64,2,0,2,0,N}
};
static const mulib_gssair_input_t diamond_inputs[] = {{0,1},{1,2}};
static const mulib_gssair_block_t diamond_blocks[] = {
  {MULIB_MIR_NATIVE_BRANCH,0,2,1}, {MULIB_MIR_NATIVE_JUMP,N,2,N}, {MULIB_MIR_NATIVE_RETURN,3,N,N}
};
static const mulib_gssair_program_t diamond = {
  diamond_nodes,4,diamond_blocks,3,diamond_inputs,2,NULL,0,{bool_params,1,MULIB_MIR_I64},NULL,0
};
/* A small independent executor checks control/copy lowering and scheduling.
 * Arithmetic reference functions in the assembled driver provide separate
 * boundary coverage of native operation semantics. */
static uint64_t run(const mulib_mir_native_program_t *p, const size_t *order, const uint64_t *args) {
  uint64_t *regs = calloc(p->register_count,sizeof(*regs)), answer = 0;
  size_t block=0, steps=0;
  int returned=0;
  assert(regs || !p->register_count);
  assert(mulib_mir_native_validate(p,65536,NULL)==MULIB_MIR_OK);
  while(!returned && steps++<10000) {
    size_t pos;
    for(pos=p->blocks[block].first;pos<p->blocks[block].first+p->blocks[block].count;++pos) {
      const mulib_mir_native_instruction_t *i=&p->code[order?order[pos]:pos];
      switch(i->opcode) {
      case MULIB_MIR_NATIVE_NOP: break;
      case MULIB_MIR_NATIVE_CONST: regs[i->destination]=i->immediate; break;
      case MULIB_MIR_NATIVE_PARAM: regs[i->destination]=args[i->a]; break;
      case MULIB_MIR_NATIVE_COPY: regs[i->destination]=regs[i->a]; break;
      case MULIB_MIR_NATIVE_JUMP: block=i->target; goto next;
      case MULIB_MIR_NATIVE_BRANCH: block=regs[i->a]?i->target:i->b; goto next;
      case MULIB_MIR_NATIVE_RETURN: answer=regs[i->a]; returned=1; goto next;
      default: assert(mulib_lowopt_evaluate(i->opcode,regs[i->a],regs[i->b],&regs[i->destination])); break;
      }
    }
    assert(0);
  next: ;
  }
  assert(returned); free(regs); return answer;
}
static int output(void *user,const char *bytes,size_t length) {
  return fwrite(bytes,1,length,(FILE *)user)==length;
}
static int reject(void *user,const char *bytes,size_t length) {
  (void)user;(void)bytes;(void)length;return 0;
}
static void compile_check(mulib_microcc_ir_t kind,const void *p,const char *name,
                          const mulib_machine_t *target,FILE *file) {
  mulib_microcc_source_t source={kind,p};
  mulib_microcc_options_t options=mulib_microcc_defaults();
  mulib_microcc_result_t result={0};
  mulib_microcc_diagnostic_t d;
  options.machine=target;
  if(mulib_microcc_compile(&source,name,&options,&result,&d)!=MULIB_MIR_OK) {
    fprintf(stderr,"%s: %s: %s at %zu\n",name,mulib_microcc_stage_string(d.stage),mulib_mir_status_string(d.mir.status),d.mir.record);
    assert(0);
  }
  assert(result.assembly && result.assembly_length==strlen(result.assembly));
  assert(mulib_microcc_write(&result,reject,NULL)==MULIB_MIR_IO_ERROR);
  if(file) assert(mulib_microcc_write(&result,output,file)==MULIB_MIR_OK);
  mulib_microcc_free(&result); assert(!result.mir.code && !result.assembly);
}
static void test_ir(void) {
  mulib_mir_diagnostic_t d;
  mulib_mir_native_buffer_t lowered={0};
  mulib_gssair_buffer_t shuffled={0}, normalized={0}, derived={0};
  mulib_anfir_buffer_t anf={0};
  mulib_superblocks_buffer_t super={0};
  mulib_depir_buffer_t dep={0};
  mulib_clickir_buffer_t sea={0};
  mulib_clickir_program_t click;
  size_t reverse[]={9,8,7,6,5,4,3,2,1,0};
  uint64_t arg;
  assert(mulib_gssair_validate(&loop,256,&d)==MULIB_MIR_OK);
  assert(mulib_gssair_validate(&loop,15,&d)==MULIB_MIR_RESOURCE_LIMIT);
  assert(mulib_gssair_lower(&loop,24,&lowered,&d)==MULIB_MIR_RESOURCE_LIMIT && !lowered.code);
  assert(mulib_gssair_lower(&loop,256,&lowered,&d)==MULIB_MIR_OK);
  assert(lowered.block_count==6 && lowered.register_count==13);
  for(arg=0;arg<20;++arg) { mulib_mir_native_program_t p=mulib_mir_native_view(&lowered); assert(run(&p,NULL,&arg)==(arg%2?22:11)); }
  mulib_mir_native_free(&lowered);
  assert(mulib_gssair_permute(&loop,reverse,256,&shuffled,&d)==MULIB_MIR_OK);
  assert(mulib_gssair_validate(&shuffled.program,256,&d)==MULIB_MIR_BAD_REFERENCE);
  click.graph=shuffled.program;click.edges=NULL;click.edge_count=0;
  assert(mulib_clickir_to_gssair(&click,256,&normalized,&d)==MULIB_MIR_OK);
  assert(mulib_anfir_from_gssair(&normalized.program,256,&anf,&d)==MULIB_MIR_OK);
  assert(mulib_superblocks_build(&normalized.program,256,&super,&d)==MULIB_MIR_OK);
  assert(mulib_superblocks_validate(&super.program,256,&d)==MULIB_MIR_OK);
  assert(mulib_depir_from_gssair(&normalized.program,256,&dep,&d)==MULIB_MIR_OK);
  assert(dep.program.entity_count>loop.node_count && dep.program.edge_count>loop.node_count);
  assert(mulib_depir_to_clickir(&dep.program,256,&sea,&d)==MULIB_MIR_OK);
  assert(mulib_depir_to_gssair(&dep.program,256,&derived,&d)==MULIB_MIR_OK);
  mulib_gssair_free(&derived);
  {
    mulib_anfir_buffer_t a={0};mulib_superblocks_buffer_t s={0};
    assert(mulib_depir_to_anfir(&dep.program,256,&a,&d)==MULIB_MIR_OK);
    assert(mulib_depir_to_superblocks(&dep.program,256,&s,&d)==MULIB_MIR_OK);
    mulib_anfir_free(&a);mulib_superblocks_free(&s);
  }
  {
    mulib_microcc_source_t sources[]={
      {MULIB_MICROCC_GSSAIR,&loop},{MULIB_MICROCC_CLICKIR,&click},
      {MULIB_MICROCC_ANFIR,&anf.program},{MULIB_MICROCC_SUPERBLOCKS,&super.program},
      {MULIB_MICROCC_DEPIR,&dep.program},{MULIB_MICROCC_CLICKIR,&sea.program}
    };
    size_t i;unsigned flags;
    for(i=0;i<sizeof(sources)/sizeof(sources[0]);++i) for(flags=0;flags<4;++flags) {
      mulib_microcc_result_t r={0};mulib_microcc_options_t o=mulib_microcc_defaults();
      mulib_mir_native_program_t p;o.flags=flags;
      assert(mulib_microcc_compile(&sources[i],"check_loop",&o,&r,NULL)==MULIB_MIR_OK);
      p=mulib_mir_native_view(&r.mir);
      for(arg=0;arg<10;++arg) assert(run(&p,r.order,&arg)==(arg%2?22:11));
      mulib_microcc_free(&r);
    }
  }
  mulib_clickir_free(&sea);mulib_depir_free(&dep);mulib_superblocks_free(&super);
  mulib_anfir_free(&anf);mulib_gssair_free(&normalized);mulib_gssair_free(&shuffled);
  assert(mulib_gssair_lower(&diamond,256,&lowered,&d)==MULIB_MIR_OK);
  assert(lowered.block_count==5);
  for(arg=0;arg<2;++arg) { mulib_mir_native_program_t p=mulib_mir_native_view(&lowered); assert(run(&p,NULL,&arg)==(arg?10:20)); }
  mulib_mir_native_free(&lowered);
}
static void test_invalid(void) {
  mulib_gssair_node_t nodes[10];mulib_gssair_input_t inputs[6];mulib_gssair_block_t blocks[4];
  mulib_gssair_program_t p=loop;mulib_gssair_buffer_t out={0};mulib_mir_diagnostic_t d;
  mulib_clickir_program_t click;mulib_clickir_edge_t edges[]={{0,1},{1,0}};
  memcpy(nodes,loop_nodes,sizeof(nodes));memcpy(inputs,loop_inputs,sizeof(inputs));memcpy(blocks,loop_blocks,sizeof(blocks));
  p.nodes=nodes;p.inputs=inputs;p.blocks=blocks;
  assert(mulib_gssair_validate(NULL,100,&d)==MULIB_MIR_INVALID);
  p.node_count=0;assert(mulib_gssair_validate(&p,100,&d)==MULIB_MIR_INVALID);p.node_count=10;
  nodes[9].a=10;assert(mulib_gssair_validate(&p,100,&d)==MULIB_MIR_BAD_REFERENCE);nodes[9]=loop_nodes[9];
  nodes[8].a=9;assert(mulib_gssair_validate(&p,100,&d)==MULIB_MIR_BAD_REFERENCE);nodes[8]=loop_nodes[8];
  nodes[8].type=MULIB_MIR_I64;assert(mulib_gssair_validate(&p,100,&d)==MULIB_MIR_BAD_TYPE);nodes[8]=loop_nodes[8];
  inputs[1].predecessor=0;assert(mulib_gssair_validate(&p,100,&d)==MULIB_MIR_BAD_REFERENCE);inputs[1]=loop_inputs[1];
  nodes[5].b=1;assert(mulib_gssair_validate(&p,100,&d)==MULIB_MIR_BAD_REFERENCE);nodes[5]=loop_nodes[5];
  blocks[2].yes=0;assert(mulib_gssair_validate(&p,100,&d)==MULIB_MIR_BAD_REFERENCE);blocks[2]=loop_blocks[2];
  blocks[1].no=2;assert(mulib_gssair_validate(&p,100,&d)==MULIB_MIR_BAD_REFERENCE);blocks[1]=loop_blocks[1];
  click.graph=p;click.edges=edges;click.edge_count=2;
  assert(mulib_clickir_to_gssair(&click,100,&out,&d)==MULIB_MIR_BAD_REFERENCE && !out.nodes);
  edges[0].after=100;assert(mulib_clickir_to_gssair(&click,100,&out,&d)==MULIB_MIR_BAD_REFERENCE);
  {
    mulib_mir_native_buffer_t b={0};mulib_mir_native_program_t native;
    assert(mulib_gssair_lower(&loop,100,&b,&d)==MULIB_MIR_OK);
    native=mulib_mir_native_view(&b);b.code[0].destination=100;
    assert(mulib_mir_native_validate(&native,100,&d)==MULIB_MIR_BAD_REFERENCE);
    b.code[0].destination=0;b.blocks[0].count=0;
    assert(mulib_mir_native_validate(&native,100,&d)==MULIB_MIR_BAD_REFERENCE);
    mulib_mir_native_free(&b);
    assert(mulib_mir_native_init(&b,SIZE_MAX,1,1,0,0,0,SIZE_MAX)==MULIB_MIR_RESOURCE_LIMIT);
  }
  {
    mulib_microcc_source_t source={MULIB_MICROCC_GSSAIR,&loop};
    mulib_microcc_result_t r={0};mulib_microcc_options_t o=mulib_microcc_defaults();mulib_microcc_diagnostic_t md;
    assert(mulib_microcc_compile(&source,"bad;name",&o,&r,&md)==MULIB_MIR_INVALID && !r.assembly);
    o.text_limit=8;assert(mulib_microcc_compile(&source,"f",&o,&r,&md)==MULIB_MIR_RESOURCE_LIMIT && md.stage==MULIB_MICROCC_ASSEMBLY && !r.mir.code);
    o=mulib_microcc_defaults();o.edge_limit=0;
    assert(mulib_microcc_compile(&source,"f",&o,&r,&md)==MULIB_MIR_RESOURCE_LIMIT && md.stage==MULIB_MICROCC_SCHEDULER);
    o=mulib_microcc_defaults();o.cycle_limit=0;
    assert(mulib_microcc_compile(&source,"f",&o,&r,&md)==MULIB_MIR_RESOURCE_LIMIT);
    o=mulib_microcc_defaults();o.limit=0;assert(mulib_microcc_compile(&source,"f",&o,&r,&md)==MULIB_MIR_RESOURCE_LIMIT);
    assert(mulib_microcc_compile(&source,"f",NULL,&r,&md)==MULIB_MIR_OK);
    { char *text=r.assembly;assert(mulib_microcc_compile(&source,"f",NULL,&r,&md)==MULIB_MIR_INVALID && r.assembly==text); }
    mulib_microcc_free(&r);
  }
}
static void test_selection(void) {
  mulib_machine_selection_t a,b;
  mulib_mir_native_instruction_t i=mulib_mir_native_instruction(MULIB_MIR_NATIVE_ADD);
  uint64_t values[]={0,17};const mulib_machine_t *m=mulib_machine_x86_64_sysv();
  mulib_machine_t custom=*m;mulib_muburg_rule_t *rules=malloc(m->rule_count*sizeof(*rules));size_t k;
  assert(rules);memcpy(rules,m->rules,m->rule_count*sizeof(*rules));custom.rules=rules;
  assert(mulib_machine_select(m,&i,0,NULL,100,&a)==MULIB_MIR_OK && !a.immediate_mask);
  assert(mulib_machine_select(m,&i,2,values,100,&b)==MULIB_MIR_OK && b.immediate_mask==2 && b.cost<a.cost);
  for(k=0;k<custom.rule_count;++k) if(rules[k].opcode==MULIB_MIR_NATIVE_ADD && rules[k].child_goal[1]==1) rules[k].cost=100;
  assert(mulib_machine_select(&custom,&i,2,values,100,&b)==MULIB_MIR_OK && !b.immediate_mask);
  values[1]=UINT64_C(1)<<40;assert(mulib_machine_select(m,&i,2,values,100,&b)==MULIB_MIR_OK && !b.immediate_mask);
  values[1]=UINT64_MAX;assert(mulib_machine_select(m,&i,2,values,100,&b)==MULIB_MIR_OK && b.immediate_mask==2);
  values[1]=2048;assert(mulib_machine_select(mulib_machine_rv64im(),&i,2,values,100,&b)==MULIB_MIR_OK && !b.immediate_mask);
  values[1]=UINT64_MAX-2047;assert(mulib_machine_select(mulib_machine_rv64im(),&i,2,values,100,&b)==MULIB_MIR_OK && b.immediate_mask==2);
  for(k=0;k<custom.rule_count;++k) if(rules[k].opcode==MULIB_MIR_NATIVE_ADD) rules[k].opcode=MULIB_MIR_NATIVE_SUB;
  assert(mulib_machine_select(&custom,&i,0,NULL,100,&b)==MULIB_MIR_UNSUPPORTED);
  custom.word_bits=32;assert(mulib_machine_validate(&custom,100)==MULIB_MIR_INVALID);free(rules);
}
static void test_literals(void) {
  const mulib_gssair_block_t block={MULIB_MIR_NATIVE_RETURN,1,N,N};
  mulib_anfir_binding_t bindings[2]={0};mulib_anfir_program_t p={0};mulib_gssair_buffer_t out={0};
  bindings[0].value=(mulib_gssair_node_t)NODE(PARAM,I64,0,N,N,0,N);
  bindings[1].value=(mulib_gssair_node_t)NODE(ADD,I64,0,0,N,0,N);
  bindings[1].literal_mask=2;bindings[1].literals[1]=7;bindings[1].literal_types[1]=MULIB_MIR_I64;
  p.bindings=bindings;p.graph.node_count=2;p.graph.blocks=&block;p.graph.block_count=1;
  p.graph.signature=(mulib_mir_signature_t){int_params,1,MULIB_MIR_I64};
  assert(mulib_anfir_to_gssair(&p,100,&out,NULL)==MULIB_MIR_OK && out.program.node_count==3);
  assert(out.nodes[2].b==1 && out.blocks[0].value==2);mulib_gssair_free(&out);
  bindings[1].literal_mask=4;assert(mulib_anfir_to_gssair(&p,100,&out,NULL)==MULIB_MIR_INVALID);
}
static uint64_t random_word(uint64_t *state) {
  *state=*state*UINT64_C(6364136223846793005)+UINT64_C(1442695040888963407);return *state;
}
static void test_mutable_schedules(void) {
  uint64_t state=UINT64_C(0x1234567887654321);size_t trial,k;int reordered=0;
  for(trial=0;trial<40;++trial) {
    mulib_mir_native_instruction_t code[40],original[40];
    const mulib_mir_native_block_t block={0,40};
    const mulib_mir_native_program_t p={code,40,&block,1,8,3,NULL,0,NULL,0};
    mulib_microcc_source_t source={MULIB_MICROCC_NATIVE,&p};
    uint64_t args[]={random_word(&state),random_word(&state),random_word(&state)},expected;
    unsigned flags;
    for(k=0;k<39;++k) {
      mulib_mir_native_opcode_t op=k<3?MULIB_MIR_NATIVE_PARAM:
        (mulib_mir_native_opcode_t)(MULIB_MIR_NATIVE_COPY+random_word(&state)%13);
      code[k]=mulib_mir_native_instruction(op);
      code[k].destination=k<3?k:(size_t)((random_word(&state)>>32)%8);
      if(k<3)code[k].a=k;
      else {
        code[k].a=(size_t)((random_word(&state)>>32)%8);
        if(mulib_mir_native_arity(op)==2)code[k].b=(size_t)((random_word(&state)>>32)%8);
        if(k%7==0) {
          code[k].opcode=MULIB_MIR_NATIVE_CONST;code[k].a=code[k].b=N;
          code[k].immediate=k%2?random_word(&state):0;
        }
      }
    }
    code[39]=(mulib_mir_native_instruction_t)OP(RETURN,N,0,N,N,0);
    memcpy(original,code,sizeof(code));expected=run(&p,NULL,args);
    for(flags=0;flags<4;++flags) {
      mulib_microcc_options_t o=mulib_microcc_defaults();mulib_microcc_result_t r={0};mulib_mir_native_program_t q;
      o.flags=flags;assert(mulib_microcc_compile(&source,"randomized",&o,&r,NULL)==MULIB_MIR_OK);
      q=mulib_mir_native_view(&r.mir);assert(run(&q,r.order,args)==expected);
      assert(!memcmp(original,code,sizeof(code)));
      for(k=0;k<r.mir.count;++k)if(r.order[k]!=k)reordered=1;
      mulib_microcc_free(&r);
    }
  }
  assert(reordered);
}
static mulib_mir_status_t fail_emitter(const mulib_machine_t *m,const mulib_mir_native_program_t *p,
    const mulib_machine_selection_t *selected,const size_t *order,const char *name,
    mulib_mir_write_fn writer,void *user,mulib_mir_diagnostic_t *d) {
  (void)m;(void)p;(void)selected;(void)order;(void)name;(void)d;
  assert(writer(user,"partial",7));return MULIB_MIR_UNSUPPORTED;
}
static void test_edges(void) {
  mulib_microcc_result_t r={0};mulib_microcc_options_t o=mulib_microcc_defaults();
  mulib_microcc_source_t source={MULIB_MICROCC_GSSAIR,&diamond};mulib_microcc_diagnostic_t d;
  mulib_machine_t custom=*o.machine;char symbol[129];
  custom.emit=fail_emitter;o.machine=&custom;
  assert(mulib_microcc_compile(&source,"partial",&o,&r,&d)==MULIB_MIR_UNSUPPORTED);
  assert(!r.assembly && !r.mir.code && d.stage==MULIB_MICROCC_ASSEMBLY);
  custom=*mulib_machine_x86_64_sysv();custom.signed_immediate_bits=64;
  assert(mulib_microcc_compile(&source,"bad_target",&o,&r,&d)==MULIB_MIR_UNSUPPORTED && !r.assembly);
  o=mulib_microcc_defaults();memset(symbol,'a',127);symbol[127]=0;
  assert(mulib_microcc_compile(&source,symbol,&o,&r,&d)==MULIB_MIR_OK);mulib_microcc_free(&r);
  symbol[127]='a';symbol[128]=0;
  assert(mulib_microcc_compile(&source,symbol,&o,&r,&d)==MULIB_MIR_INVALID);
  assert(mulib_microcc_compile(NULL,"f",&o,&r,&d)==MULIB_MIR_INVALID);
  {
    char callee[]="owned_callee";const char *symbols[]={callee};
    const mulib_mir_native_instruction_t code[]={OP(CALL,0,0,0,0,0),OP(RETURN,N,0,N,N,0)};
    const mulib_mir_native_block_t block={0,2};
    const mulib_mir_native_program_t p={code,2,&block,1,1,0,NULL,0,symbols,1};
    source.kind=MULIB_MICROCC_NATIVE;source.program=&p;
    assert(mulib_microcc_compile(&source,"owned",&o,&r,&d)==MULIB_MIR_OK);
    callee[0]='x';assert(!strcmp(r.mir.symbols[0],"owned_callee") && strstr(r.assembly,"owned_callee"));
    assert(mulib_mir_native_symbol(&r.mir,0,"bad name")==MULIB_MIR_INVALID);
    assert(!strcmp(r.mir.symbols[0],"owned_callee"));mulib_microcc_free(&r);
  }
  {
    const mulib_mir_native_instruction_t forever=OP(JUMP,N,N,N,0,0);
    const mulib_mir_native_block_t block={0,1};
    mulib_mir_native_program_t p={&forever,1,&block,1,0,0,NULL,0,NULL,0};
    source.kind=MULIB_MICROCC_NATIVE;source.program=&p;
    assert(mulib_microcc_compile(&source,"forever",&o,&r,&d)==MULIB_MIR_OK);mulib_microcc_free(&r);
    p.count=0;assert(mulib_microcc_compile(&source,"empty",&o,&r,&d)==MULIB_MIR_INVALID);
  }
  {
    mulib_superblocks_buffer_t s={0};mulib_gssair_buffer_t out={0};size_t saved;
    assert(mulib_superblocks_build(&loop,256,&s,NULL)==MULIB_MIR_OK);
    saved=s.order[1];s.order[1]=s.order[0];
    assert(mulib_superblocks_validate(&s.program,256,NULL)==MULIB_MIR_BAD_REFERENCE);s.order[1]=saved;
    saved=s.starts[1];s.starts[1]=0;
    assert(mulib_superblocks_to_gssair(&s.program,256,&out,NULL)==MULIB_MIR_BAD_REFERENCE && !out.nodes);
    s.starts[1]=saved;mulib_superblocks_free(&s);
  }
  {
    mulib_depir_buffer_t dep={0};mulib_gssair_buffer_t out={0};size_t saved;
    assert(mulib_depir_from_gssair(&loop,256,&dep,NULL)==MULIB_MIR_OK);
    saved=dep.edges[0].after;dep.edges[0].after=dep.program.entity_count;
    assert(mulib_depir_to_gssair(&dep.program,256,&out,NULL)==MULIB_MIR_BAD_REFERENCE && !out.nodes);
    dep.edges[0].after=saved;mulib_depir_free(&dep);
  }
}
static void test_mutations(void) {
  uint64_t state=UINT64_C(0xf00d);size_t trial;
  for(trial=0;trial<500;++trial) {
    mulib_gssair_node_t nodes[10];mulib_gssair_program_t p=loop;
    mulib_mir_native_buffer_t native={0};mulib_gssair_buffer_t sorted={0};
    mulib_clickir_program_t c;mulib_mir_status_t status;size_t n=(size_t)(random_word(&state)%10);
    size_t value=(size_t)(random_word(&state)%14);
    memcpy(nodes,loop_nodes,sizeof(nodes));p.nodes=nodes;if(value==13)value=N;
    switch(trial%7) {
    case 0:nodes[n].a=value;break;
    case 1:nodes[n].b=value;break;
    case 2:nodes[n].memory=value;break;
    case 3:nodes[n].block=value;break;
    case 4:nodes[n].opcode=(int)(random_word(&state)%32)-1;break;
    case 5:nodes[n].type=(mulib_mir_type_t)(random_word(&state)%7);break;
    default:nodes[n].immediate=random_word(&state);break;
    }
    status=mulib_gssair_lower(&p,256,&native,NULL);
    if(status==MULIB_MIR_OK) {
      mulib_mir_native_program_t v=mulib_mir_native_view(&native);
      assert(mulib_mir_native_validate(&v,256,NULL)==MULIB_MIR_OK);
    } else assert(!native.code);
    mulib_mir_native_free(&native);
    c.graph=p;c.edges=NULL;c.edge_count=0;
    status=mulib_clickir_to_gssair(&c,256,&sorted,NULL);
    if(status==MULIB_MIR_OK)assert(mulib_gssair_validate(&sorted.program,256,NULL)==MULIB_MIR_OK);
    else assert(!sorted.nodes);
    mulib_gssair_free(&sorted);
  }
}
static void emit_all(const mulib_machine_t *machine,FILE *file) {
  static const char *const names[]={"add","sub","mul","and","or","xor","shl","shr","sar","eq","slt","ult"};
  mulib_mir_native_instruction_t code[]={OP(PARAM,0,0,N,N,0),OP(PARAM,1,1,N,N,0),OP(ADD,2,0,1,N,0),OP(RETURN,N,2,N,N,0)};
  const mulib_mir_native_block_t block={0,4};
  mulib_mir_native_program_t p={code,4,&block,1,3,2,NULL,0,NULL,0};
  int op;char name[64];
  for(op=MULIB_MIR_NATIVE_ADD;op<=MULIB_MIR_NATIVE_ULT;++op) {
    code[2].opcode=(mulib_mir_native_opcode_t)op;
    (void)snprintf(name,sizeof(name),"microcc_%s",names[op-MULIB_MIR_NATIVE_ADD]);
    compile_check(MULIB_MICROCC_NATIVE,&p,name,machine,file);
  }
  code[1]=mulib_mir_native_instruction(MULIB_MIR_NATIVE_CONST);
  code[1].destination=1;code[1].immediate=UINT64_MAX-16;
  for(op=MULIB_MIR_NATIVE_ADD;op<=MULIB_MIR_NATIVE_ULT;++op) {
    code[2].opcode=(mulib_mir_native_opcode_t)op;
    (void)snprintf(name,sizeof(name),"microcc_%s_imm",names[op-MULIB_MIR_NATIVE_ADD]);
    compile_check(MULIB_MICROCC_NATIVE,&p,name,machine,file);
  }
  code[2].opcode=MULIB_MIR_NATIVE_ADD;code[1].immediate=UINT64_C(1)<<40;
  compile_check(MULIB_MICROCC_NATIVE,&p,"microcc_add_wide",machine,file);
  compile_check(MULIB_MICROCC_GSSAIR,&loop,"microcc_loop",machine,file);
  compile_check(MULIB_MICROCC_GSSAIR,&diamond,"microcc_diamond",machine,file);
  {
    mulib_anfir_buffer_t a={0};mulib_depir_buffer_t d={0};mulib_superblocks_buffer_t s={0};mulib_clickir_buffer_t c={0};
    assert(mulib_anfir_from_gssair(&loop,256,&a,NULL)==MULIB_MIR_OK);
    assert(mulib_depir_from_gssair(&loop,256,&d,NULL)==MULIB_MIR_OK);
    assert(mulib_superblocks_build(&loop,256,&s,NULL)==MULIB_MIR_OK);
    assert(mulib_clickir_from_gssair(&loop,256,&c,NULL)==MULIB_MIR_OK);
    compile_check(MULIB_MICROCC_ANFIR,&a.program,"microcc_anf",machine,file);
    compile_check(MULIB_MICROCC_DEPIR,&d.program,"microcc_dep",machine,file);
    compile_check(MULIB_MICROCC_SUPERBLOCKS,&s.program,"microcc_super",machine,file);
    compile_check(MULIB_MICROCC_CLICKIR,&c.program,"microcc_click",machine,file);
    mulib_anfir_free(&a);mulib_depir_free(&d);mulib_superblocks_free(&s);mulib_clickir_free(&c);
  }
  {
    mulib_mir_native_instruction_t hazard[]={OP(PARAM,0,0,N,N,0),OP(CONST,1,N,N,N,3),OP(ADD,2,0,1,N,0),
      OP(CONST,0,N,N,N,10),OP(MUL,1,0,2,N,0),OP(ADD,0,1,2,N,0),OP(RETURN,N,0,N,N,0)};
    const mulib_mir_native_block_t hb={0,7};mulib_mir_native_program_t hp={hazard,7,&hb,1,3,1,NULL,0,NULL,0};
    compile_check(MULIB_MICROCC_NATIVE,&hp,"microcc_hazards",machine,file);
  }
  {
    const mulib_mir_type_t types[]={MULIB_MIR_WORD,MULIB_MIR_I64};
    const mulib_gssair_function_t funcs[]={{"microcc_mutate",{types,2,MULIB_MIR_I64}}};
    const size_t args[]={0,1};
    mulib_gssair_node_t nodes[]={NODE(PARAM,WORD,0,N,N,0,N),NODE(PARAM,I64,0,N,N,1,N),
      NODE(STORE,VOID,0,0,1,0,N),NODE(LOAD,I64,0,0,N,0,2),NODE(CALL,I64,0,0,2,0,3),
      NODE(LOAD,I64,0,0,N,0,4),NODE(ADD,I64,0,3,5,0,N),NODE(ADD,I64,0,6,4,0,N)};
    const mulib_gssair_block_t mb={MULIB_MIR_NATIVE_RETURN,7,N,N};
    mulib_gssair_program_t mp={nodes,8,&mb,1,NULL,0,args,2,{types,2,MULIB_MIR_I64},funcs,1};
    compile_check(MULIB_MICROCC_GSSAIR,&mp,"microcc_memory",machine,file);
    nodes[5].memory=3;assert(mulib_gssair_validate(&mp,100,NULL)==MULIB_MIR_BAD_REFERENCE);
  }
  {
    mulib_mir_native_instruction_t calls[14];size_t args[10],k;
    const char *const symbols[]={"microcc_ten"};const mulib_mir_native_block_t cb={0,14};
    mulib_mir_native_program_t cp={calls,14,&cb,1,12,10,args,10,symbols,1};
    for(k=0;k<10;++k) {calls[k]=mulib_mir_native_instruction(MULIB_MIR_NATIVE_PARAM);calls[k].destination=k;calls[k].a=k;args[k]=k;}
    calls[10]=(mulib_mir_native_instruction_t)OP(CALL,10,0,10,0,0);
    calls[11]=(mulib_mir_native_instruction_t)OP(PARAM,11,0,N,N,0);
    calls[12]=(mulib_mir_native_instruction_t)OP(ADD,10,10,11,N,0);
    calls[13]=(mulib_mir_native_instruction_t)OP(RETURN,N,10,N,N,0);
    compile_check(MULIB_MICROCC_NATIVE,&cp,"microcc_call",machine,file);
  }
  {
    const mulib_mir_native_instruction_t big[]={OP(PARAM,299,0,N,N,0),OP(RETURN,N,299,N,N,0)};
    const mulib_mir_native_block_t bb={0,2};mulib_mir_native_program_t bp={big,2,&bb,1,300,1,NULL,0,NULL,0};
    compile_check(MULIB_MICROCC_NATIVE,&bp,"microcc_bigframe",machine,file);
  }
  {
    mulib_mir_native_instruction_t far[305];size_t k;
    const mulib_mir_native_block_t blocks[]={{0,2},{2,301},{303,2}};
    const mulib_mir_native_program_t fp={far,305,blocks,3,1,1,NULL,0,NULL,0};
    far[0]=(mulib_mir_native_instruction_t)OP(PARAM,0,0,N,N,0);
    far[1]=(mulib_mir_native_instruction_t)OP(BRANCH,N,0,1,2,0);
    for(k=0;k<300;++k) far[k+2]=(mulib_mir_native_instruction_t)OP(CONST,0,N,N,N,UINT64_C(0x123456789abcdef0)+k);
    far[302]=(mulib_mir_native_instruction_t)OP(RETURN,N,0,N,N,0);
    far[303]=(mulib_mir_native_instruction_t)OP(CONST,0,N,N,N,42);
    far[304]=(mulib_mir_native_instruction_t)OP(RETURN,N,0,N,N,0);
    compile_check(MULIB_MICROCC_NATIVE,&fp,"microcc_far",machine,file);
  }
}
int main(int argc,char **argv) {
  test_ir();test_invalid();test_selection();test_literals();test_mutable_schedules();test_edges();test_mutations();
  if(argc==3 && !strcmp(argv[1],"--emit")) {
    const mulib_machine_t *target=!strcmp(argv[2],"rv64")?mulib_machine_rv64im():mulib_machine_x86_64_sysv();
    emit_all(target,stdout);
  } else {
    assert(argc==1);emit_all(mulib_machine_x86_64_sysv(),NULL);emit_all(mulib_machine_rv64im(),NULL);
  }
  return 0;
}
