/** @file mulib-shm.h
 * @brief System V shared-memory creation, attachment and explicit removal.
 * Define MULIB_IMPL_SHM once. No automatic synchronization is provided.
 */
#ifndef MULIB_SHM_H
#define MULIB_SHM_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_shm
  {
    void *data;
    size_t size;
    int id, readonly;
  } mulib_shm_t;
#define MULIB_SHM_INIT { NULL, 0, -1, 0 }
  /** @brief Create a private, mode-0600 segment and attach read/write. Returns
   * 0 success, -1 invalid, -2 OS failure, -3 unsupported. Initialized output
   * is unchanged on failure. Share id explicitly with cooperating processes.
   */
  int mulib_shm_create (mulib_shm_t *, size_t);
  /** @brief Attach an existing id and discover its size; readonly is 0 or 1.
   */
  int mulib_shm_attach (mulib_shm_t *, int id, int readonly);
  /** @brief Mark id for deletion after the last detach; mappings stay valid.
   */
  int mulib_shm_remove (int id);
  /** @brief Detach/reset without deleting the segment; idempotent. */
  int mulib_shm_close (mulib_shm_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SHM) && !defined(MULIB_SHM_IMPLEMENTED)
#define MULIB_SHM_IMPLEMENTED
#include <errno.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
int
mulib_shm_attach (mulib_shm_t *s, int id, int ro)
{
  if (!s || s->id != -1 || s->data || s->size || id < 0
      || (ro != 0 && ro != 1))
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    struct shmid_ds st;
    void *p;
    if (shmctl (id, IPC_STAT, &st) < 0)
      return -2;
    p = shmat (id, NULL, ro ? SHM_RDONLY : 0);
    if (p == (void *)-1)
      return -2;
    s->data = p;
    s->size = st.shm_segsz;
    s->id = id;
    s->readonly = ro;
    return 0;
  }
#else
  return -3;
#endif
}
int
mulib_shm_create (mulib_shm_t *s, size_t n)
{
  if (!s || s->id != -1 || s->data || s->size || !n)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  {
    int id = shmget (IPC_PRIVATE, n, IPC_CREAT | 0600), r;
    if (id < 0)
      return -2;
    r = mulib_shm_attach (s, id, 0);
    if (r)
      {
        int saved = errno;
        shmctl (id, IPC_RMID, NULL);
        errno = saved;
      }
    return r;
  }
#else
  return -3;
#endif
}
int
mulib_shm_remove (int id)
{
  if (id < 0)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  return shmctl (id, IPC_RMID, NULL) == 0 ? 0 : -2;
#else
  return -3;
#endif
}
int
mulib_shm_close (mulib_shm_t *s)
{
  if (!s)
    return -1;
  if (s->id == -1)
    return 0;
#if defined(__unix__) || defined(__APPLE__)
  if (shmdt (s->data) < 0)
    return -2;
  s->data = NULL;
  s->size = 0;
  s->id = -1;
  s->readonly = 0;
  return 0;
#else
  return -3;
#endif
}
#endif
