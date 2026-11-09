#define MULIB_IMPL_MIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#define MULIB_IMPL_LIR
#define MULIB_IMPL_ISCHED
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_LOWOPT
#define MULIB_IMPL_ISEL
#define MULIB_IMPL_PEEPHOLE
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_SUPERINSTR
#define MULIB_IMPL_OPTML
#include "mulib-optml.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define N SIZE_MAX
#define OP(op,d,a,b,t,v) {MULIB_MIR_NATIVE_##op,d,a,b,t,v}
static uint64_t random_word (uint64_t *state)
{ *state ^= *state << 13; *state ^= *state >> 7; *state ^= *state << 17; return *state; }
/* Independent unsigned arithmetic oracle, including explicit signed ordering
 * and sign-bit replication (no optimizer evaluator calls). */
static uint64_t value (const mulib_mir_native_instruction_t *i, const uint64_t *r)
{
  uint64_t a = i->a == N ? 0 : r[i->a], b = i->b == N ? 0 : r[i->b];
  switch (i->opcode) {
  case MULIB_MIR_NATIVE_CONST: return i->immediate;
  case MULIB_MIR_NATIVE_COPY: return a;
  case MULIB_MIR_NATIVE_ADD: return a + b;
  case MULIB_MIR_NATIVE_SUB: return a - b;
  case MULIB_MIR_NATIVE_MUL: return a * b;
  case MULIB_MIR_NATIVE_AND: return a & b;
  case MULIB_MIR_NATIVE_OR: return a | b;
  case MULIB_MIR_NATIVE_XOR: return a ^ b;
  case MULIB_MIR_NATIVE_SHL: return a << (b % 64);
  case MULIB_MIR_NATIVE_SHR: return a >> (b % 64);
  case MULIB_MIR_NATIVE_SAR:
    for (unsigned k = 0; k < b % 64; ++k) a = (a >> 1) | (a & (UINT64_C(1) << 63));
    return a;
  case MULIB_MIR_NATIVE_EQ: return a == b;
  case MULIB_MIR_NATIVE_ULT: return a < b;
  case MULIB_MIR_NATIVE_SLT: return (a >> 63) != (b >> 63) ? (a >> 63) : a < b;
  default: assert (0); return 0;
  }
}
static uint64_t run (const mulib_mir_native_program_t *p, const size_t *order, const uint64_t *args)
{
  uint64_t *r = (uint64_t *)calloc (p->register_count, sizeof (*r)), result = 0;
  size_t block = 0, steps = 0;
  assert (r);
  for (;;) {
    assert (++steps < 10000);
    for (size_t j = p->blocks[block].first; j < p->blocks[block].first + p->blocks[block].count; ++j) {
      const mulib_mir_native_instruction_t *i = &p->code[order ? order[j] : j];
      switch (i->opcode) {
      case MULIB_MIR_NATIVE_NOP: break;
      case MULIB_MIR_NATIVE_PARAM: r[i->destination] = args[i->a]; break;
      case MULIB_MIR_NATIVE_JUMP: block = i->target; goto next;
      case MULIB_MIR_NATIVE_BRANCH: block = r[i->a] ? i->target : i->b; goto next;
      case MULIB_MIR_NATIVE_RETURN: result = r[i->a]; free (r); return result;
      default: r[i->destination] = value (i, r); break;
      }
    }
    assert (0);
  next:;
  }
}
static int sink (void *user, const char *text, size_t length)
{ return fwrite (text,1,length,(FILE *)user) == length; }
static int reject (void *u, const char *t, size_t n)
{ (void)u; (void)t; (void)n; return 0; }

