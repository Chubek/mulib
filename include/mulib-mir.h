/** @file mulib-mir.h
 * @brief Typed SSA, runtime register IR and lower-level native machine IR with bounded
 * ownership. Define MULIB_IMPL_MIR in one translation unit. No implementation
 * dependencies.
 */
#ifndef MULIB_MIR_H
#define MULIB_MIR_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_MIR_NONE SIZE_MAX
  typedef enum mulib_mir_type
  {
    MULIB_MIR_VOID,
    MULIB_MIR_I64,
    MULIB_MIR_BOOL,
    MULIB_MIR_F64,
    MULIB_MIR_WORD
  } mulib_mir_type_t;
  typedef enum mulib_mir_opcode
  {
    MULIB_MIR_CONST,
    MULIB_MIR_PARAM,
    MULIB_MIR_COPY,
    MULIB_MIR_ADD,
    MULIB_MIR_SUB,
    MULIB_MIR_MUL,
    MULIB_MIR_DIV,
    MULIB_MIR_MOD,
    MULIB_MIR_NEG,
    MULIB_MIR_EQ,
    MULIB_MIR_LT,
    MULIB_MIR_LE,
    MULIB_MIR_NOT,
    MULIB_MIR_RETURN
  } mulib_mir_opcode_t;
  /** Record index is its SSA value ID. Unused operands must be NONE, unused
   * immediate zero. */
  typedef struct mulib_mir_instruction
  {
    mulib_mir_opcode_t opcode;
    mulib_mir_type_t type;
    size_t a, b;
    int64_t immediate;
  } mulib_mir_instruction_t;
  /** Borrowed immutable records. PARAM immediate indexes the argument array.
   */
  typedef struct mulib_mir_program
  {
    const mulib_mir_instruction_t *code;
    size_t count, parameter_count;
  } mulib_mir_program_t;
  typedef enum mulib_mir_status
  {
    MULIB_MIR_OK,
    MULIB_MIR_INVALID,
    MULIB_MIR_BAD_OPCODE,
    MULIB_MIR_BAD_REFERENCE,
    MULIB_MIR_BAD_TYPE,
    MULIB_MIR_BAD_RETURN,
    MULIB_MIR_RESOURCE_LIMIT,
    MULIB_MIR_NO_MEMORY,
    MULIB_MIR_OVERFLOW,
    MULIB_MIR_DIVIDE_BY_ZERO,
    MULIB_MIR_STEP_LIMIT,
    MULIB_MIR_HOST_ERROR,
    MULIB_MIR_SUSPENDED,
    MULIB_MIR_FINISHED,
     MULIB_MIR_IO_ERROR,
     MULIB_MIR_UNSUPPORTED
  } mulib_mir_status_t;
  typedef struct mulib_mir_diagnostic
  {
    mulib_mir_status_t status;
    size_t record;
  } mulib_mir_diagnostic_t;
  /** Owned fixed-capacity builder. Do not copy a live buffer; zero initialize
   * before init. */
  typedef struct mulib_mir_buffer
  {
    mulib_mir_instruction_t *code;
    size_t count, capacity, parameter_count;
  } mulib_mir_buffer_t;
  /** Allocate capacity records; zero capacity is allowed. Destination must be
   * empty. */
  mulib_mir_status_t mulib_mir_buffer_init (mulib_mir_buffer_t *buffer,
                                            size_t capacity,
                                            size_t parameters);
  /** Copy a record without validation; failure leaves the buffer unchanged. */
  mulib_mir_status_t mulib_mir_append (mulib_mir_buffer_t *buffer,
                                       mulib_mir_instruction_t instruction);
  /** Borrow a view, invalidated by free. */
  mulib_mir_program_t mulib_mir_view (const mulib_mir_buffer_t *buffer);
  /** Release records and zero the object; NULL is harmless. */
  void mulib_mir_buffer_free (mulib_mir_buffer_t *buffer);
  /** Validate every record, including dead records; earlier-only references
   * rule out cycles. */
  mulib_mir_status_t mulib_mir_validate (const mulib_mir_program_t *program,
                                         size_t record_limit,
                                         mulib_mir_diagnostic_t *diagnostic);
  /** Operand count, or -1 for an unknown opcode. */
  int mulib_mir_arity (mulib_mir_opcode_t opcode);
  /** Evaluate a scalar operation; CONST uses immediate. PARAM/RETURN are
   * invalid here. On error *value is unchanged. Typed callers must first
   * validate their records.
   */
  mulib_mir_status_t mulib_mir_evaluate (mulib_mir_opcode_t opcode, int64_t a,
                                         int64_t b, int64_t immediate,
                                         int64_t *value);
  /** Static diagnostic description. */
  const char *mulib_mir_status_string (mulib_mir_status_t status);

  /** Register IR. WORD is an opaque 64-bit runtime handle; F64 is double.
   * Registers have fixed types and start at zero. This is a separate dialect
   * from the original, earlier-reference-only SSA API above.
   */
  typedef union mulib_mir_value
  {
    int64_t integer;
    uint64_t word;
    double number;
  } mulib_mir_value_t;
  typedef enum mulib_mir_flow_opcode
  {
    MULIB_MIR_FLOW_CONST,
    MULIB_MIR_FLOW_PARAM,
    MULIB_MIR_FLOW_COPY,
    MULIB_MIR_FLOW_ADD,
    MULIB_MIR_FLOW_SUB,
    MULIB_MIR_FLOW_MUL,
    MULIB_MIR_FLOW_DIV,
    MULIB_MIR_FLOW_MOD,
    MULIB_MIR_FLOW_NEG,
    MULIB_MIR_FLOW_EQ,
    MULIB_MIR_FLOW_LT,
    MULIB_MIR_FLOW_LE,
    MULIB_MIR_FLOW_NOT,
    MULIB_MIR_FLOW_WRAP_ADD,
    MULIB_MIR_FLOW_WRAP_SUB,
    MULIB_MIR_FLOW_WRAP_MUL,
    MULIB_MIR_FLOW_AND,
    MULIB_MIR_FLOW_OR,
    MULIB_MIR_FLOW_XOR,
    MULIB_MIR_FLOW_SHL,
    MULIB_MIR_FLOW_SHR,
    MULIB_MIR_FLOW_I64_F64,
    MULIB_MIR_FLOW_F64_I64,
    MULIB_MIR_FLOW_JUMP,
    MULIB_MIR_FLOW_BRANCH,
    MULIB_MIR_FLOW_CALL,
    MULIB_MIR_FLOW_RETURN
  } mulib_mir_flow_opcode_t;
  /** Unused register indices/target are NONE; unused immediate.word is zero.
   * PARAM uses a as argument index. CALL uses a,b as a contiguous register
   * range and target as host index. BRANCH uses a as a Boolean, target for
   * true and fallthrough for false. JUMP uses target. RETURN uses a.
   */
  typedef struct mulib_mir_flow_instruction
  {
    mulib_mir_flow_opcode_t opcode;
    size_t destination, a, b, target;
    mulib_mir_value_t immediate;
  } mulib_mir_flow_instruction_t;
  typedef struct mulib_mir_signature
  {
    const mulib_mir_type_t *parameters;
    size_t parameter_count;
    mulib_mir_type_t result;
  } mulib_mir_signature_t;
  /** All arrays are borrowed and immutable, including host signatures. */
  typedef struct mulib_mir_flow_program
  {
    const mulib_mir_flow_instruction_t *code;
    size_t count;
    const mulib_mir_type_t *register_types;
    size_t register_count;
    mulib_mir_signature_t signature;
    const mulib_mir_signature_t *hosts;
    size_t host_count;
  } mulib_mir_flow_program_t;
  /** Return OK and set result, SUSPENDED to await an explicit resume value,
   * or a fault. A suspended callback is never called again on resume.
   * Arguments are borrowed for the call. Side effects are not rolled back.
   */
  typedef mulib_mir_status_t (*mulib_mir_host_fn) (
      void *user, size_t host, const mulib_mir_value_t *arguments,
      size_t count, mulib_mir_value_t *result);
  /** Owned execution storage; initialize to zero and release with frame_free.
   * Program must remain immutable and alive. Do not edit fields during a run.
   * Independent frames may share a program. A frame is not reentrant.
   */
  typedef struct mulib_mir_frame
  {
    const mulib_mir_flow_program_t *program;
    mulib_mir_value_t *registers, *arguments;
    mulib_mir_host_fn host;
    void *user;
    size_t pc, steps, remaining;
    mulib_mir_value_t result;
    mulib_mir_status_t status;
    int running;
  } mulib_mir_frame_t;
  /** Validate all records/types/targets, including unreachable code. The last
   * record must be a RETURN or JUMP, preventing fallthrough out of the image.
   * limit bounds each array and the aggregate host-parameter count.
   */
  mulib_mir_status_t mulib_mir_flow_validate (
      const mulib_mir_flow_program_t *program, size_t limit,
      mulib_mir_diagnostic_t *diagnostic);
  /** Validate, copy arguments, and allocate zero-initialized typed registers.
   * Failure leaves an empty frame unchanged. BOOL arguments must be 0 or 1.
   */
  mulib_mir_status_t mulib_mir_frame_init (
      mulib_mir_frame_t *frame, const mulib_mir_flow_program_t *program,
      const mulib_mir_value_t *arguments, size_t argument_count,
      mulib_mir_host_fn host, void *user, size_t limit,
      mulib_mir_diagnostic_t *diagnostic);
  void mulib_mir_frame_free (mulib_mir_frame_t *frame);
  /** Execute one record. remaining is the instruction budget; steps includes
   * faults and suspending calls. Faults leave destination and pc unchanged.
   * RETURN sets result/status FINISHED. No implicit resume or fault retry.
   */
  mulib_mir_status_t mulib_mir_frame_step (mulib_mir_frame_t *frame);
  /** Complete a suspended CALL. Invalid Boolean results leave it suspended. */
  mulib_mir_status_t mulib_mir_frame_resume (mulib_mir_frame_t *frame,
                                            mulib_mir_value_t result);
  /** Emit a C11 function with signature mulib_mir_status_t name(frame*).
   * Output uses this header's runtime and a validated matching frame. It has
   * direct C control-flow edges and can be compiled ahead of time. The writer
   * returns nonzero on success; partial output is possible on writer failure.
   * symbol must start with mulib_generated_ and be at most 127 ASCII bytes.
   */
  typedef int (*mulib_mir_write_fn) (void *user, const char *bytes,
                                    size_t length);
  mulib_mir_status_t mulib_mir_flow_emit_c (
       const mulib_mir_flow_program_t *program, const char *symbol, size_t limit,
       mulib_mir_write_fn writer, void *user, mulib_mir_diagnostic_t *diagnostic);

  /** Native machine IR: untyped 64-bit virtual registers, explicit blocks,
   * memory and direct calls. Integer arithmetic wraps; shifts mask the count
   * to six bits. SLT/SAR are signed, ULT/SHR unsigned. Registers start at zero.
   * This dialect deliberately has no SSA, phi, implicit fallthrough or host
   * runtime. The earlier checked SSA/VM dialects retain their semantics. */
  typedef enum mulib_mir_native_opcode
  {
    MULIB_MIR_NATIVE_NOP, MULIB_MIR_NATIVE_CONST, MULIB_MIR_NATIVE_PARAM,
    MULIB_MIR_NATIVE_COPY, MULIB_MIR_NATIVE_ADD, MULIB_MIR_NATIVE_SUB,
    MULIB_MIR_NATIVE_MUL, MULIB_MIR_NATIVE_AND, MULIB_MIR_NATIVE_OR,
    MULIB_MIR_NATIVE_XOR, MULIB_MIR_NATIVE_SHL, MULIB_MIR_NATIVE_SHR,
    MULIB_MIR_NATIVE_SAR, MULIB_MIR_NATIVE_EQ, MULIB_MIR_NATIVE_SLT,
    MULIB_MIR_NATIVE_ULT, MULIB_MIR_NATIVE_LOAD, MULIB_MIR_NATIVE_STORE,
    MULIB_MIR_NATIVE_CALL, MULIB_MIR_NATIVE_JUMP, MULIB_MIR_NATIVE_BRANCH,
    MULIB_MIR_NATIVE_RETURN, MULIB_MIR_NATIVE_OPCODE_COUNT
  } mulib_mir_native_opcode_t;
  /** Unused indices are NONE, unused immediate is zero. CALL uses a,b as a
   * slice of program.arguments and target as symbol index. PARAM uses a as
   * parameter index. BRANCH uses a as condition, target/b as true/false blocks.
   * LOAD/STORE access a native 64-bit word at address a; STORE's value is b. */
  typedef struct mulib_mir_native_instruction
  {
    mulib_mir_native_opcode_t opcode;
    size_t destination, a, b, target;
    uint64_t immediate;
  } mulib_mir_native_instruction_t;
  typedef struct mulib_mir_native_block { size_t first, count; }
    mulib_mir_native_block_t;
  /** Borrowed immutable arrays. Blocks partition code, and end in exactly one
   * explicit terminator. Block zero is the entry. Symbols are ASCII C names. */
  typedef struct mulib_mir_native_program
  {
    const mulib_mir_native_instruction_t *code;
    size_t count;
    const mulib_mir_native_block_t *blocks;
    size_t block_count, register_count, parameter_count;
    const size_t *arguments;
    size_t argument_count;
    const char *const *symbols;
    size_t symbol_count;
  } mulib_mir_native_program_t;
  /** Owned structural arrays and symbol strings. Zero initialize; do not copy
   * live owners. Counts are fixed by init; fill arrays before validation. */
  typedef struct mulib_mir_native_buffer
  {
    mulib_mir_native_instruction_t *code;
    mulib_mir_native_block_t *blocks;
    size_t *arguments;
    char **symbols;
    size_t count, block_count, register_count, parameter_count;
    size_t argument_count, symbol_count;
  } mulib_mir_native_buffer_t;
  /** Allocate zeroed storage transactionally; limit bounds each array. */
  mulib_mir_status_t mulib_mir_native_init (
      mulib_mir_native_buffer_t *out, size_t records, size_t blocks,
      size_t registers, size_t parameters, size_t arguments, size_t symbols,
      size_t limit);
  /** Copy/replace one symbol, checking its spelling and length (1..127). */
  mulib_mir_status_t mulib_mir_native_symbol (
      mulib_mir_native_buffer_t *buffer, size_t index, const char *name);
  /** Borrow an immutable view, invalidated by free or editing the buffer. */
  mulib_mir_native_program_t mulib_mir_native_view (
      const mulib_mir_native_buffer_t *buffer);
  /** Validate all blocks, operands and symbols, including unreachable code. */
  mulib_mir_status_t mulib_mir_native_validate (
      const mulib_mir_native_program_t *program, size_t limit,
      mulib_mir_diagnostic_t *diagnostic);
  /** Validate and deep-copy, preserving an empty destination on failure. */
  mulib_mir_status_t mulib_mir_native_clone (
      const mulib_mir_native_program_t *program, size_t limit,
      mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
  /** Free all owned storage and zero the owner; NULL is harmless. */
  void mulib_mir_native_free (mulib_mir_native_buffer_t *buffer);
  /** Data operand count (CALL arguments are a separate slice), or -1. */
  int mulib_mir_native_arity (int opcode);
  /** Whether an opcode writes a virtual register. */
  int mulib_mir_native_defines (int opcode);
  /** Whether an opcode may access memory (LOAD, STORE, CALL). */
  int mulib_mir_native_effect (int opcode);
  /** Construct a canonical empty instruction with all indices NONE. */
  mulib_mir_native_instruction_t mulib_mir_native_instruction (
      mulib_mir_native_opcode_t opcode);
  /** Test assembly-safe ASCII identifier spelling, bounded to 127 bytes. */
  int mulib_mir_native_name (const char *name);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MIR) && !defined(MULIB_MIR_IMPLEMENTED)
