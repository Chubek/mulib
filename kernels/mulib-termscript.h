/** @file mulib-termscript.h
 * @brief Parse, run, and compile a stack language for terminal applications.
 * Define MULIB_IMPL_TERMSCRIPT for the source execution convenience helper,
 * and enable individual dependency implementations explicitly.
 */
#ifndef MULIB_TERMSCRIPT_H
#define MULIB_TERMSCRIPT_H
#include "mulib-termscript-capasm.h"
#include "mulib-termscript-host.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Convenience helper for a fresh VM. The caller retains `program`; it must
 * be initialized, empty and kept alive until after VM clear, also on failure
 * if execution began. Use parse and run separately for other ownership needs. */
int mulib_termscript_run_source(mulib_termscript_vm_t *, const char *, size_t,
                                mulib_termscript_program_t *program, mulib_termscript_error_t *);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSCRIPT) && !defined(MULIB_TERMSCRIPT_IMPLEMENTED)
#define MULIB_TERMSCRIPT_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif

int
mulib_termscript_run_source(mulib_termscript_vm_t *vm, const char *source, size_t n,
                            mulib_termscript_program_t *program, mulib_termscript_error_t *err)
{
  if (!vm || !program || program->source || program->nodes) {
    if (err) {
      err->offset = 0;
      err->message = "expected a VM and empty program storage";
    }
    return 0;
  }
  return mulib_termscript_parse(source, n, program, err) && mulib_termscript_run(vm, program, err);
}

#ifdef __cplusplus
}
#endif
#endif
