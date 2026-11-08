# mulib-evtlistener

[Manuals](README.md) · [Library index](index.md)

The listener side of [`mulib-evtstream`](mulib-evtstream.md). Define
`MULIB_IMPL_EVTLISTENER` and `MULIB_IMPL_EVTSTREAM` in one translation unit and
include `mulib-evtlistener.h`. No AST implementation or allocation is needed
unless the application uses AST constructors. See
`examples/modules/evtlistener.c` and `tests/test_evtlistener.c`.

## Listener structure

```c
typedef struct mulib_evtlistener {
    mulib_evtlistener_event_fn event;
    void *user;
} mulib_evtlistener_t;
```

Initialize with `{ callback, user }`. The callback takes
`(const mulib_event_t *event, void *user)` and returns positive to continue,
zero to cancel, or negative on error. A NULL user pointer is valid; a NULL
callback is not. The listener is a copyable, non-owning value: there is no
allocation, retained node, event queue or destroy function. Copies share the
same user state, not an independent copy of it.

## Consuming and receiving events

`mulib_evtlistener_listen(&stream, &listener)` synchronously drains an existing
pull stream. It returns 1 on exhaustion, 0 on cancellation, or -1 for invalid
arguments or a callback error. Empty streams succeed without invoking the
callback; a valid listener is still required. Exhaustion is sticky, as for
`mulib_evtstream_next`. Each event has already been consumed when delivered:
after a stop/error, calling `listen` again resumes with the next event. No
synthetic leave events or rollback occur on cancellation.

`mulib_evtlistener_push(&listener, &event)` delivers one parser-produced event
immediately without a tree or pull cursor. Return values normalize to 1
(positive callback result), 0, or -1 (negative callback result). ENTER/LEAVE
kind and a non-NULL event node are required. The parser may supply a temporary
stack-local `mulib_ast_t` with production, text, length and offset; tree links
may be NULL. Binary text is represented by pointer/length, not a C string.

The callback receives borrowed event/node/text data valid only during the
call; copy any data that must survive it. Application-owned callback state
must remain alive during delivery. The callback must not mutate the pull
cursor or its tree during `listen`. Independent listeners/cursors may share
immutable data; shared mutable callback state requires synchronization.

The producer is responsible for balancing events, depth and avoiding
speculative parsing side effects. This module does not validate AST graphs or
event nesting, interpret productions, or perform asynchronous notification.
There are no additional size/depth limits or recursive traversal introduced
by the listener.

[`mulib-walker`](mulib-walker.md) consumes this same listener structure for AST
visits. [`mulib-vizigen`](mulib-vizigen.md) returns a `mulib_evtlistener_t` from
`mulib_vizigen_listener`, allowing the same bytecode to receive parser-time
events or walk an existing tree. The older `mulib-listener.h` remains a separate
production-specific enter/leave callback-table API.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-evtlistener.h`](../include/mulib-evtlistener.h) |
| Implementation switch | `MULIB_IMPL_EVTLISTENER` |
| Repository-local includes | [mulib-evtstream](mulib-evtstream.md) |
| Complete example | [`examples/modules/evtlistener.c`](../examples/modules/evtlistener.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/evtlistener.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
