/** @file include-mulib-qvm.h
 * @brief Quota VM: a resumable 16-register, unsigned-64-bit bytecode engine.
 * Static-inline dependency, with owned code/RAM and explicit host calls.
 * Arithmetic wraps modulo 2^64; memory accesses are checked and little-endian. */
#ifndef INCLUDE_MULIB_QVM_H
#define INCLUDE_MULIB_QVM_H
#include "include-mulib-container-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_qvm_opcode
{
  MULIB_QVM_HALT, MULIB_QVM_CONST, MULIB_QVM_MOVE, MULIB_QVM_ADD,
  MULIB_QVM_SUB, MULIB_QVM_MUL, MULIB_QVM_DIV, MULIB_QVM_AND,
  MULIB_QVM_OR, MULIB_QVM_XOR, MULIB_QVM_EQ, MULIB_QVM_LT,
  MULIB_QVM_LOAD, MULIB_QVM_STORE, MULIB_QVM_JUMP, MULIB_QVM_JNZ,
  MULIB_QVM_HOST, MULIB_QVM_YIELD
} mulib_qvm_opcode;
/** @brief a is destination/value, b/c are sources; immediate is CONST value,
 * jump target, memory offset added to register b, or host operation number. */
typedef struct mulib_qvm_instruction
{
  mulib_qvm_opcode opcode;
  unsigned a, b, c;
  uint64_t immediate;
} mulib_qvm_instruction;
/** @brief Host receives two values and writes one result. Return zero or a
 * negative CT error; callbacks must not reenter or retain output pointers. */
typedef int (*mulib_qvm_host) (void *, uint64_t, uint64_t, uint64_t, uint64_t *);
typedef struct mulib_qvm
{
  mulib_qvm_instruction *code;
  size_t count, pc;
  mulib_ct_blob memory;
  uint64_t registers[16], steps, step_limit;
  mulib_qvm_host host;
  void *user;
  int halted, fault, exit_code;
} mulib_qvm;
/** @brief Validate the whole instruction stream, including unreachable code. */
static inline int
mulib_qvm_validate (const mulib_qvm_instruction *code, size_t count)
{
  size_t i;
  if (!code || !count || count > SIZE_MAX / sizeof (*code)) return MULIB_CT_EINVAL;
  for (i = 0; i < count; ++i)
    {
      const mulib_qvm_instruction *p = code + i;
      if (p->opcode < 0 || p->opcode > MULIB_QVM_YIELD
          || p->a >= 16 || p->b >= 16 || p->c >= 16) return MULIB_CT_EFORMAT;
      if ((p->opcode == MULIB_QVM_JUMP || p->opcode == MULIB_QVM_JNZ)
          && p->immediate >= count) return MULIB_CT_EFORMAT;
    }
  return 0;
}
/** @brief Release owned code/RAM; NULL-safe and idempotent. */
static inline void
mulib_qvm_free (mulib_qvm *vm)
{
  if (!vm) return;
  MULIB_CT_FREE (vm->code); mulib_ct_blob_free (&vm->memory);
  memset (vm, 0, sizeof (*vm));
}
/** @brief Construct into uninitialized output. max_bytes bounds code plus RAM.
 * A zero step limit permits no instructions. Host/context are borrowed. */
static inline int
mulib_qvm_init (mulib_qvm *vm, const mulib_qvm_instruction *code, size_t count,
                size_t memory_bytes, size_t max_bytes, uint64_t steps,
                mulib_qvm_host host, void *user)
{
  mulib_qvm next;
  size_t bytes;
  int rc = mulib_qvm_validate (code, count);
  if (!vm) return MULIB_CT_EINVAL;
  if (rc) return rc;
  bytes = count * sizeof (*code);
  if (bytes > max_bytes || memory_bytes > max_bytes - bytes) return MULIB_CT_ELIMIT;
  memset (&next, 0, sizeof (next));
  next.code = (mulib_qvm_instruction *)MULIB_CT_MALLOC (bytes);
  if (!next.code) return MULIB_CT_ENOMEM;
  memcpy (next.code, code, bytes);
  rc = mulib_ct_blob_reserve (&next.memory, memory_bytes, max_bytes - bytes);
  if (rc) { mulib_qvm_free (&next); return rc; }
  if (memory_bytes) memset (next.memory.data, 0, memory_bytes);
  next.memory.length = memory_bytes; next.count = count; next.step_limit = steps;
  next.host = host; next.user = user; *vm = next;
  return 0;
}
/** @brief Reset PC, registers, counters, RAM, halt and fault flags. */
static inline int
mulib_qvm_reset (mulib_qvm *vm)
{
  if (!vm || !vm->code) return MULIB_CT_ESTATE;
  vm->pc = 0; vm->steps = 0; vm->halted = vm->fault = vm->exit_code = 0;
  memset (vm->registers, 0, sizeof (vm->registers));
  if (vm->memory.length) memset (vm->memory.data, 0, vm->memory.length);
  return 0;
}
/** @brief Run at most budget instructions. AGAIN is resumable; faults are
 * sticky until reset. HALT uses the low eight bits of register a as exit code. */
