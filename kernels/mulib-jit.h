/**
 * @file mulib-jit.h
 * @brief RW-to-RX native memory and a typed MIR baseline compiler, Linux AMD64.
 * Define JIT, DYNCALL, VMWEAVE, MIR, EGRAPH and REWRITE switches in one C11 TU.
 */
#ifndef MULIB_JIT_H
#define MULIB_JIT_H
#include "mulib-dyncall.h"
#include "mulib-vmweave.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_jit mulib_jit_t;
  typedef enum mulib_jit_result
  {
    MULIB_JIT_OK = 0,
    MULIB_JIT_EINVAL = -1,
    MULIB_JIT_ENOTSUP = -2,
    MULIB_JIT_ENOMEM = -3,
    MULIB_JIT_EOS = -4,
    MULIB_JIT_ESTATE = -5,
    MULIB_JIT_ERANGE = -6,
    MULIB_JIT_EMIR = -7
  } mulib_jit_result;
  /** @brief Whether this build implements native execution, not OS permission.
   */
  int mulib_jit_supported (void);
  /** @brief Allocate zeroed RW, non-executable memory; *output must initially
   * be NULL. Capacity must be nonzero. Output unchanged on failure. No feature
   * macros needed.
   */
  int mulib_jit_create (mulib_jit_t **output, size_t capacity);
  /** @brief Copy into the RW allocation. Sealed objects reject all writes. */
  int mulib_jit_write (mulib_jit_t *jit, size_t offset, const void *bytes,
                       size_t length);
  /** @brief Irreversibly make nonempty code RX, never RWX; idempotent on
   * success. */
  int mulib_jit_seal (mulib_jit_t *jit);
  /** @brief Run sealed, trusted int64_t(void) SysV code at offset zero.
   * Not a sandbox: caller must supply valid ABI-conforming host machine code.
   * No executable pointer escapes; allocation remains live throughout the
   * call.
   */
  int mulib_jit_run_i64_0 (const mulib_jit_t *jit, int64_t *result);
  /** @brief Unmap and free; on OS failure retain ownership. NULL is harmless.
   */
  int mulib_jit_destroy (mulib_jit_t *jit);
  typedef struct mulib_jit_flow mulib_jit_flow_t;
  /** Compile and own a validated MIR register program. Emits native control
   * flow and calls the shared scalar/runtime helpers at each instruction.
   * Optional optimization is VMWeave's fault-preserving local constant fold.
   * Input is deep copied. *output must be NULL. No interpreted fallback.
   */
  int mulib_jit_flow_compile (const mulib_mir_flow_program_t *program,
                              size_t limit, int optimize,
                              mulib_jit_flow_t **output,
                              mulib_mir_diagnostic_t *diagnostic);
  /** Borrow the image used to initialize execution frames. */
  const mulib_mir_flow_program_t *
  mulib_jit_flow_program (const mulib_jit_flow_t *jit);
  /** Execute native code with a frame initialized from this exact image.
   * Same budget, suspension, and fault contracts as mulib_vmweave_run.
   */
  mulib_mir_status_t mulib_jit_flow_run (const mulib_jit_flow_t *jit,
                                        mulib_mir_frame_t *frame, size_t budget);
  /** NULL safe; ownership retained if native unmapping fails. */
  int mulib_jit_flow_destroy (mulib_jit_flow_t *jit);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_JIT) && !defined(MULIB_JIT_IMPLEMENTED)
