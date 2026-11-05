/** @file mulib-walker.h
 * @brief Visitor walker sharing the parser event-listener interface.
 * Define MULIB_IMPL_WALKER, MULIB_IMPL_EVTLISTENER and MULIB_IMPL_EVTSTREAM.
 */
#ifndef MULIB_WALKER_H
#define MULIB_WALKER_H
#include "mulib-evtlistener.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Walk a borrowed AST/subtree in enter/leave order, without allocation.
 * NULL root is empty. Returns 1 complete, 0 callback stopped, -1 error.
 * Neither callbacks nor other threads may mutate the tree during traversal. */
int mulib_walker_walk(const mulib_ast_t *, const mulib_evtlistener_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WALKER) && !defined(MULIB_WALKER_IMPLEMENTED)
#define MULIB_WALKER_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_walker_walk(const mulib_ast_t *root, const mulib_evtlistener_t *listener) {
  mulib_evtstream_t stream = mulib_evtstream_from_ast(root);
  return mulib_evtlistener_listen(&stream, listener);
}
#ifdef __cplusplus
}
#endif
#endif