static void test_cover (void)
{
  mulib_muburg_node_t nodes[] = {{0,0,{0,0}}, {1,2,{0,0}}, {2,1,{1,0}}, {999,0,{0,0}}};
  mulib_muburg_rule_t rules[] = {
    {0,0,0,{0,0},3,1}, {1,0,0,{0,0},1,2},
    {0,1,2,{0,0},0,3}, {0,1,2,{1,1},2,4}, {0,2,1,{0,0},1,5}
  };
  mulib_isel_limits_t lim = {4,5,100,16,128,1000};
  mulib_isel_cover_t cover = {0};
  assert (mulib_isel_cover (nodes,4,rules,5,100,2,0,lim,&cover,NULL) == MULIB_MIR_OK);
  assert (cover.cost == 5 && cover.count == 4); /* unused 98 goals and node 3 are never visited */
  assert (cover.memo[1].rule == 3);
  mulib_isel_cover_free (&cover);
  rules[2].cost = UINT64_MAX;
  assert (mulib_isel_cover (nodes,4,rules,5,100,2,0,lim,&cover,NULL) == MULIB_MIR_OK);
  assert (cover.cost == 5); mulib_isel_cover_free (&cover);
  rules[3].cost = UINT64_MAX;
  assert (mulib_isel_cover (nodes,4,rules,5,100,2,0,lim,&cover,NULL) == MULIB_MIR_OVERFLOW);
  rules[3].cost = 2; nodes[2].child[0] = 2;
  assert (mulib_isel_cover (nodes,4,rules,5,100,2,0,lim,&cover,NULL) == MULIB_MIR_BAD_REFERENCE);
  nodes[2].child[0] = 1; lim.work = 1;
  assert (mulib_isel_cover (nodes,4,rules,5,100,2,0,lim,&cover,NULL) == MULIB_MIR_RESOURCE_LIMIT && !cover.memo);
  lim.work = 1000; lim.cells = 1;
  assert (mulib_isel_cover (nodes,4,rules,5,100,2,0,lim,&cover,NULL) == MULIB_MIR_RESOURCE_LIMIT);
  lim.cells = 16;
  assert (mulib_isel_cover (nodes,4,rules,5,100,3,0,lim,&cover,NULL) == MULIB_MIR_UNSUPPORTED);
  {
    mulib_muburg_node_t sparse[] = {{0,0,{0,0}},{1,2,{0,0}}};
    mulib_muburg_rule_t grammar[] = {{0,0,0,{0,0},1,1},{0,1,2,{0,0},1,2}};
    mulib_isel_limits_t large = {2,2,SIZE_MAX,SIZE_MAX,128,1000};
    /* A sparse two-state query must not allocate nodes*goals or reject its overflow. */
    assert (mulib_isel_cover (sparse,2,grammar,2,SIZE_MAX,1,0,large,&cover,NULL) == MULIB_MIR_OK);
    assert (cover.count == 2 && cover.cost == 3); mulib_isel_cover_free (&cover);
  }
  {
    mulib_muburg_node_t missing[] = {{0,0,{0,0}},{1,0,{0,0}},{2,2,{0,1}}};
    mulib_muburg_rule_t grammar[] = {{0,0,0,{0,0},UINT64_MAX,1},{0,2,2,{0,0},1,2}};
    mulib_isel_limits_t bounds = {3,2,1,3,128,1000};
    /* An overflowing prefix plus an uncovered child is not a complete overflowing cover. */
    assert (mulib_isel_cover (missing,3,grammar,2,1,2,0,bounds,&cover,NULL) == MULIB_MIR_UNSUPPORTED);
  }
}
static void test_rules (void)
{
  size_t count;
  const mulib_peephole_rule_t *rules = mulib_peephole_rules (&count);
  uint64_t seed = 872634;
  assert (count == 75);
  for (size_t k = 0; k < count; ++k) for (size_t trial = 0; trial < 100; ++trial) {
    const mulib_peephole_rule_t *rule = &rules[k];
    mulib_mir_native_instruction_t i = mulib_mir_native_instruction (rule->opcode), original, inner;
    mulib_peephole_facts_t f = {0,{0,0},NULL,NULL};
    uint64_t r[4] = {random_word (&seed),random_word (&seed),0,0};
    i.destination = 3; i.a = 0;
    if (mulib_mir_native_arity (i.opcode) == 2) i.b = 1;
    if (i.opcode == MULIB_MIR_NATIVE_BRANCH) { i.destination = N; i.b = 1; i.target = 2; }
    inner = mulib_mir_native_instruction (rule->nested); inner.destination = 2; inner.a = 0; inner.b = 1;
    switch (rule->guard) {
    case MULIB_PEEPHOLE_FOLD: f.known = i.opcode == MULIB_MIR_NATIVE_COPY ? 1u : 3u; break;
    case MULIB_PEEPHOLE_A: r[0] = rule->literal; f.known = 1; break;
    case MULIB_PEEPHOLE_B: r[1] = rule->literal; f.known = 2; break;
    case MULIB_PEEPHOLE_SAME: i.b = 0; break;
    case MULIB_PEEPHOLE_SHIFT_ZERO: r[1] = 128; f.known = 2; break;
    case MULIB_PEEPHOLE_SELF_COPY: i.destination = 0; break;
    case MULIB_PEEPHOLE_BRANCH_SAME: i.b = i.target; break;
    case MULIB_PEEPHOLE_BRANCH_TRUE: r[0] = 1; f.known = 1; break;
    case MULIB_PEEPHOLE_BRANCH_FALSE: r[0] = 0; f.known = 1; break;
    case MULIB_PEEPHOLE_LEFT_A: case MULIB_PEEPHOLE_LEFT_B:
      r[2] = value (&inner,r); f.left = &inner; i.a = 2;
      i.b = rule->guard == MULIB_PEEPHOLE_LEFT_A ? 0 : 1; break;
    case MULIB_PEEPHOLE_RIGHT_A: case MULIB_PEEPHOLE_RIGHT_B:
      r[2] = value (&inner,r); f.right = &inner; i.b = 2;
      i.a = rule->guard == MULIB_PEEPHOLE_RIGHT_A ? 0 : 1; break;
    }
    f.values[0] = r[i.a]; f.values[1] = i.b == N ? 0 : r[i.b]; original = i;
    assert (mulib_peephole_apply (&i,&f,rule,1,NULL) == 1);
    if (original.opcode == MULIB_MIR_NATIVE_BRANCH)
      assert (i.opcode == MULIB_MIR_NATIVE_JUMP && i.target == (r[original.a] ? original.target : original.b));
    else if (i.opcode == MULIB_MIR_NATIVE_NOP) assert (value (&original,r) == r[original.destination]);
    else assert (value (&original,r) == value (&i,r));
  }
  {
    mulib_optml_rules_t policy = {0}; size_t offset;
    const char text[] = "# subset\nfold-add\n add-zero-right # identity\n";
    assert (mulib_optml_parse (text,sizeof(text)-1,256,&policy,&offset) == MULIB_MIR_OK && policy.count == 2);
    mulib_optml_rules_free (&policy);
    assert (mulib_optml_parse ("bogus",5,256,&policy,&offset) == MULIB_MIR_INVALID && !policy.rules && offset == 0);
    assert (mulib_optml_parse ("fold-add\nfold-add",17,256,&policy,&offset) == MULIB_MIR_INVALID);
    assert (mulib_optml_parse (NULL,0,0,&policy,&offset) == MULIB_MIR_OK && !policy.count);
    mulib_optml_rules_free (&policy);
  }
}
static void check_schedule (const mulib_lir_t *r)
{
  size_t *positions = malloc (r->body.count * sizeof (*positions));
  assert (positions);
  for (size_t k = 0; k < r->body.count; ++k) positions[r->order[k]] = k;
  for (size_t b = 0; b < r->body.block_count; ++b) {
    size_t first = r->body.blocks[b].first, end = first + r->body.blocks[b].count;
    for (size_t a = first; a < end; ++a) for (size_t c = a + 1; c < end; ++c)
      if (mulib_lir_dependency (r,a,c)) {
        assert (positions[a] < positions[c]);
        assert (r->annotations[a].cycle + r->annotations[a].latency <= r->annotations[c].cycle);
      }
  }
  free (positions);
}
static void test_random (void)
{
  uint64_t seed = 341534;
  for (size_t trial = 0; trial < 40; ++trial) {
    mulib_mir_native_instruction_t code[60];
    mulib_mir_native_block_t block = {0,60};
    mulib_mir_native_program_t p = {code,60,&block,1,8,2,NULL,0,NULL,0};
    mulib_lir_t selected = {0}, optimized = {0};
    mulib_optml_options_t o = mulib_optml_defaults ();
    mulib_optml_stats_t stats;
    for (size_t k = 0; k < 59; ++k) {
      unsigned op = (unsigned)(random_word (&seed) % 15);
      code[k] = mulib_mir_native_instruction ((mulib_mir_native_opcode_t)(MULIB_MIR_NATIVE_CONST + op));
      code[k].destination = (size_t)(random_word (&seed) % 8);
      if (code[k].opcode == MULIB_MIR_NATIVE_CONST) code[k].immediate = random_word (&seed) % 3;
      else if (code[k].opcode == MULIB_MIR_NATIVE_PARAM) code[k].a = (size_t)(random_word (&seed) % 2);
      else {
        code[k].a = (size_t)(random_word (&seed) % 8);
        if (mulib_mir_native_arity (code[k].opcode) == 2) code[k].b = (size_t)(random_word (&seed) % 8);
      }
    }
    code[59] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_RETURN); code[59].a = trial % 8;
    if (trial & 1) o.selection.machine = mulib_machine_rv64im ();
    o.selection.schedule = (trial & 2) != 0;
    assert (mulib_isel_run (&p,&o.selection,&selected,NULL) == MULIB_MIR_OK);
    assert (mulib_optml_run (&selected,&o,&optimized,&stats,NULL) == MULIB_MIR_OK);
    assert (mulib_lir_validate (&optimized,65536,NULL) == MULIB_MIR_OK);
    check_schedule (&optimized);
    for (size_t k = 0; k < 20; ++k) {
      uint64_t args[] = {random_word (&seed),random_word (&seed)};
      mulib_mir_native_program_t v = mulib_mir_native_view (&optimized.body);
      assert (run (&p,NULL,args) == run (&v,optimized.order,args));
    }
    mulib_lir_free (&selected); mulib_lir_free (&optimized);
  }
}
static void test_cover_differential (void)
{
  uint64_t seed = 91345;
  for (size_t trial = 0; trial < 80; ++trial) {
    mulib_muburg_node_t nodes[24];
    mulib_muburg_rule_t rules[30];
    mulib_muburg_plan_t eager = {0};
    mulib_isel_cover_t lazy = {0};
    mulib_muburg_limits_t ml = {24,30,3,72};
    mulib_isel_limits_t il = {24,30,3,72,128,1000000};
    memset (nodes,0,sizeof(nodes)); memset (rules,0,sizeof(rules));
    for (size_t k = 0; k < 24; ++k) {
      nodes[k].arity = k == 23 ? 0 : (unsigned)(random_word (&seed) % 3);
      nodes[k].opcode = (int)nodes[k].arity;
      for (unsigned j = 0; j < nodes[k].arity; ++j)
        nodes[k].child[j] = k + 1 + (size_t)(random_word (&seed) % (23-k));
    }
    for (size_t k = 0; k < 30; ++k) {
      rules[k].lhs = k < 9 ? k % 3 : (size_t)(random_word (&seed) % 3);
      rules[k].arity = k < 9 ? (unsigned)(k / 3) : (unsigned)(random_word (&seed) % 3);
      rules[k].opcode = (int)rules[k].arity; rules[k].instruction = (unsigned)k;
      rules[k].cost = random_word (&seed) % 7;
      if (k >= 9) for (unsigned j = 0; j < rules[k].arity; ++j)
        rules[k].child_goal[j] = (size_t)(random_word (&seed) % 3);
    }
    assert (mulib_muburg_select (nodes,24,rules,30,3,0,trial%3,ml,&eager,NULL) == MULIB_MUBURG_OK);
    assert (mulib_isel_cover (nodes,24,rules,30,3,0,trial%3,il,&lazy,NULL) == MULIB_MIR_OK);
    assert (eager.cost == lazy.cost);
    for (size_t k = 0; k < lazy.count; ++k) {
      size_t cell = lazy.memo[k].node * 3 + lazy.memo[k].goal;
      assert (lazy.memo[k].rule == eager.choices[cell] && lazy.memo[k].cost == eager.costs[cell]);
    }
    mulib_isel_cover_free (&lazy); mulib_muburg_free (&eager);
  }
}
static void test_flow (void)
{
  const mulib_mir_native_instruction_t code[] = {
    OP(PARAM,0,0,N,N,0), OP(CONST,1,N,N,N,0), OP(CONST,2,N,N,N,1), OP(JUMP,N,N,N,1,0),
    OP(ULT,3,1,0,N,0), OP(BRANCH,N,3,3,2,0),
    OP(ADD,1,1,2,N,0), OP(JUMP,N,N,N,1,0), OP(RETURN,N,1,N,N,0)
  };
  const mulib_mir_native_block_t blocks[] = {{0,4},{4,2},{6,2},{8,1}};
  mulib_mir_native_program_t p = {code,9,blocks,4,4,1,NULL,0,NULL,0};
  mulib_optml_result_t result = {0};
  assert (mulib_optml_compile (&p,"loop",NULL,&result,NULL) == MULIB_MIR_OK);
  for (uint64_t arg = 0; arg < 100; ++arg) {
    mulib_mir_native_program_t v = mulib_mir_native_view (&result.lir.body);
    assert (run (&v,result.lir.order,&arg) == arg);
  }
  assert (mulib_optml_write (&result,reject,NULL) == MULIB_MIR_IO_ERROR);
  mulib_optml_free (&result);
}
static void test_annotations (void)
{
  mulib_mir_native_instruction_t code[] = {
    OP(PARAM,0,0,N,N,0), OP(PARAM,1,1,N,N,0), OP(AND,2,0,1,N,0),
    OP(PARAM,1,2,N,N,0), OP(OR,3,2,1,N,0), OP(RETURN,N,3,N,N,0)
  };
  mulib_mir_native_block_t block = {0,6};
  mulib_mir_native_program_t p = {code,6,&block,1,4,3,NULL,0,NULL,0};
  mulib_lir_t selected = {0}, optimized = {0};
  uint64_t args[] = {15,3,8};
  assert (mulib_isel_run (&p,NULL,&selected,NULL) == MULIB_MIR_OK);
  assert (mulib_optml_run (&selected,NULL,&optimized,NULL,NULL) == MULIB_MIR_OK);
  {
    mulib_mir_native_program_t v = mulib_mir_native_view (&optimized.body);
    assert (run (&v,optimized.order,args) == 11); /* y was overwritten: no absorption */
  }
  mulib_lir_free (&selected); mulib_lir_free (&optimized);
  code[3] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_NOP);
  assert (mulib_isel_run (&p,NULL,&selected,NULL) == MULIB_MIR_OK);
  selected.annotations[4].constraints = MULIB_LIR_KEEP | MULIB_LIR_BARRIER;
  assert (mulib_optml_run (&selected,NULL,&optimized,NULL,NULL) == MULIB_MIR_OK);
  assert (optimized.body.code[4].opcode == MULIB_MIR_NATIVE_OR);
  assert (optimized.annotations[4].constraints == (MULIB_LIR_KEEP | MULIB_LIR_BARRIER));
  assert (optimized.annotations[4].source == 4); check_schedule (&optimized);
  mulib_lir_free (&optimized);
  selected.annotations[4].constraints = 0;
  assert (mulib_optml_run (&selected,NULL,&optimized,NULL,NULL) == MULIB_MIR_OK);
  assert (optimized.body.code[4].opcode == MULIB_MIR_NATIVE_COPY && optimized.body.code[4].a == 1);
  mulib_lir_free (&selected); mulib_lir_free (&optimized);
}
static void test_resources (void)
{
  mulib_isched_node_t nodes[] = {{2},{2},{1}};
  mulib_isched_resources_t resources[] = {{1,5},{1,5},{2,1}};
  mulib_isched_edge_t edges[] = {{0,2},{1,2}};
  mulib_isched_limits_t lim = {3,2,20};
  mulib_isched_schedule_t result = {0};
  assert (mulib_isched_run_resources (nodes,resources,3,edges,2,0,lim,&result,NULL) == MULIB_ISCHED_OK);
  assert (result.start[0] == 0 && result.start[1] == 5 && result.start[2] == 7 && result.makespan == 8);
  mulib_isched_free (&result); lim.cycles = 7;
  assert (mulib_isched_run_resources (nodes,resources,3,edges,2,0,lim,&result,NULL) == MULIB_ISCHED_RESOURCE_LIMIT && !result.order);
  resources[0].throughput = 0;
  assert (mulib_isched_run_resources (nodes,resources,3,edges,2,0,lim,&result,NULL) == MULIB_ISCHED_INVALID);
}
static void test_arch (const char *directory)
{
  mulib_mir_native_instruction_t code[] = {
    OP(PARAM,0,0,N,N,0), OP(PARAM,1,1,N,N,0), OP(MUL,2,0,1,N,0),
    OP(ADD,3,0,1,N,0), OP(ADD,2,2,3,N,0), OP(RETURN,N,2,N,N,0)
  };
  mulib_mir_native_block_t block = {0,6};
  mulib_mir_native_program_t p = {code,6,&block,1,4,2,NULL,0,NULL,0};
  const char *files[] = {"amd64.aml","riscv64.aml"};
  {
    char path[1024], source[8192];
    mulib_optml_rules_t policy = {0};
    size_t count, length;
    const mulib_peephole_rule_t *rules = mulib_peephole_rules (&count);
    FILE *file;
    snprintf (path,sizeof(path),"%s/../optim/default.optml",directory);
    file = fopen (path,"rb"); assert (file);
    length = fread (source,1,sizeof(source),file); assert (length < sizeof(source) && !ferror(file));
    assert (!fclose(file));
    assert (mulib_optml_parse (source,length,sizeof(source),&policy,NULL) == MULIB_MIR_OK);
    assert (policy.count == count);
    for (size_t k = 0; k < count; ++k) assert (!strcmp (policy.rules[k].name,rules[k].name));
    mulib_optml_rules_free (&policy);
  }
  for (size_t k = 0; k < 2; ++k) {
    char path[1024];
    mulib_archml_t *arch = NULL;
    mulib_lir_t r = {0}, clone = {0};
    mulib_isel_options_t o = mulib_isel_defaults ();
    snprintf (path,sizeof(path),"%s/%s",directory,files[k]);
    assert (mulib_archml_load (path,NULL,&arch,NULL) == MULIB_ARCHML_OK);
    o.arch = arch; if (k) o.machine = mulib_machine_rv64im ();
    assert (mulib_isel_run (&p,&o,&r,NULL) == MULIB_MIR_OK);
    assert (r.annotations[2].latency == 4);
    check_schedule (&r);
    r.annotations[2].constraints = MULIB_LIR_KEEP;
    assert (mulib_lir_clone (&r,65536,&clone,NULL) == MULIB_MIR_OK);
    assert (clone.body.code != r.body.code && clone.annotations[2].constraints == MULIB_LIR_KEEP);
    mulib_lir_free (&clone);
    strcpy (r.annotations[2].schedule,"serial");
    assert (mulib_isel_schedule (&r,&o,NULL) == MULIB_MIR_OK && r.annotations[2].latency == 64);
    assert (r.annotations[2].barrier); check_schedule (&r);
    {
      mulib_lir_annotation_t *saved = r.annotations;
      o.cycle_limit = 0;
      assert (mulib_isel_schedule (&r,&o,NULL) == MULIB_MIR_RESOURCE_LIMIT && r.annotations == saved);
    }
    mulib_archml_destroy (arch); /* output retains no pointers into ArchML */
    assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_OK);
    mulib_lir_free (&r);
  }
}
static void test_macros (void)
{
  static const char group[] = "\tmovq %rax, -80000000(%rbp)\n\tmovq %rcx, -80000008(%rbp)\n"
    "\taddq %rcx, %rax\n\tmovq %rax, -80000016(%rbp)\n";
  mulib_strbuf_t text = {0};
  mulib_superinstr_result_t packed = {0};
  for (size_t k = 0; k < 8; ++k) assert (mulib_strbuf_append_n (&text,group,sizeof(group)-1));
  assert (mulib_superinstr_pack (text.data,text.length,"f",NULL,&packed) == MULIB_MIR_OK);
  assert (packed.macros && packed.uses >= 2 && strstr (packed.text,".macro") && strstr (packed.text,".purgem"));
  assert (packed.length < text.length);
  mulib_superinstr_free (&packed); mulib_strbuf_free (&text);
  assert (mulib_superinstr_pack ("\t.macro x\n",10,"f",NULL,&packed) == MULIB_MIR_INVALID);
  assert (mulib_superinstr_pack (NULL,0,"f",NULL,&packed) == MULIB_MIR_OK && packed.length == 0);
  mulib_superinstr_free (&packed);
}
static void test_failure (void)
{
  mulib_mir_native_instruction_t code[] = {OP(CONST,0,N,N,N,42),OP(RETURN,N,0,N,N,0)};
  mulib_mir_native_block_t block = {0,2};
  mulib_mir_native_program_t p = {code,2,&block,1,1,0,NULL,0,NULL,0};
  mulib_optml_options_t o = mulib_optml_defaults ();
  mulib_optml_result_t r = {0};
  o.text_limit = 1;
  assert (mulib_optml_compile (&p,"f",&o,&r,NULL) == MULIB_MIR_RESOURCE_LIMIT && mulib_lir_empty (&r.lir));
  o = mulib_optml_defaults (); o.selection.edge_limit = 0;
  assert (mulib_optml_compile (&p,"f",&o,&r,NULL) == MULIB_MIR_RESOURCE_LIMIT && !r.assembly);
  o = mulib_optml_defaults (); code[1].a = 2;
  assert (mulib_optml_compile (&p,"f",&o,&r,NULL) == MULIB_MIR_BAD_REFERENCE);
  code[1].a = 0;
  assert (mulib_optml_compile (&p,"bad;symbol",&o,&r,NULL) == MULIB_MIR_INVALID);
  assert (mulib_optml_compile (&p,"f",&o,&r,NULL) == MULIB_MIR_OK);
  {
    char *saved = r.assembly;
    assert (mulib_optml_compile (&p,"f",&o,&r,NULL) == MULIB_MIR_INVALID && r.assembly == saved);
  }
  mulib_optml_free (&r);
  {
    mulib_mir_native_instruction_t many[32];
    mulib_mir_native_block_t blocks[16];
    mulib_mir_native_program_t large = {many,32,blocks,16,1,0,NULL,0,NULL,0};
    mulib_lir_t selected = {0};
    mulib_isel_options_t bounds = mulib_isel_defaults ();
    bounds.work_limit = 2048;
    assert (mulib_isel_run (&p,&bounds,&selected,NULL) == MULIB_MIR_OK);
    mulib_lir_free (&selected);
    for (size_t b = 0; b < 16; ++b) {
      blocks[b].first = b * 2; blocks[b].count = 2;
      many[b*2] = code[0];
      many[b*2+1] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_JUMP);
      many[b*2+1].target = b + 1;
    }
    many[31] = code[1];
    /* Many individually cheap blocks must share one selection/scheduling budget. */
    assert (mulib_isel_run (&large,&bounds,&selected,NULL) == MULIB_MIR_RESOURCE_LIMIT && mulib_lir_empty (&selected));
    assert (mulib_isel_run (&large,NULL,&selected,NULL) == MULIB_MIR_OK);
    assert (mulib_lir_validate (&selected,65536,NULL) == MULIB_MIR_OK);
    mulib_lir_free (&selected);
    large.count = large.block_count = 1; large.register_count = 0;
    blocks[0].count = 1;
    many[0] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_JUMP); many[0].target = 0;
    assert (mulib_isel_run (&large,NULL,&selected,NULL) == MULIB_MIR_OK);
    assert (mulib_lir_validate (&selected,65536,NULL) == MULIB_MIR_OK);
    mulib_lir_free (&selected);
  }
}
static int count_output (void *user, const char *text, size_t length)
{ (void)text; (void)length; ++*(size_t *)user; return 1; }
static void test_stale_lir (void)
{
  const mulib_mir_native_instruction_t code[] = {
    OP(PARAM,0,0,N,N,0), OP(CONST,1,N,N,N,5), OP(ADD,2,0,1,N,0),
    OP(CONST,0,N,N,N,99), OP(RETURN,N,2,N,N,0)
  };
  mulib_mir_native_block_t block = {0,5};
  mulib_mir_native_program_t p = {code,5,&block,1,3,1,NULL,0,NULL,0};
  mulib_lir_t r = {0};
  size_t writes = 0;
  assert (mulib_isel_run (&p,NULL,&r,NULL) == MULIB_MIR_OK);
  assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_OK);
  r.selection[2].operands[1] ^= 1;
  assert (mulib_optml_emit (&r,"stale",65536,count_output,&writes,NULL) == MULIB_MIR_INVALID && !writes);
  r.selection[2].operands[1] ^= 1;
  r.annotations[2].values[1] ^= 1;
  assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_INVALID);
  r.annotations[2].values[1] ^= 1;
  ++r.selection[2].cost;
  assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_INVALID);
  --r.selection[2].cost;
  ++r.cost; assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_INVALID); --r.cost;
  ++r.block_cycles[0]; assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_INVALID); --r.block_cycles[0];
  {
    size_t a = 0, b = 0, cycle;
    for (size_t k = 0; k < r.body.count; ++k) {
      if (r.order[k] == 2) a = k;
      if (r.order[k] == 3) b = k;
    }
    r.order[a] = 3; r.order[b] = 2;
    cycle = r.annotations[2].cycle; r.annotations[2].cycle = r.annotations[3].cycle; r.annotations[3].cycle = cycle;
    /* Cycles remain increasing: validation must independently discover the WAR hazard. */
    assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_INVALID);
    r.order[a] = 2; r.order[b] = 3;
    cycle = r.annotations[2].cycle; r.annotations[2].cycle = r.annotations[3].cycle; r.annotations[3].cycle = cycle;
  }
  r.annotations[0].resources = r.annotations[1].resources = 1;
  r.annotations[0].throughput = r.annotations[1].throughput = 100;
  assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_INVALID);
  assert (mulib_isel_schedule (&r,NULL,NULL) == MULIB_MIR_OK);
  r.body.code[1].immediate = 6;
  assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_INVALID);
  r.annotations[0].latency = 0; r.annotations[0].known_mask = 255;
  r.selection[0].rule = SIZE_MAX; r.order[0] = SIZE_MAX;
  assert (mulib_isel_refresh (&r,NULL,NULL) == MULIB_MIR_OK);
  assert (r.selection[2].operands[1] == 6 && mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_OK);
  r.scheduled = 0; r.order[0] = SIZE_MAX;
  assert (mulib_lir_write (&r,65536,count_output,&writes) == MULIB_MIR_OK);
  assert (mulib_isel_schedule (&r,NULL,NULL) == MULIB_MIR_OK);
  strcpy (r.annotations[0].schedule,"bad\nclass");
  assert (mulib_lir_validate (&r,65536,NULL) == MULIB_MIR_INVALID);
  mulib_lir_free (&r);
}
static void test_invalid_rules (void)
{
  size_t count;
  const mulib_peephole_rule_t *all = mulib_peephole_rules (&count);
  mulib_peephole_rule_t rule = all[0];
  mulib_mir_native_instruction_t i = {MULIB_MIR_NATIVE_ADD,2,0,1,N,0}, before = i;
  mulib_peephole_facts_t f = {3,{8,3},NULL,NULL};
  size_t matched = 123;
  assert (mulib_peephole_validate (all,count));
  rule.guard = (mulib_peephole_guard_t)999;
  assert (mulib_peephole_apply (&i,&f,&rule,1,&matched) == -1 && matched == 123 && !memcmp (&i,&before,sizeof(i)));
  rule = all[0]; rule.opcode = MULIB_MIR_NATIVE_STORE;
  assert (!mulib_peephole_validate (&rule,1));
  rule = all[0]; rule.opcode = MULIB_MIR_NATIVE_COPY; rule.action = MULIB_PEEPHOLE_COPY_B;
  assert (!mulib_peephole_validate (&rule,1));
  rule = all[0]; rule.action = MULIB_PEEPHOLE_LEFT_OTHER;
  assert (!mulib_peephole_validate (&rule,1));
  rule = all[0]; rule.guard = MULIB_PEEPHOLE_BRANCH_TRUE;
  assert (!mulib_peephole_validate (&rule,1));
  rule = all[0]; i.a = N;
  assert (mulib_peephole_apply (&i,&f,&rule,1,&matched) == -1 && matched == 123);
  i = before; f.known = 4;
  assert (mulib_peephole_apply (&i,&f,&rule,1,&matched) == -1);
  {
    mulib_mir_native_instruction_t code[] = {OP(CONST,0,N,N,N,1),OP(RETURN,N,0,N,N,0)};
    mulib_mir_native_block_t block = {0,2};
    mulib_mir_native_program_t p = {code,2,&block,1,1,0,NULL,0,NULL,0};
    mulib_optml_options_t o = mulib_optml_defaults ();
    mulib_optml_result_t r = {0};
    rule.action = (mulib_peephole_action_t)-1; o.rules = &rule; o.rule_count = 1; o.passes = 0;
    assert (mulib_optml_compile (&p,"bad_rule",&o,&r,NULL) == MULIB_MIR_INVALID && !r.assembly);
  }
}
static void test_resource_properties (void)
{
  uint64_t seed = 4567;
  for (unsigned trial = 0; trial < 80; ++trial) {
    mulib_isched_node_t nodes[8];
    mulib_isched_resources_t units[8];
    mulib_isched_edge_t edges[28];
    size_t count = 0, positions[8], ready[4] = {0};
    mulib_isched_schedule_t result = {0};
    mulib_isched_limits_t limits = {8,28,1000};
    for (size_t a = 0; a < 8; ++a) {
      nodes[a].latency = 1 + (size_t)(random_word (&seed) % 8);
      units[a].mask = random_word (&seed) & 15;
      units[a].throughput = 1 + (size_t)(random_word (&seed) % 10);
      for (size_t b = a + 1; b < 8; ++b) if (random_word (&seed) & 1) {
        edges[count].before = a; edges[count++].after = b;
      }
    }
    assert (mulib_isched_run_resources (nodes,units,8,edges,count,trial & 1,limits,&result,NULL) == MULIB_ISCHED_OK);
    for (size_t j = 0; j < 8; ++j) {
      size_t k = result.order[j]; positions[k] = j;
      if (trial & 1) assert (k == j);
      if (j) assert (result.start[k] > result.start[result.order[j-1]]);
      for (size_t u = 0; u < 4; ++u) if (units[k].mask & (UINT64_C(1) << u)) {
        assert (result.start[k] >= ready[u]); ready[u] = result.start[k] + units[k].throughput;
      }
    }
    for (size_t e = 0; e < count; ++e) {
      size_t a = edges[e].before, b = edges[e].after;
      assert (positions[a] < positions[b] && result.start[b] >= result.start[a] + nodes[a].latency);
    }
    mulib_isched_free (&result);
  }
  {
    mulib_isched_node_t nodes[] = {{1},{1}};
    mulib_isched_resources_t units[] = {{0,SIZE_MAX},{0,SIZE_MAX}};
    mulib_isched_schedule_t r = {0};
    mulib_isched_limits_t limits = {2,1,2};
    assert (mulib_isched_run_resources (nodes,units,2,NULL,0,1,limits,&r,NULL) == MULIB_ISCHED_OK && r.makespan == 2);
    mulib_isched_free (&r);
    units[0].mask = units[1].mask = 1;
    assert (mulib_isched_run_resources (nodes,units,2,NULL,0,1,limits,&r,NULL) == MULIB_ISCHED_OVERFLOW && !r.order);
  }
}
static void test_macro_input (void)
{
  const char *bad[] = {"\t.altmacro\n", "\t.MACRO f\n", "/* block\ncomment */\n",
    "\t.rept 10\n\tnop\n\t.endr\n", "\t.if 0\n\tnop\n\t.endif\n", "\tnop\\\n\tnop\n",
    "\tmulib_si_f_0\n", "\t.include \"input.s\"\n", "label: .altmacro\n",
    "label:.altmacro\n", "\t.text ; .altmacro\n", "\f.altmacro\n", "\r.altmacro\n"};
  for (size_t k = 0; k < sizeof(bad)/sizeof(*bad); ++k) {
    mulib_superinstr_result_t r = {0};
    mulib_mir_status_t s = mulib_superinstr_pack (bad[k],strlen(bad[k]),"f",NULL,&r);
    assert ((s == MULIB_MIR_UNSUPPORTED || s == MULIB_MIR_INVALID) && !r.text);
  }
  {
    mulib_superinstr_options_t o = mulib_superinstr_defaults ();
    mulib_superinstr_result_t r = {0};
    const char text[] = "\tnop\nlabel:\n\tnop\n";
    o.work_limit = 0; o.max_lines = SIZE_MAX;
    assert (mulib_superinstr_pack (text,sizeof(text)-1,"f",&o,&r) == MULIB_MIR_RESOURCE_LIMIT && !r.text);
    assert (mulib_superinstr_pack (text,sizeof(text)-1,"f",NULL,&r) == MULIB_MIR_OK && !strcmp(r.text,text));
    mulib_superinstr_free (&r);
  }
}
static void emit_native (const char *path, int rv)
{
  /* Deliberately reuse registers: emission must honor WAR/WAW, memory and CALL
   * boundaries as well as the usual def-use graph. */
  const mulib_mir_native_instruction_t code[] = {
    OP(PARAM,0,0,N,N,0), OP(PARAM,1,1,N,N,0), OP(CONST,2,N,N,N,2),
    OP(MUL,3,0,2,N,0), OP(ADD,0,0,1,N,0), OP(XOR,3,3,0,N,0),
    OP(SHL,3,3,1,N,0), OP(RETURN,N,3,N,N,0)
  };
  mulib_mir_native_block_t block = {0,8};
  mulib_mir_native_program_t p = {code,8,&block,1,4,2,NULL,0,NULL,0};
  mulib_optml_options_t o = mulib_optml_defaults ();
  mulib_optml_result_t r = {0};
  FILE *f = fopen (path,"wb"); assert (f);
  if (rv) o.selection.machine = mulib_machine_rv64im ();
  assert (mulib_optml_compile (&p,"backend_check",&o,&r,NULL) == MULIB_MIR_OK);
  assert (mulib_optml_write (&r,sink,f) == MULIB_MIR_OK);
  mulib_optml_free (&r);
  {
    mulib_mir_native_instruction_t repeated[27];
    mulib_mir_native_block_t bb = {0,27};
    mulib_mir_native_program_t pp = {repeated,27,&bb,1,2,2,NULL,0,NULL,0};
    repeated[0] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_PARAM);
    repeated[0].destination = 0; repeated[0].a = 0;
    repeated[1] = repeated[0]; repeated[1].destination = 1; repeated[1].a = 1;
    for (size_t k = 2; k < 26; ++k) {
      repeated[k] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_ADD);
      repeated[k].destination = repeated[k].a = 0; repeated[k].b = 1;
    }
    repeated[26] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_RETURN); repeated[26].a = 0;
    assert (mulib_optml_compile (&pp,"backend_macro",&o,&r,NULL) == MULIB_MIR_OK);
    assert (r.stats.macros && r.stats.macro_uses >= 2);
    assert (mulib_optml_write (&r,sink,f) == MULIB_MIR_OK);
    mulib_optml_free (&r);
  }
  {
    const mulib_mir_native_instruction_t memory[] = {
      OP(PARAM,0,0,N,N,0), OP(PARAM,1,1,N,N,0), OP(STORE,N,0,1,N,0),
      OP(CALL,2,0,2,0,0), OP(LOAD,3,0,N,N,0), OP(ADD,2,2,3,N,0), OP(RETURN,N,2,N,N,0)
    };
    const size_t args[] = {0,1};
    const char *const symbols[] = {"backend_host"};
    mulib_mir_native_block_t bb = {0,7};
    mulib_mir_native_program_t pp = {memory,7,&bb,1,4,2,args,2,symbols,1};
    assert (mulib_optml_compile (&pp,"backend_memory",&o,&r,NULL) == MULIB_MIR_OK);
    assert (mulib_optml_write (&r,sink,f) == MULIB_MIR_OK);
    mulib_optml_free (&r);
  }
  assert (!fclose(f));
}
int main (int argc, char **argv)
{
  int op;
  for (op = 0; op < MULIB_MIR_NATIVE_OPCODE_COUNT; ++op) assert (mulib_lir_opcode_name (op));
  assert (!strcmp (mulib_lir_opcode_name (MULIB_MIR_NATIVE_RETURN),"return"));
  assert (!strcmp (mulib_lir_opcode_name (MULIB_MIR_NATIVE_SDIV),"sdiv"));
  assert (!strcmp (mulib_lir_opcode_name (MULIB_MIR_NATIVE_SREM),"srem"));
  assert (!mulib_lir_opcode_name (MULIB_MIR_NATIVE_OPCODE_COUNT));
  if (argc > 2 && !strcmp (argv[1],"--emit")) { emit_native (argv[2],argc > 3); return 0; }
  test_cover (); test_cover_differential (); test_rules (); test_resources (); test_random (); test_flow (); test_annotations (); test_macros (); test_failure ();
  test_stale_lir (); test_invalid_rules (); test_resource_properties (); test_macro_input ();
  if (argc > 1) test_arch (argv[1]);
  puts ("isel/OptML: memoization, 75 rewrite properties, randomized semantics, scheduling and ownership passed");
  return 0;
}