#define MULIB_JIT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#if defined(__linux__) && defined(__x86_64__) && !defined(__ILP32__)
#define MULIB_JIT_NATIVE_ 1
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  struct mulib_jit
  {
    void *mapping, *memory;
    size_t capacity, mapped, size;
    int sealed;
  };
  int
  mulib_jit_supported (void)
  {
#ifdef MULIB_JIT_NATIVE_
    return sizeof (void *) == sizeof (mulib_dyncall_i64_0_fn);
#else
  return 0;
#endif
  }
  int
  mulib_jit_create (mulib_jit_t **out, size_t cap)
  {
    if (!out || *out || !cap)
      return MULIB_JIT_EINVAL;
#ifdef MULIB_JIT_NATIVE_
    {
      long page = sysconf (_SC_PAGESIZE);
      size_t mapped, remainder;
      mulib_jit_t *j;
      int fd;
      void *memory;
      if (!mulib_jit_supported ())
        return MULIB_JIT_ENOTSUP;
      if (page <= 0 || (uintmax_t)page > SIZE_MAX)
        return MULIB_JIT_EOS;
      /* A readable zero prefix permits native tooling (e.g. Clang's indirect
       * function sanitizer) to inspect bytes preceding an uninstrumented
       * entry. */
      if (cap > SIZE_MAX - 16)
        return MULIB_JIT_ERANGE;
      mapped = cap + 16;
      remainder = mapped % (size_t)page;
      if (remainder && mapped > SIZE_MAX - ((size_t)page - remainder))
        return MULIB_JIT_ERANGE;
      mapped += remainder ? (size_t)page - remainder : 0;
      j = (mulib_jit_t *)calloc (1, sizeof (*j));
      if (!j)
        return MULIB_JIT_ENOMEM;
      /* /dev/zero avoids MAP_ANONYMOUS and late feature-test macro changes. */
      fd = open ("/dev/zero", O_RDWR);
      if (fd < 0)
        {
          free (j);
          return MULIB_JIT_EOS;
        }
      memory = mmap (NULL, mapped, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
      (void)close (fd);
      if (memory == MAP_FAILED)
        {
          free (j);
          return MULIB_JIT_EOS;
        }
      j->mapping = memory;
      j->memory = (unsigned char *)memory + 16;
      j->capacity = cap;
      j->mapped = mapped;
      *out = j;
      return 0;
    }
#else
  return MULIB_JIT_ENOTSUP;
#endif
  }
  int
  mulib_jit_write (mulib_jit_t *j, size_t at, const void *bytes, size_t n)
  {
    if (!j || (!bytes && n))
      return MULIB_JIT_EINVAL;
    if (j->sealed)
      return MULIB_JIT_ESTATE;
    if (at > j->capacity || n > j->capacity - at)
      return MULIB_JIT_ERANGE;
    if (n)
      {
        memcpy ((unsigned char *)j->memory + at, bytes, n);
        if (at + n > j->size)
          j->size = at + n;
      }
    return 0;
  }
  int
  mulib_jit_seal (mulib_jit_t *j)
  {
    if (!j)
      return MULIB_JIT_EINVAL;
    if (j->sealed)
      return 0;
    if (!j->size)
      return MULIB_JIT_ESTATE;
#ifdef MULIB_JIT_NATIVE_
    /* AMD64 instruction/data caches are coherent. */
    if (mprotect (j->mapping, j->mapped, PROT_READ | PROT_EXEC) != 0)
      return MULIB_JIT_EOS;
    j->sealed = 1;
    return 0;
#else
  return MULIB_JIT_ENOTSUP;
#endif
  }
  int
  mulib_jit_run_i64_0 (const mulib_jit_t *j, int64_t *out)
  {
    if (!j || !out)
      return MULIB_JIT_EINVAL;
    if (!j->sealed)
      return MULIB_JIT_ESTATE;
#ifdef MULIB_JIT_NATIVE_
    {
      mulib_dyncall_i64_0_fn entry;
      if (sizeof (entry) != sizeof (j->memory))
        return MULIB_JIT_ENOTSUP;
      /* Linux AMD64 ABI extension, not an ISO C object/function pointer cast.
       */
      memcpy (&entry, &j->memory, sizeof (entry));
      return mulib_dyncall_i64_0 (entry, out);
    }
#else
  return MULIB_JIT_ENOTSUP;
#endif
  }
  int
  mulib_jit_destroy (mulib_jit_t *j)
  {
    if (!j)
      return 0;
#ifdef MULIB_JIT_NATIVE_
    if (munmap (j->mapping, j->mapped) != 0)
      return MULIB_JIT_EOS;
    free (j);
    return 0;
#else
  return MULIB_JIT_ENOTSUP;
#endif
  }
  struct mulib_jit_flow
  {
    mulib_jit_t *native;
    mulib_vmweave_program_t *image;
  };
  const mulib_mir_flow_program_t *
  mulib_jit_flow_program (const mulib_jit_flow_t *j)
  {
    return j ? mulib_vmweave_program_view (j->image) : NULL;
  }
  int
  mulib_jit_flow_destroy (mulib_jit_flow_t *j)
  {
    int s;
    if (!j) return MULIB_JIT_OK;
    s = mulib_jit_destroy (j->native);
    if (s) return s;
    mulib_vmweave_program_destroy (j->image); free (j);
    return MULIB_JIT_OK;
  }
#ifdef MULIB_JIT_NATIVE_
  static void
  mulib_jit_flow_u32_ (unsigned char *p, uint32_t n)
  {
    unsigned k;
    for (k = 0; k < 4; ++k) p[k] = (unsigned char)(n >> (k * 8));
  }
  static void
  mulib_jit_flow_rel_ (unsigned char *p, size_t at, size_t target)
  {
    int64_t delta = (int64_t)target - (int64_t)(at + 4);
    mulib_jit_flow_u32_ (p + at, (uint32_t)delta);
  }
  static int
  mulib_jit_flow_native_ (mulib_jit_flow_t *j)
  {
    const mulib_mir_flow_program_t *p = mulib_jit_flow_program (j);
    unsigned char *bytes;
    size_t size, exit_at, table_at, k;
    uint64_t address = 0;
    mulib_mir_status_t (*step)(mulib_mir_frame_t *) = mulib_mir_frame_step;
    int s;
    /* Fixed-size blocks simplify all relocations; every displacement and
     * table entry is checked by this whole-image signed-32-bit bound. */
    if (p->count > ((size_t)INT32_MAX - 72) / 68
        || offsetof (mulib_mir_frame_t, pc) > INT32_MAX
        || sizeof (step) != sizeof (address)) return MULIB_JIT_ERANGE;
    exit_at = 64 + 64 * p->count; table_at = exit_at + 8;
    size = table_at + 4 * p->count;
    bytes = (unsigned char *)malloc (size);
    if (!bytes) return MULIB_JIT_ENOMEM;
    memset (bytes, 0x90, size); memcpy (&address, &step, sizeof (address));
    /* endbr64; push r12; mov r12,rdi; load saved pc; RIP-relative jump table. */
    memcpy (bytes, "\xf3\x0f\x1e\xfa\x41\x54\x49\x89\xfc"
                   "\x49\x8b\x84\x24", 13);
    mulib_jit_flow_u32_ (bytes + 13, (uint32_t)offsetof (mulib_mir_frame_t, pc));
    memcpy (bytes + 17, "\x48\x8d\x15", 3);
    mulib_jit_flow_rel_ (bytes, 20, table_at);
    memcpy (bytes + 24, "\x48\x63\x04\x82\x48\x01\xd0\xff\xe0", 9);
    for (k = 0; k < p->count; ++k)
      {
        const mulib_mir_flow_instruction_t *i = &p->code[k];
        size_t at = 64 + 64 * k, n;
        int64_t delta = (int64_t)at - (int64_t)table_at;
        mulib_jit_flow_u32_ (bytes + table_at + 4 * k, (uint32_t)delta);
        memcpy (bytes + at, "\xf3\x0f\x1e\xfa\x4c\x89\xe7\x48\xb8", 9);
        for (n = 0; n < 8; ++n) bytes[at + 9 + n] = (unsigned char)(address >> (n * 8));
        memcpy (bytes + at + 17, "\xff\xd0\x85\xc0\x0f\x85", 6);
        mulib_jit_flow_rel_ (bytes, at + 23, exit_at);
        if (i->opcode == MULIB_MIR_FLOW_BRANCH)
          {
            /* cmp [r12+pc], target; je true; jmp false. */
            memcpy (bytes + at + 27, "\x49\x81\xbc\x24", 4);
            mulib_jit_flow_u32_ (bytes + at + 31, (uint32_t)offsetof (mulib_mir_frame_t, pc));
            mulib_jit_flow_u32_ (bytes + at + 35, (uint32_t)i->target);
            memcpy (bytes + at + 39, "\x0f\x84", 2);
            mulib_jit_flow_rel_ (bytes, at + 41, 64 + 64 * i->target);
            bytes[at + 45] = 0xe9;
            mulib_jit_flow_rel_ (bytes, at + 46, at + 64);
          }
        else
          {
            size_t target = i->opcode == MULIB_MIR_FLOW_JUMP ? 64 + 64 * i->target
                            : i->opcode == MULIB_MIR_FLOW_RETURN ? exit_at : at + 64;
            bytes[at + 27] = 0xe9;
            mulib_jit_flow_rel_ (bytes, at + 28, target);
          }
      }
    memcpy (bytes + exit_at, "\x41\x5c\xc3", 3);
    s = mulib_jit_create (&j->native, size);
    if (!s) s = mulib_jit_write (j->native, 0, bytes, size);
    if (!s) s = mulib_jit_seal (j->native);
    free (bytes);
    return s;
  }
#endif
  int
  mulib_jit_flow_compile (const mulib_mir_flow_program_t *p, size_t limit,
                          int optimize, mulib_jit_flow_t **out,
                          mulib_mir_diagnostic_t *d)
  {
    if (!out || *out) return MULIB_JIT_EINVAL;
    if (!mulib_jit_supported ()) return MULIB_JIT_ENOTSUP;
#ifdef MULIB_JIT_NATIVE_
    {
      mulib_jit_flow_t *j = (mulib_jit_flow_t *)calloc (1, sizeof (*j));
      mulib_mir_status_t ms;
      int s;
      if (!j) return MULIB_JIT_ENOMEM;
      ms = mulib_vmweave_compile (p, limit, optimize, &j->image, d);
      if (ms != MULIB_MIR_OK) { free (j); return MULIB_JIT_EMIR; }
      s = mulib_jit_flow_native_ (j);
      if (s) { (void)mulib_jit_flow_destroy (j); return s; }
      *out = j;
      return MULIB_JIT_OK;
    }
#else
    (void)p; (void)limit; (void)optimize; (void)d;
    return MULIB_JIT_ENOTSUP;
#endif
  }
  mulib_mir_status_t
  mulib_jit_flow_run (const mulib_jit_flow_t *j, mulib_mir_frame_t *f, size_t budget)
  {
    if (!j || !f || !j->native || !f->registers || f->running
        || f->program != mulib_jit_flow_program (j)) return MULIB_MIR_INVALID;
    if (f->status != MULIB_MIR_OK && f->status != MULIB_MIR_STEP_LIMIT) return f->status;
    if (f->pc >= f->program->count) return MULIB_MIR_BAD_REFERENCE;
    f->remaining = budget;
#ifdef MULIB_JIT_NATIVE_
    {
      mulib_mir_status_t (*entry)(mulib_mir_frame_t *);
      if (sizeof (entry) != sizeof (j->native->memory)) return MULIB_MIR_INVALID;
      memcpy (&entry, &j->native->memory, sizeof (entry));
      return entry (f);
    }
#else
    return MULIB_MIR_INVALID;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
