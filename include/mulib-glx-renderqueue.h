/* MuLIB graphical extension: glx-renderqueue. C11, header-only. */
#ifndef MULIB_GLX_RENDERQUEUE_H
#define MULIB_GLX_RENDERQUEUE_H
#include "mulib-glx-drawlist.h"
/* Queue borrows lists, their command arrays, and image resources until execute
 * completes. Do not reset/release/mutate them while queued. Submit a frozen
 * snapshot.draws when resources cannot be kept alive by the application.
 */
typedef struct {
    const mulib_glx_drawlist_t **v;
    size_t n, cap;
} mulib_glx_renderqueue_t;

static inline int mulib_glx_renderqueue_submit(mulib_glx_renderqueue_t *q,
                                              const mulib_glx_drawlist_t *d)
{
    if (!q || q->n > q->cap || q->n == SIZE_MAX ||
        !mulib_glx_drawlist_valid(d) ||
        !mgx_grow((void **)&q->v, &q->cap, q->n + 1, sizeof d))
        return 0;
    q->v[q->n++] = d;
    return 1;
}

/* Submission order and each list's painter order are both preserved.
 * Validation failure leaves the queue and target unchanged. Once painting
 * starts, the queue is consumed even on allocation failure, avoiding accidental
 * replay of already painted alpha commands. Such failure may leave a prefix.
 */
static inline int mulib_glx_renderqueue_execute_checked(
    mulib_glx_renderqueue_t *q, mgx_surface *s)
{
    int ok = 1;
    if (!q || q->n > q->cap || q->cap > SIZE_MAX / sizeof *q->v ||
        (q->cap && !q->v) || !mgx_surface_valid(s))
        return 0;
    for (size_t i = 0; i < q->n; i++)
        if (!mulib_glx_drawlist_valid(q->v[i]))
            return 0;
    for (size_t i = 0; i < q->n && ok; i++)
        ok = mulib_glx_drawlist_render_checked(q->v[i], s);
    q->n = 0;
    return ok;
}

static inline void mulib_glx_renderqueue_execute(mulib_glx_renderqueue_t *q,
                                                mgx_surface *s)
{
    (void)mulib_glx_renderqueue_execute_checked(q, s);
}

static inline void mulib_glx_renderqueue_free(mulib_glx_renderqueue_t *q)
{
    if (q) {
        free(q->v);
        memset(q, 0, sizeof *q);
    }
}
#endif
