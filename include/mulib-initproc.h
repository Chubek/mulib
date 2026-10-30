/** @file mulib-initproc.h
 * @brief POSIX child-reaping primitive for caller-owned init/supervisor loops.
 * Define MULIB_IMPL_INITPROC once. Does not become PID 1, fork, or install
 * handlers. */
#ifndef MULIB_INITPROC_H
#define MULIB_INITPROC_H
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Wait for child pid>0 or any child (-1). nonblocking is 0/1.
   * 1 reaped (outputs pid and shell-style exit status), 0 not ready,
   * 2 no matching children, -1 invalid, -2 OS error, -3 unsupported.
   * EINTR retried. Signal death maps to 128+signal. Outputs unchanged unless
   * reaped. */
  int mulib_initproc_reap (int child, int nonblocking, int *pid,
                           int *exit_status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INITPROC) && !defined(MULIB_INITPROC_IMPLEMENTED)
#define MULIB_INITPROC_IMPLEMENTED
#include <errno.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/wait.h>
#endif
int
mulib_initproc_reap (int child, int nonblock, int *pid, int *code)
{
  if (!pid || !code || (child != -1 && child <= 0)
      || (nonblock != 0 && nonblock != 1))
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    pid_t result;
    int status;
    do
      {
        result = waitpid ((pid_t)child, &status, nonblock ? WNOHANG : 0);
      }
    while (result < 0 && errno == EINTR);
    if (result < 0)
      return errno == ECHILD ? 2 : -2;
    if (!result)
      return 0;
    if (WIFEXITED (status))
      *code = WEXITSTATUS (status);
    else if (WIFSIGNALED (status))
      *code = 128 + WTERMSIG (status);
    else
      return -2;
    *pid = (int)result;
    return 1;
  }
#else
  return -3;
#endif
}
#endif