static inline int
mulib_qvm_run (mulib_qvm *vm, size_t budget)
{
  size_t done;
  if (!vm || !vm->code) return MULIB_CT_EINVAL;
  if (vm->fault) return vm->fault;
  if (vm->halted) return MULIB_CT_DONE;
  for (done = 0; done < budget; ++done)
    {
      const mulib_qvm_instruction *p;
      uint64_t b, c, value = 0, address;
      size_t next;
      int write = 1, rc = 0;
      if (vm->steps >= vm->step_limit) return vm->fault = MULIB_CT_ELIMIT;
      if (vm->pc >= vm->count) return vm->fault = MULIB_CT_EFORMAT;
      p = vm->code + vm->pc; b = vm->registers[p->b]; c = vm->registers[p->c];
      next = vm->pc + 1; ++vm->steps;
      switch (p->opcode)
        {
        case MULIB_QVM_HALT:
          vm->halted = 1; vm->exit_code = (int)(vm->registers[p->a] & 255);
          return MULIB_CT_DONE;
        case MULIB_QVM_CONST: value = p->immediate; break;
        case MULIB_QVM_MOVE: value = b; break;
        case MULIB_QVM_ADD: value = b + c; break;
        case MULIB_QVM_SUB: value = b - c; break;
        case MULIB_QVM_MUL: value = b * c; break;
        case MULIB_QVM_DIV: if (!c) rc = MULIB_CT_EINVAL; else value = b / c; break;
        case MULIB_QVM_AND: value = b & c; break;
        case MULIB_QVM_OR: value = b | c; break;
        case MULIB_QVM_XOR: value = b ^ c; break;
        case MULIB_QVM_EQ: value = b == c; break;
        case MULIB_QVM_LT: value = b < c; break;
        case MULIB_QVM_LOAD:
        case MULIB_QVM_STORE:
          if (p->immediate > UINT64_MAX - b) { rc = MULIB_CT_ELIMIT; break; }
          address = b + p->immediate;
          if (address > vm->memory.length || vm->memory.length - (size_t)address < 8)
            { rc = MULIB_CT_ELIMIT; break; }
          if (p->opcode == MULIB_QVM_LOAD) value = mulib_ct_load64 (vm->memory.data + (size_t)address);
          else { mulib_ct_store64 (vm->memory.data + (size_t)address, vm->registers[p->a]); write = 0; }
          break;
        case MULIB_QVM_JUMP: next = (size_t)p->immediate; write = 0; break;
        case MULIB_QVM_JNZ:
          if (vm->registers[p->a]) next = (size_t)p->immediate;
          write = 0; break;
        case MULIB_QVM_HOST:
          if (!vm->host) rc = MULIB_CT_ENOTSUP;
          else { rc = vm->host (vm->user, p->immediate, b, c, &value); if (rc > 0) rc = MULIB_CT_EIO; }
          break;
        case MULIB_QVM_YIELD: vm->pc = next; return MULIB_CT_AGAIN;
        default: rc = MULIB_CT_EFORMAT; break;
        }
      if (rc) return vm->fault = rc;
      if (write) vm->registers[p->a] = value;
      vm->pc = next;
    }
  return MULIB_CT_AGAIN;
}
#ifdef __cplusplus
}
#endif
#endif