#define MULIB_MIR_IMPLEMENTED
#include <inttypes.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_mir_status_t
  mulib_mir_buffer_init (mulib_mir_buffer_t *b, size_t capacity,
                         size_t parameters)
  {
    mulib_mir_instruction_t *p = NULL;
    if (!b || b->code || b->count || b->capacity)
      return MULIB_MIR_INVALID;
    if (capacity > SIZE_MAX / sizeof (*p))
      return MULIB_MIR_RESOURCE_LIMIT;
    if (capacity)
      {
        p = (mulib_mir_instruction_t *)malloc (capacity * sizeof (*p));
        if (!p)
          return MULIB_MIR_NO_MEMORY;
      }
    b->code = p;
    b->capacity = capacity;
    b->parameter_count = parameters;
    return MULIB_MIR_OK;
  }
  mulib_mir_status_t
  mulib_mir_append (mulib_mir_buffer_t *b, mulib_mir_instruction_t i)
  {
    if (!b || b->count > b->capacity || (b->capacity && !b->code))
      return MULIB_MIR_INVALID;
    if (b->count == b->capacity)
      return MULIB_MIR_RESOURCE_LIMIT;
    b->code[b->count++] = i;
    return MULIB_MIR_OK;
  }
  mulib_mir_program_t
  mulib_mir_view (const mulib_mir_buffer_t *b)
  {
    mulib_mir_program_t p = { NULL, 0, 0 };
    if (b)
      {
        p.code = b->code;
        p.count = b->count;
        p.parameter_count = b->parameter_count;
      }
    return p;
  }
  void
  mulib_mir_buffer_free (mulib_mir_buffer_t *b)
  {
    if (b)
      {
        free (b->code);
        b->code = NULL;
        b->count = b->capacity = b->parameter_count = 0;
      }
  }
  int
  mulib_mir_arity (mulib_mir_opcode_t op)
  {
    switch (op)
      {
      case MULIB_MIR_CONST:
      case MULIB_MIR_PARAM:
        return 0;
      case MULIB_MIR_COPY:
      case MULIB_MIR_NEG:
      case MULIB_MIR_NOT:
      case MULIB_MIR_RETURN:
        return 1;
      case MULIB_MIR_ADD:
      case MULIB_MIR_SUB:
      case MULIB_MIR_MUL:
      case MULIB_MIR_DIV:
      case MULIB_MIR_MOD:
      case MULIB_MIR_EQ:
      case MULIB_MIR_LT:
      case MULIB_MIR_LE:
        return 2;
      default:
        return -1;
      }
  }
  mulib_mir_status_t
  mulib_mir_validate (const mulib_mir_program_t *p, size_t limit,
                      mulib_mir_diagnostic_t *d)
  {
    size_t k = 0;
    mulib_mir_status_t s = MULIB_MIR_OK;
    if (!p || !p->code || !p->count)
      {
        s = MULIB_MIR_INVALID;
        goto done;
      }
    if (p->count > limit || p->count > SIZE_MAX / sizeof (*p->code))
      {
        s = MULIB_MIR_RESOURCE_LIMIT;
        goto done;
      }
    for (k = 0; k < p->count; ++k)
      {
        const mulib_mir_instruction_t *i = &p->code[k];
        int arity = mulib_mir_arity (i->opcode);
        if (arity < 0)
          {
            s = MULIB_MIR_BAD_OPCODE;
            break;
          }
        if ((arity >= 1 && i->a >= k) || (arity == 2 && i->b >= k)
            || (arity == 0 && i->a != MULIB_MIR_NONE)
            || (arity < 2 && i->b != MULIB_MIR_NONE))
          {
            s = MULIB_MIR_BAD_REFERENCE;
            break;
          }
        if (i->opcode == MULIB_MIR_RETURN)
          {
            if (k != p->count - 1 || i->type != MULIB_MIR_VOID || i->immediate)
              {
                s = MULIB_MIR_BAD_RETURN;
                break;
              }
          }
        else if (i->type != MULIB_MIR_I64 && i->type != MULIB_MIR_BOOL)
          {
            s = MULIB_MIR_BAD_TYPE;
            break;
          }
        if (i->opcode != MULIB_MIR_CONST && i->opcode != MULIB_MIR_PARAM
            && i->immediate)
          {
            s = MULIB_MIR_INVALID;
            break;
          }
        if (i->opcode == MULIB_MIR_CONST && i->type == MULIB_MIR_BOOL
            && i->immediate != 0 && i->immediate != 1)
          {
            s = MULIB_MIR_BAD_TYPE;
            break;
          }
        if (i->opcode == MULIB_MIR_PARAM
            && (i->immediate < 0
                || (uintmax_t)i->immediate >= (uintmax_t)p->parameter_count))
          {
            s = MULIB_MIR_BAD_REFERENCE;
            break;
          }
        if (arity && i->opcode != MULIB_MIR_RETURN)
          {
            mulib_mir_type_t a = p->code[i->a].type;
            mulib_mir_type_t b = arity == 2 ? p->code[i->b].type : a;
            int valid;
            if (i->opcode == MULIB_MIR_COPY)
              valid = i->type == a;
            else if (i->opcode == MULIB_MIR_NOT)
              valid = a == MULIB_MIR_BOOL && i->type == MULIB_MIR_BOOL;
            else if (i->opcode == MULIB_MIR_EQ)
              valid = a == b && i->type == MULIB_MIR_BOOL;
            else if (i->opcode == MULIB_MIR_LT || i->opcode == MULIB_MIR_LE)
              valid
                  = a == MULIB_MIR_I64 && b == a && i->type == MULIB_MIR_BOOL;
            else
              valid = a == MULIB_MIR_I64 && b == a && i->type == MULIB_MIR_I64;
            if (!valid)
              {
                s = MULIB_MIR_BAD_TYPE;
                break;
              }
          }
      }
    if (s == MULIB_MIR_OK && p->code[p->count - 1].opcode != MULIB_MIR_RETURN)
      {
        s = MULIB_MIR_BAD_RETURN;
        k = p->count - 1;
      }
  done:
    if (d)
      {
        d->status = s;
        d->record = k;
      }
    return s;
  }
  mulib_mir_status_t
  mulib_mir_evaluate (mulib_mir_opcode_t op, int64_t a, int64_t b, int64_t imm,
                      int64_t *out)
  {
    int64_t v;
    if (!out)
      return MULIB_MIR_INVALID;
    switch (op)
      {
      case MULIB_MIR_CONST:
        v = imm;
        break;
      case MULIB_MIR_COPY:
        v = a;
        break;
      case MULIB_MIR_ADD:
        if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
          return MULIB_MIR_OVERFLOW;
        v = a + b;
        break;
      case MULIB_MIR_SUB:
        if ((b > 0 && a < INT64_MIN + b) || (b < 0 && a > INT64_MAX + b))
          return MULIB_MIR_OVERFLOW;
        v = a - b;
        break;
      case MULIB_MIR_MUL:
        if (a > 0)
          {
            if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a))
              return MULIB_MIR_OVERFLOW;
          }
        else if (a < 0)
          {
            if ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b))
              return MULIB_MIR_OVERFLOW;
          }
        v = a * b;
        break;
      case MULIB_MIR_DIV:
      case MULIB_MIR_MOD:
        if (!b)
          return MULIB_MIR_DIVIDE_BY_ZERO;
        if (a == INT64_MIN && b == -1)
          return MULIB_MIR_OVERFLOW;
        v = op == MULIB_MIR_DIV ? a / b : a % b;
        break;
      case MULIB_MIR_NEG:
        if (a == INT64_MIN)
          return MULIB_MIR_OVERFLOW;
        v = -a;
        break;
      case MULIB_MIR_EQ:
        v = a == b;
        break;
      case MULIB_MIR_LT:
        v = a < b;
        break;
      case MULIB_MIR_LE:
        v = a <= b;
        break;
      case MULIB_MIR_NOT:
        v = !a;
        break;
      default:
        return MULIB_MIR_INVALID;
      }
    *out = v;
    return MULIB_MIR_OK;
  }
  const char *
  mulib_mir_status_string (mulib_mir_status_t s)
  {
    switch (s)
      {
      case MULIB_MIR_OK:
        return "ok";
      case MULIB_MIR_INVALID:
        return "invalid descriptor or immediate";
      case MULIB_MIR_BAD_OPCODE:
        return "unknown opcode";
      case MULIB_MIR_BAD_REFERENCE:
        return "invalid, forward, cyclic, or unused reference";
      case MULIB_MIR_BAD_TYPE:
        return "type mismatch or noncanonical Boolean";
      case MULIB_MIR_BAD_RETURN:
        return "expected one final void RETURN";
      case MULIB_MIR_RESOURCE_LIMIT:
        return "resource or allocation-size limit";
      case MULIB_MIR_NO_MEMORY:
        return "allocation failed";
      case MULIB_MIR_OVERFLOW:
        return "signed arithmetic overflow";
      case MULIB_MIR_DIVIDE_BY_ZERO:
        return "division by zero";
      case MULIB_MIR_STEP_LIMIT:
        return "instruction budget exhausted";
      case MULIB_MIR_HOST_ERROR:
        return "host callback failed";
      case MULIB_MIR_SUSPENDED:
        return "awaiting host result";
      case MULIB_MIR_FINISHED:
        return "execution finished";
      case MULIB_MIR_IO_ERROR:
        return "output callback failed";
      case MULIB_MIR_UNSUPPORTED:
        return "unsupported target operation or instruction cover";
      default:
        return "unknown MIR status";
      }
  }
  static mulib_mir_status_t
  mulib_mir_flow_error_ (mulib_mir_diagnostic_t *d, mulib_mir_status_t s,
                        size_t record)
  {
    if (d) { d->status = s; d->record = record; }
    return s;
  }
  int
  mulib_mir_native_name (const char *s)
  {
    size_t k;
    if (!s || !((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')
                || *s == '_')) return 0;
    for (k = 1; k <= 127; ++k)
      {
        unsigned char c = (unsigned char)s[k];
        if (!c) return 1;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9') || c == '_')) return 0;
      }
    return 0;
  }
  int
  mulib_mir_native_arity (int op)
  {
    if (op < 0 || op >= MULIB_MIR_NATIVE_OPCODE_COUNT) return -1;
    if ((op >= MULIB_MIR_NATIVE_ADD && op <= MULIB_MIR_NATIVE_ULT)
        || op == MULIB_MIR_NATIVE_STORE) return 2;
    if (op == MULIB_MIR_NATIVE_COPY || op == MULIB_MIR_NATIVE_LOAD
        || op == MULIB_MIR_NATIVE_BRANCH || op == MULIB_MIR_NATIVE_RETURN)
      return 1;
    return 0;
  }
  int
  mulib_mir_native_defines (int op)
  {
    return (op >= MULIB_MIR_NATIVE_CONST && op <= MULIB_MIR_NATIVE_LOAD)
           || op == MULIB_MIR_NATIVE_CALL;
  }
  int
  mulib_mir_native_effect (int op)
  {
    return op == MULIB_MIR_NATIVE_LOAD || op == MULIB_MIR_NATIVE_STORE
           || op == MULIB_MIR_NATIVE_CALL;
  }
  mulib_mir_native_instruction_t
  mulib_mir_native_instruction (mulib_mir_native_opcode_t op)
  {
    mulib_mir_native_instruction_t i;
    i.opcode = op;
    i.destination = i.a = i.b = i.target = MULIB_MIR_NONE;
    i.immediate = 0;
    return i;
  }
  void
  mulib_mir_native_free (mulib_mir_native_buffer_t *b)
  {
    size_t k;
    if (!b) return;
    if (b->symbols)
      for (k = 0; k < b->symbol_count; ++k) free (b->symbols[k]);
    free (b->code); free (b->blocks); free (b->arguments); free (b->symbols);
    memset (b, 0, sizeof (*b));
  }
  mulib_mir_status_t
  mulib_mir_native_init (mulib_mir_native_buffer_t *out, size_t n, size_t nb,
                         size_t nr, size_t np, size_t na, size_t ns,
                         size_t limit)
  {
    mulib_mir_native_buffer_t b;
    if (!out || out->code || out->blocks || out->arguments || out->symbols
        || out->count || out->block_count || out->register_count
        || out->parameter_count || out->argument_count || out->symbol_count)
      return MULIB_MIR_INVALID;
    if (n > limit || nb > limit || nr > limit || np > limit || na > limit
        || ns > limit || n > SIZE_MAX / sizeof (*b.code)
        || nb > SIZE_MAX / sizeof (*b.blocks)
        || nr > SIZE_MAX / sizeof (uint64_t)
        || np > SIZE_MAX / sizeof (uint64_t)
        || na > SIZE_MAX / sizeof (*b.arguments)
        || ns > SIZE_MAX / sizeof (*b.symbols)) return MULIB_MIR_RESOURCE_LIMIT;
    memset (&b, 0, sizeof (b));
    b.count = n; b.block_count = nb; b.register_count = nr;
    b.parameter_count = np; b.argument_count = na; b.symbol_count = ns;
    if (n) b.code = (mulib_mir_native_instruction_t *)calloc (n, sizeof (*b.code));
    if (nb) b.blocks = (mulib_mir_native_block_t *)calloc (nb, sizeof (*b.blocks));
    if (na) b.arguments = (size_t *)calloc (na, sizeof (*b.arguments));
    if (ns) b.symbols = (char **)calloc (ns, sizeof (*b.symbols));
    if ((n && !b.code) || (nb && !b.blocks) || (na && !b.arguments)
        || (ns && !b.symbols))
      { mulib_mir_native_free (&b); return MULIB_MIR_NO_MEMORY; }
    *out = b;
    return MULIB_MIR_OK;
  }
  mulib_mir_status_t
  mulib_mir_native_symbol (mulib_mir_native_buffer_t *b, size_t k, const char *s)
  {
    char *copy;
    size_t n;
    if (!b || k >= b->symbol_count || !b->symbols || !mulib_mir_native_name (s))
      return MULIB_MIR_INVALID;
    n = strlen (s) + 1;
    copy = (char *)malloc (n);
    if (!copy) return MULIB_MIR_NO_MEMORY;
    memcpy (copy, s, n); free (b->symbols[k]); b->symbols[k] = copy;
    return MULIB_MIR_OK;
  }
  mulib_mir_native_program_t
  mulib_mir_native_view (const mulib_mir_native_buffer_t *b)
  {
    mulib_mir_native_program_t p;
    memset (&p, 0, sizeof (p));
    if (b)
      {
        p.code = b->code; p.count = b->count; p.blocks = b->blocks;
        p.block_count = b->block_count; p.register_count = b->register_count;
        p.parameter_count = b->parameter_count; p.arguments = b->arguments;
        p.argument_count = b->argument_count;
        p.symbols = (const char *const *)b->symbols; p.symbol_count = b->symbol_count;
      }
    return p;
  }
  mulib_mir_status_t
  mulib_mir_native_validate (const mulib_mir_native_program_t *p, size_t limit,
                             mulib_mir_diagnostic_t *d)
  {
    size_t k = 0, b, end = 0;
    mulib_mir_status_t s = MULIB_MIR_INVALID;
    if (!p || !p->code || !p->count || !p->blocks || !p->block_count
        || (p->argument_count && !p->arguments)
        || (p->symbol_count && !p->symbols)) goto done;
    s = MULIB_MIR_RESOURCE_LIMIT;
    if (p->count > limit || p->block_count > limit || p->register_count > limit
        || p->parameter_count > limit || p->argument_count > limit
        || p->symbol_count > limit || p->count > SIZE_MAX / sizeof (*p->code)
        || p->block_count > SIZE_MAX / sizeof (*p->blocks)
        || p->argument_count > SIZE_MAX / sizeof (*p->arguments)
        || p->symbol_count > SIZE_MAX / sizeof (*p->symbols)
        || p->register_count > SIZE_MAX / sizeof (uint64_t)
        || p->parameter_count > SIZE_MAX / sizeof (uint64_t)) goto done;
    s = MULIB_MIR_BAD_REFERENCE;
    for (k = 0; k < p->argument_count; ++k)
      if (p->arguments[k] >= p->register_count) goto done;
    s = MULIB_MIR_INVALID;
    for (k = 0; k < p->symbol_count; ++k)
      if (!mulib_mir_native_name (p->symbols[k])) goto done;
    for (b = 0; b < p->block_count; ++b)
      {
        const mulib_mir_native_block_t *bb = &p->blocks[b];
        k = bb->first;
        s = MULIB_MIR_BAD_REFERENCE;
        if (bb->first != end || !bb->count || bb->count > p->count - end) goto done;
        end += bb->count;
        for (; k < end; ++k)
          {
            const mulib_mir_native_instruction_t *i = &p->code[k];
            int arity = mulib_mir_native_arity (i->opcode);
            int term = i->opcode >= MULIB_MIR_NATIVE_JUMP;
            s = MULIB_MIR_BAD_OPCODE;
            if (arity < 0) goto done;
            s = MULIB_MIR_BAD_RETURN;
            if (term != (k == end - 1)) goto done;
            s = MULIB_MIR_BAD_REFERENCE;
            if (mulib_mir_native_defines (i->opcode)
                ? i->destination >= p->register_count
                : i->destination != MULIB_MIR_NONE) goto done;
            if (i->opcode == MULIB_MIR_NATIVE_CALL)
              {
                if (i->a > p->argument_count || i->b > p->argument_count - i->a
                    || i->target >= p->symbol_count) goto done;
              }
            else
              {
                if (i->opcode == MULIB_MIR_NATIVE_PARAM)
                  { if (i->a >= p->parameter_count) goto done; }
                else if (arity ? i->a >= p->register_count : i->a != MULIB_MIR_NONE)
                  goto done;
                if (i->opcode == MULIB_MIR_NATIVE_BRANCH)
                  { if (i->b >= p->block_count) goto done; }
                else if (arity == 2 ? i->b >= p->register_count : i->b != MULIB_MIR_NONE)
                  goto done;
                if (i->opcode == MULIB_MIR_NATIVE_JUMP || i->opcode == MULIB_MIR_NATIVE_BRANCH)
                  { if (i->target >= p->block_count) goto done; }
                else if (i->target != MULIB_MIR_NONE) goto done;
              }
            s = MULIB_MIR_INVALID;
            if (i->opcode != MULIB_MIR_NATIVE_CONST && i->immediate) goto done;
          }
      }
    s = end == p->count ? MULIB_MIR_OK : MULIB_MIR_BAD_REFERENCE;
  done:
    return mulib_mir_flow_error_ (d, s, k);
  }
  mulib_mir_status_t
  mulib_mir_native_clone (const mulib_mir_native_program_t *p, size_t limit,
                          mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *d)
  {
    mulib_mir_native_buffer_t b;
    mulib_mir_status_t s;
    size_t k;
    memset (&b, 0, sizeof (b));
    s = mulib_mir_native_validate (p, limit, d);
    if (s != MULIB_MIR_OK) return s;
    if (!out || out->code || out->blocks || out->arguments || out->symbols
        || out->count || out->block_count || out->register_count
        || out->parameter_count || out->argument_count || out->symbol_count)
      return mulib_mir_flow_error_ (d, MULIB_MIR_INVALID, 0);
    s = mulib_mir_native_init (&b, p->count, p->block_count, p->register_count,
        p->parameter_count, p->argument_count, p->symbol_count, limit);
    if (s != MULIB_MIR_OK) return mulib_mir_flow_error_ (d, s, 0);
    memcpy (b.code, p->code, p->count * sizeof (*b.code));
    memcpy (b.blocks, p->blocks, p->block_count * sizeof (*b.blocks));
    if (p->argument_count) memcpy (b.arguments, p->arguments, p->argument_count * sizeof (*b.arguments));
    for (k = 0; k < p->symbol_count; ++k)
      if ((s = mulib_mir_native_symbol (&b, k, p->symbols[k])) != MULIB_MIR_OK) break;
    if (s == MULIB_MIR_OK) { *out = b; memset (&b, 0, sizeof (b)); }
    mulib_mir_native_free (&b);
    return mulib_mir_flow_error_ (d, s, k);
  }
  static int
  mulib_mir_flow_type_ (mulib_mir_type_t t)
  {
    return t == MULIB_MIR_I64 || t == MULIB_MIR_BOOL || t == MULIB_MIR_F64
           || t == MULIB_MIR_WORD;
  }
  static mulib_mir_status_t
  mulib_mir_flow_signature_ (const mulib_mir_signature_t *s, size_t limit)
  {
    size_t k;
    if (!mulib_mir_flow_type_ (s->result)
        || (s->parameter_count && !s->parameters))
      return MULIB_MIR_BAD_TYPE;
    if (s->parameter_count > limit
        || s->parameter_count > SIZE_MAX / sizeof (*s->parameters))
      return MULIB_MIR_RESOURCE_LIMIT;
    for (k = 0; k < s->parameter_count; ++k)
      if (!mulib_mir_flow_type_ (s->parameters[k]))
        return MULIB_MIR_BAD_TYPE;
    return MULIB_MIR_OK;
  }
  mulib_mir_status_t
  mulib_mir_flow_validate (const mulib_mir_flow_program_t *p, size_t limit,
                           mulib_mir_diagnostic_t *d)
  {
    size_t k = 0, total = 0;
    mulib_mir_status_t s = MULIB_MIR_OK;
    if (!p || !p->code || !p->count || !p->register_types
        || !p->register_count || (p->host_count && !p->hosts))
      { s = MULIB_MIR_INVALID; goto done; }
    if (p->count > limit || p->register_count > limit || p->host_count > limit
        || p->count > SIZE_MAX / sizeof (*p->code)
        || p->register_count > SIZE_MAX / sizeof (mulib_mir_value_t)
        || p->register_count > SIZE_MAX / sizeof (*p->register_types)
        || p->host_count > SIZE_MAX / sizeof (*p->hosts))
      { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
    s = mulib_mir_flow_signature_ (&p->signature, limit);
    if (s != MULIB_MIR_OK) goto done;
    if (p->signature.parameter_count > SIZE_MAX / sizeof (mulib_mir_value_t))
      { s = MULIB_MIR_RESOURCE_LIMIT; goto done; }
    for (k = 0; k < p->register_count; ++k)
      if (!mulib_mir_flow_type_ (p->register_types[k]))
        { s = MULIB_MIR_BAD_TYPE; goto done; }
    for (k = 0; k < p->host_count; ++k)
      {
        s = mulib_mir_flow_signature_ (&p->hosts[k], limit - total);
        if (s != MULIB_MIR_OK) goto done;
        total += p->hosts[k].parameter_count;
      }
    for (k = 0; k < p->count; ++k)
      {
        const mulib_mir_flow_instruction_t *i = &p->code[k];
        mulib_mir_type_t a = MULIB_MIR_VOID, b = MULIB_MIR_VOID;
        mulib_mir_type_t t = MULIB_MIR_VOID;
        int arity = 0, destination = 1, target = 0;
        switch (i->opcode)
          {
          case MULIB_MIR_FLOW_CONST: case MULIB_MIR_FLOW_PARAM: break;
          case MULIB_MIR_FLOW_COPY: case MULIB_MIR_FLOW_NEG:
          case MULIB_MIR_FLOW_NOT: case MULIB_MIR_FLOW_I64_F64:
          case MULIB_MIR_FLOW_F64_I64: arity = 1; break;
          case MULIB_MIR_FLOW_ADD: case MULIB_MIR_FLOW_SUB:
          case MULIB_MIR_FLOW_MUL: case MULIB_MIR_FLOW_DIV:
          case MULIB_MIR_FLOW_MOD: case MULIB_MIR_FLOW_EQ:
          case MULIB_MIR_FLOW_LT: case MULIB_MIR_FLOW_LE:
          case MULIB_MIR_FLOW_WRAP_ADD: case MULIB_MIR_FLOW_WRAP_SUB:
          case MULIB_MIR_FLOW_WRAP_MUL: case MULIB_MIR_FLOW_AND:
          case MULIB_MIR_FLOW_OR: case MULIB_MIR_FLOW_XOR:
          case MULIB_MIR_FLOW_SHL: case MULIB_MIR_FLOW_SHR: arity = 2; break;
          case MULIB_MIR_FLOW_JUMP: destination = 0; target = 1; break;
          case MULIB_MIR_FLOW_BRANCH:
            destination = 0; target = 1; arity = 1; break;
          case MULIB_MIR_FLOW_CALL: target = 1; break;
          case MULIB_MIR_FLOW_RETURN: destination = 0; arity = 1; break;
          default: s = MULIB_MIR_BAD_OPCODE; goto done;
          }
        if ((destination && i->destination >= p->register_count)
            || (!destination && i->destination != MULIB_MIR_NONE)
            || (arity && i->a >= p->register_count)
            || (arity == 2 && i->b >= p->register_count)
            || (!target && i->target != MULIB_MIR_NONE))
          { s = MULIB_MIR_BAD_REFERENCE; goto done; }
        if (i->opcode != MULIB_MIR_FLOW_CALL
            && ((arity < 2 && i->b != MULIB_MIR_NONE)
                || (!arity && i->opcode != MULIB_MIR_FLOW_PARAM
                    && i->a != MULIB_MIR_NONE)))
          { s = MULIB_MIR_BAD_REFERENCE; goto done; }
        if (i->opcode != MULIB_MIR_FLOW_CONST && i->immediate.word)
          { s = MULIB_MIR_INVALID; goto done; }
        if (destination) t = p->register_types[i->destination];
        if (arity) a = p->register_types[i->a];
        if (arity == 2) b = p->register_types[i->b];
        switch (i->opcode)
          {
          case MULIB_MIR_FLOW_CONST:
            if (t == MULIB_MIR_BOOL && i->immediate.integer != 0
                && i->immediate.integer != 1) s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_PARAM:
            if (i->a >= p->signature.parameter_count) s = MULIB_MIR_BAD_REFERENCE;
            else if (t != p->signature.parameters[i->a]) s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_COPY:
            if (t != a) s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_EQ:
            if (a != b || t != MULIB_MIR_BOOL) s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_LT: case MULIB_MIR_FLOW_LE:
            if (a != b || t != MULIB_MIR_BOOL || a == MULIB_MIR_BOOL)
              s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_NOT:
            if (a != MULIB_MIR_BOOL || t != a) s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_I64_F64:
            if (a != MULIB_MIR_I64 || t != MULIB_MIR_F64) s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_F64_I64:
            if (a != MULIB_MIR_F64 || t != MULIB_MIR_I64) s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_WRAP_ADD: case MULIB_MIR_FLOW_WRAP_SUB:
          case MULIB_MIR_FLOW_WRAP_MUL: case MULIB_MIR_FLOW_AND:
          case MULIB_MIR_FLOW_OR: case MULIB_MIR_FLOW_XOR:
          case MULIB_MIR_FLOW_SHL: case MULIB_MIR_FLOW_SHR:
            if (a != b || t != a || (a != MULIB_MIR_WORD && a != MULIB_MIR_I64))
              s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_JUMP: case MULIB_MIR_FLOW_BRANCH:
            if (i->target >= p->count) s = MULIB_MIR_BAD_REFERENCE;
            else if (arity && a != MULIB_MIR_BOOL) s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_RETURN:
            if (a != p->signature.result) s = MULIB_MIR_BAD_TYPE;
            break;
          case MULIB_MIR_FLOW_CALL:
            if (i->target >= p->host_count || i->a > p->register_count
                || i->b > p->register_count - i->a)
              s = MULIB_MIR_BAD_REFERENCE;
            else
              {
                const mulib_mir_signature_t *h = &p->hosts[i->target];
                size_t j;
                if (t != h->result || i->b != h->parameter_count)
                  s = MULIB_MIR_BAD_TYPE;
                else for (j = 0; j < i->b; ++j)
                  if (p->register_types[i->a + j] != h->parameters[j])
                    { s = MULIB_MIR_BAD_TYPE; break; }
              }
            break;
          default:
            if (t != a || (arity == 2 && a != b)
                || (a != MULIB_MIR_I64 && a != MULIB_MIR_F64))
              s = MULIB_MIR_BAD_TYPE;
            break;
          }
        if (s != MULIB_MIR_OK) goto done;
      }
    if (p->code[p->count - 1].opcode != MULIB_MIR_FLOW_RETURN
        && p->code[p->count - 1].opcode != MULIB_MIR_FLOW_JUMP)
      { s = MULIB_MIR_BAD_RETURN; k = p->count - 1; }
  done:
    if (d) { d->status = s; d->record = k; }
    return s;
  }
  void
  mulib_mir_frame_free (mulib_mir_frame_t *f)
  {
    if (f && !f->running)
      { free (f->registers); free (f->arguments); memset (f, 0, sizeof (*f)); }
  }
  mulib_mir_status_t
  mulib_mir_frame_init (mulib_mir_frame_t *f, const mulib_mir_flow_program_t *p,
                        const mulib_mir_value_t *args, size_t argc,
                        mulib_mir_host_fn host, void *user, size_t limit,
                        mulib_mir_diagnostic_t *d)
  {
    mulib_mir_frame_t next;
    mulib_mir_status_t s;
    size_t k;
    if (!f || f->program || f->registers || f->arguments || f->running)
      return mulib_mir_flow_error_ (d, MULIB_MIR_INVALID, 0);
    s = mulib_mir_flow_validate (p, limit, d);
    if (s != MULIB_MIR_OK) return s;
    if (argc != p->signature.parameter_count || (argc && !args)
        || (p->host_count && !host)) return mulib_mir_flow_error_ (d, MULIB_MIR_INVALID, 0);
    for (k = 0; k < argc; ++k)
      if (p->signature.parameters[k] == MULIB_MIR_BOOL
          && args[k].integer != 0 && args[k].integer != 1)
        return mulib_mir_flow_error_ (d, MULIB_MIR_BAD_TYPE, k);
    memset (&next, 0, sizeof (next));
    next.registers = (mulib_mir_value_t *)calloc (p->register_count,
                                                sizeof (*next.registers));
    if (argc) next.arguments = (mulib_mir_value_t *)malloc (argc * sizeof (*args));
    if (!next.registers || (argc && !next.arguments))
      { mulib_mir_frame_free (&next); return mulib_mir_flow_error_ (d, MULIB_MIR_NO_MEMORY, 0); }
    for (k = 0; k < p->register_count; ++k)
      if (p->register_types[k] == MULIB_MIR_F64) next.registers[k].number = 0.0;
    if (argc) memcpy (next.arguments, args, argc * sizeof (*args));
    next.program = p; next.host = host; next.user = user;
    *f = next;
    return MULIB_MIR_OK;
  }
  /* Conversion through unsigned arithmetic gives defined two's-complement
   * wrapping without signed overflow or out-of-range unsigned-to-signed casts. */
  static int64_t
  mulib_mir_flow_signed_ (uint64_t x)
  {
    return x <= INT64_MAX ? (int64_t)x : -1 - (int64_t)(UINT64_MAX - x);
  }
  mulib_mir_status_t
  mulib_mir_frame_step (mulib_mir_frame_t *f)
  {
    const mulib_mir_flow_program_t *p;
    const mulib_mir_flow_instruction_t *i;
    mulib_mir_value_t a, b, v;
    mulib_mir_type_t t = MULIB_MIR_VOID;
    mulib_mir_status_t s = MULIB_MIR_OK;
    size_t next;
    if (!f || !f->program || !f->registers || f->running) return MULIB_MIR_INVALID;
    if (f->status != MULIB_MIR_OK && f->status != MULIB_MIR_STEP_LIMIT)
      return f->status;
    p = f->program;
    if (f->pc >= p->count) return f->status = MULIB_MIR_BAD_REFERENCE;
    if (!f->remaining) return f->status = MULIB_MIR_STEP_LIMIT;
    if (f->steps == SIZE_MAX) return f->status = MULIB_MIR_RESOURCE_LIMIT;
    --f->remaining; ++f->steps;
    i = &p->code[f->pc]; next = f->pc + 1;
    memset (&a, 0, sizeof (a)); memset (&b, 0, sizeof (b)); memset (&v, 0, sizeof (v));
    if (i->a < p->register_count) { a = f->registers[i->a]; t = p->register_types[i->a]; }
    if (i->b < p->register_count) b = f->registers[i->b];
    switch (i->opcode)
      {
      case MULIB_MIR_FLOW_CONST: v = i->immediate; break;
      case MULIB_MIR_FLOW_PARAM: v = f->arguments[i->a]; break;
      case MULIB_MIR_FLOW_COPY: v = a; break;
      case MULIB_MIR_FLOW_ADD: case MULIB_MIR_FLOW_SUB:
      case MULIB_MIR_FLOW_MUL: case MULIB_MIR_FLOW_DIV:
      case MULIB_MIR_FLOW_MOD: case MULIB_MIR_FLOW_NEG:
        if (t == MULIB_MIR_I64)
          {
            static const mulib_mir_opcode_t ops[] = {
              MULIB_MIR_ADD, MULIB_MIR_SUB, MULIB_MIR_MUL,
              MULIB_MIR_DIV, MULIB_MIR_MOD, MULIB_MIR_NEG };
            s = mulib_mir_evaluate (ops[i->opcode - MULIB_MIR_FLOW_ADD],
                                    a.integer, b.integer, 0, &v.integer);
          }
        else switch (i->opcode)
          {
          case MULIB_MIR_FLOW_ADD: v.number = a.number + b.number; break;
          case MULIB_MIR_FLOW_SUB: v.number = a.number - b.number; break;
          case MULIB_MIR_FLOW_MUL: v.number = a.number * b.number; break;
          case MULIB_MIR_FLOW_DIV: v.number = a.number / b.number; break;
          case MULIB_MIR_FLOW_MOD: v.number = fmod (a.number, b.number); break;
          default: v.number = -a.number; break;
          }
        break;
      case MULIB_MIR_FLOW_EQ:
        v.integer = t == MULIB_MIR_F64 ? a.number == b.number
                    : t == MULIB_MIR_WORD ? a.word == b.word : a.integer == b.integer;
        break;
      case MULIB_MIR_FLOW_LT:
        v.integer = t == MULIB_MIR_F64 ? a.number < b.number
                    : t == MULIB_MIR_WORD ? a.word < b.word : a.integer < b.integer;
        break;
      case MULIB_MIR_FLOW_LE:
        v.integer = t == MULIB_MIR_F64 ? a.number <= b.number
                    : t == MULIB_MIR_WORD ? a.word <= b.word : a.integer <= b.integer;
        break;
      case MULIB_MIR_FLOW_NOT: v.integer = !a.integer; break;
      case MULIB_MIR_FLOW_WRAP_ADD: case MULIB_MIR_FLOW_WRAP_SUB:
      case MULIB_MIR_FLOW_WRAP_MUL: case MULIB_MIR_FLOW_AND:
      case MULIB_MIR_FLOW_OR: case MULIB_MIR_FLOW_XOR:
      case MULIB_MIR_FLOW_SHL: case MULIB_MIR_FLOW_SHR:
        {
          uint64_t x = t == MULIB_MIR_I64 ? (uint64_t)a.integer : a.word;
          uint64_t y = t == MULIB_MIR_I64 ? (uint64_t)b.integer : b.word, z = 0;
          switch (i->opcode)
            {
            case MULIB_MIR_FLOW_WRAP_ADD: z = x + y; break;
            case MULIB_MIR_FLOW_WRAP_SUB: z = x - y; break;
            case MULIB_MIR_FLOW_WRAP_MUL: z = x * y; break;
            case MULIB_MIR_FLOW_AND: z = x & y; break;
            case MULIB_MIR_FLOW_OR: z = x | y; break;
            case MULIB_MIR_FLOW_XOR: z = x ^ y; break;
            case MULIB_MIR_FLOW_SHL: z = y < 64 ? x << y : 0; break;
            default: z = y < 64 ? x >> y : 0; break;
            }
          if (t == MULIB_MIR_I64) v.integer = mulib_mir_flow_signed_ (z);
          else v.word = z;
        }
        break;
      case MULIB_MIR_FLOW_I64_F64: v.number = (double)a.integer; break;
      case MULIB_MIR_FLOW_F64_I64:
        if (!(a.number >= -0x1p63 && a.number < 0x1p63)) s = MULIB_MIR_OVERFLOW;
        else v.integer = (int64_t)a.number;
        break;
      case MULIB_MIR_FLOW_JUMP: next = i->target; break;
      case MULIB_MIR_FLOW_BRANCH: if (a.integer) next = i->target; break;
      case MULIB_MIR_FLOW_CALL:
        f->running = 1;
        s = f->host (f->user, i->target, f->registers + i->a, i->b, &v);
        f->running = 0;
        if (s == MULIB_MIR_OK && p->register_types[i->destination] == MULIB_MIR_BOOL
            && v.integer != 0 && v.integer != 1) s = MULIB_MIR_BAD_TYPE;
        if (s != MULIB_MIR_OK && s != MULIB_MIR_SUSPENDED) s = MULIB_MIR_HOST_ERROR;
        break;
      case MULIB_MIR_FLOW_RETURN:
        f->result = a; return f->status = MULIB_MIR_FINISHED;
      default: s = MULIB_MIR_BAD_OPCODE; break;
      }
    if (s == MULIB_MIR_OK)
      {
        if (i->destination != MULIB_MIR_NONE) f->registers[i->destination] = v;
        f->pc = next;
      }
    f->status = s;
    return s;
  }
  mulib_mir_status_t
  mulib_mir_frame_resume (mulib_mir_frame_t *f, mulib_mir_value_t v)
  {
    const mulib_mir_flow_instruction_t *i;
    if (!f || !f->program || f->running || f->status != MULIB_MIR_SUSPENDED
        || f->pc >= f->program->count) return MULIB_MIR_INVALID;
    i = &f->program->code[f->pc];
    if (i->opcode != MULIB_MIR_FLOW_CALL) return MULIB_MIR_INVALID;
    if (f->program->register_types[i->destination] == MULIB_MIR_BOOL
        && v.integer != 0 && v.integer != 1) return MULIB_MIR_BAD_TYPE;
    f->registers[i->destination] = v; ++f->pc;
    return f->status = MULIB_MIR_OK;
  }
  static int
  mulib_mir_flow_put_ (mulib_mir_write_fn writer, void *user, const char *fmt, ...)
  {
    char text[1024];
    va_list ap;
    int n;
    va_start (ap, fmt); n = vsnprintf (text, sizeof (text), fmt, ap); va_end (ap);
    return n >= 0 && (size_t)n < sizeof (text) && writer (user, text, (size_t)n);
  }
  mulib_mir_status_t
  mulib_mir_flow_emit_c (const mulib_mir_flow_program_t *p, const char *name,
                         size_t limit, mulib_mir_write_fn writer, void *user,
                         mulib_mir_diagnostic_t *d)
  {
    size_t k = 0, j;
    mulib_mir_status_t s;
    if (!name || !writer || strncmp (name, "mulib_generated_", 16) || !name[16])
      return mulib_mir_flow_error_ (d, MULIB_MIR_INVALID, 0);
    for (k = 0; name[k]; ++k)
      if (k >= 127 || !((name[k] >= 'a' && name[k] <= 'z')
                       || (name[k] >= 'A' && name[k] <= 'Z')
                       || (name[k] >= '0' && name[k] <= '9') || name[k] == '_'))
        return mulib_mir_flow_error_ (d, MULIB_MIR_INVALID, 0);
    s = mulib_mir_flow_validate (p, limit, d);
    if (s != MULIB_MIR_OK) return s;
    for (k = 0; k < p->count; ++k)
      if (p->code[k].opcode == MULIB_MIR_FLOW_CONST
          && p->register_types[p->code[k].destination] == MULIB_MIR_F64
          && (sizeof (double) != sizeof (uint64_t) || FLT_RADIX != 2
              || DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024))
        return mulib_mir_flow_error_ (d, MULIB_MIR_BAD_TYPE, k);
#define MULIB_MIR_FLOW_PUT_(...) do { if (!mulib_mir_flow_put_ (writer, user, __VA_ARGS__)) return mulib_mir_flow_error_ (d, MULIB_MIR_IO_ERROR, k); } while (0)
    MULIB_MIR_FLOW_PUT_ ("#include \"mulib-mir.h\"\n#include <float.h>\n");
    for (k = 0; k < p->count; ++k)
      if (p->code[k].opcode == MULIB_MIR_FLOW_CONST
          && p->register_types[p->code[k].destination] == MULIB_MIR_F64)
        {
          MULIB_MIR_FLOW_PUT_ ("_Static_assert(sizeof(double)==sizeof(uint64_t) && FLT_RADIX==2 && DBL_MANT_DIG==53 && DBL_MAX_EXP==1024, \"binary64 floating immediates required\");\n");
          break;
        }
    MULIB_MIR_FLOW_PUT_ ("static const mulib_mir_flow_instruction_t %s_code[] = {\n", name);
    for (k = 0; k < p->count; ++k)
      {
        const mulib_mir_flow_instruction_t *i = &p->code[k];
        size_t fields[] = { i->destination, i->a, i->b, i->target };
        MULIB_MIR_FLOW_PUT_ ("{%d,", (int)i->opcode);
        for (j = 0; j < 4; ++j)
          {
            if (fields[j] == MULIB_MIR_NONE) { MULIB_MIR_FLOW_PUT_ ("MULIB_MIR_NONE,"); }
            else { MULIB_MIR_FLOW_PUT_ ("%zu,", fields[j]); }
          }
        if (i->opcode == MULIB_MIR_FLOW_CONST
            && p->register_types[i->destination] == MULIB_MIR_F64)
          {
            uint64_t bits;
            /* Integer object bits preserve negative zero and NaN payloads;
             * decimal/hex floating literals can canonicalize the latter. */
            memcpy (&bits, &i->immediate.number, sizeof (bits));
            MULIB_MIR_FLOW_PUT_ ("{.word=UINT64_C(%" PRIu64 ")}", bits);
          }
        else if (i->opcode == MULIB_MIR_FLOW_CONST
                 && p->register_types[i->destination] != MULIB_MIR_WORD)
          {
            if (i->immediate.integer == INT64_MIN) { MULIB_MIR_FLOW_PUT_ ("{.integer=INT64_MIN}"); }
            else { MULIB_MIR_FLOW_PUT_ ("{.integer=%sINT64_C(%" PRIu64 ")}", i->immediate.integer < 0 ? "-" : "",
                                        (uint64_t)(i->immediate.integer < 0 ? -i->immediate.integer : i->immediate.integer)); }
          }
        else { MULIB_MIR_FLOW_PUT_ ("{.word=UINT64_C(%" PRIu64 ")}", i->immediate.word); }
        MULIB_MIR_FLOW_PUT_ ("},\n");
      }
    MULIB_MIR_FLOW_PUT_ ("};\nstatic const mulib_mir_type_t %s_types[]={", name);
    for (k = 0; k < p->register_count; ++k) { MULIB_MIR_FLOW_PUT_ ("%d,", (int)p->register_types[k]); }
    MULIB_MIR_FLOW_PUT_ ("};\n");
    for (k = 0; k <= p->host_count; ++k)
      {
        const mulib_mir_signature_t *sig = k ? &p->hosts[k - 1] : &p->signature;
        MULIB_MIR_FLOW_PUT_ ("static const mulib_mir_type_t %s_args%zu[]={", name, k);
        if (!sig->parameter_count) { MULIB_MIR_FLOW_PUT_ ("MULIB_MIR_VOID"); }
        for (j = 0; j < sig->parameter_count; ++j) { MULIB_MIR_FLOW_PUT_ ("%d,", (int)sig->parameters[j]); }
        MULIB_MIR_FLOW_PUT_ ("};\n");
      }
    MULIB_MIR_FLOW_PUT_ ("static const mulib_mir_signature_t %s_hosts[]={\n", name);
    if (!p->host_count) { MULIB_MIR_FLOW_PUT_ ("{NULL,0,MULIB_MIR_VOID}\n"); }
    for (k = 0; k < p->host_count; ++k)
      { MULIB_MIR_FLOW_PUT_ ("{%s_args%zu,%zu,%d},\n", name, k + 1, p->hosts[k].parameter_count, (int)p->hosts[k].result); }
    MULIB_MIR_FLOW_PUT_ ("};\nconst mulib_mir_flow_program_t *%s_program(void) {\n"
                        "static const mulib_mir_flow_program_t p={%s_code,%zu,%s_types,%zu,"
                        "{%s_args0,%zu,%d},%s_hosts,%zu}; return &p; }\n",
                        name, name, p->count, name, p->register_count, name,
                        p->signature.parameter_count, (int)p->signature.result, name, p->host_count);
    MULIB_MIR_FLOW_PUT_ ("mulib_mir_status_t %s(mulib_mir_frame_t *f) {\n"
                        "mulib_mir_status_t s;\nif (!f || f->program != %s_program() || f->running) return MULIB_MIR_INVALID;\n"
                        "switch(f->pc) {\n", name, name);
    for (k = 0; k < p->count; ++k) { MULIB_MIR_FLOW_PUT_ ("case %zu: goto L%zu;\n", k, k); }
    MULIB_MIR_FLOW_PUT_ ("default: return MULIB_MIR_BAD_REFERENCE; }\n");
    for (k = 0; k < p->count; ++k)
      {
        const mulib_mir_flow_instruction_t *i = &p->code[k];
        MULIB_MIR_FLOW_PUT_ ("L%zu: s=mulib_mir_frame_step(f); if(s!=MULIB_MIR_OK) return s;\n", k);
        if (i->opcode == MULIB_MIR_FLOW_RETURN)
          { MULIB_MIR_FLOW_PUT_ ("return MULIB_MIR_FINISHED;\n"); }
        else if (i->opcode == MULIB_MIR_FLOW_JUMP)
          { MULIB_MIR_FLOW_PUT_ ("goto L%zu;\n", i->target); }
        else if (i->opcode == MULIB_MIR_FLOW_BRANCH)
          { MULIB_MIR_FLOW_PUT_ ("if(f->pc==%zu) goto L%zu; else goto L%zu;\n", i->target, i->target, k + 1); }
        else { MULIB_MIR_FLOW_PUT_ ("goto L%zu;\n", k + 1); }
      }
    MULIB_MIR_FLOW_PUT_ ("}\n");
#undef MULIB_MIR_FLOW_PUT_
    return MULIB_MIR_OK;
  }
#ifdef __cplusplus
}
#endif
#endif
