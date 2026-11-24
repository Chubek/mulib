# Shared GUI helpers and Lua RetGTK

These helpers are local, header-only C11 state and a host-driven Lua adapter.
They do **not** implement D-Bus services, system clipboard ownership, XDG portal
transport, a native RetGTK binding, or a complete desktop application launcher.
The CPU renderer and native toolkits remain shared; no renderer is introduced
here.

## C ownership and bounded input

Initialize owning structs to zero, do not copy them, and release them explicitly.
Access must remain on one thread or be externally synchronized.

* `mulib-gui-resource.h`: successful `add` transfers one reference; failed adds
  leave ownership with the caller. `acquire` returns a reference that must be
  released, and rejects reference-count overflow. Final release unlinks the
  resource before invoking its destructor. `free` invalidates **all** acquired
  pointers, detaches the registry, and releases each resource once. Reentrant
  registry operations are rejected during `free`; recursive `free` is harmless.
  The registry object itself must outlive every callback. The added `closing`
  member is lifecycle state, not an application-settable property.
* `mulib-gui-clipboard.h`: copies MIME and binary bytes, including when arguments
  borrow the clipboard being replaced. Failed validation/allocation leaves data
  and serial unchanged. Serial overflow is rejected. `get` returns borrowed
  bytes, invalidated by successful replacement/free; on failure its length output
  is zero. An empty, successfully set payload still has a non-null pointer.
  Drag/drop uses this same **in-process-only** storage.
* `mulib-gui-mime.h`: validates bounded ASCII concrete `type/subtype` strings with
  optional token-valued parameters, such as `text/plain; charset=utf-8`.
  Wildcards, control characters, and quoted parameter values are rejected.
  Comparison remains exact/case-sensitive; there is no MIME normalization.
  Guessing is a small signature/extension heuristic, **not** a security check or
  shared-mime-info database. Extensions are case-insensitive and cannot come from
  an earlier directory component.
* `mulib-gui-settings.h`: stores at most 63 key bytes and 255 value bytes. The
  format is plain `key=value`, with LF, CRLF or a final unterminated line.
  There is no comment, section, interpolation, escaping, or execution syntax.
  Reading transactionally merges into existing settings; duplicate keys use the
  last value. Invalid/oversized/NUL-containing input, I/O error, or allocation
  failure leaves the store unchanged. Borrowed strings may be passed back to
  `set`, but otherwise expire on mutation/free.
* `mulib-gui-desktop.h`: parses a bounded metadata subset transactionally, without
  truncation. Duplicate known keys, invalid booleans, invalid string escapes,
  repeated Desktop Entry sections, NULs and oversized input are rejected.
  `Name`, `Icon` and `Type` decode `\s`, `\n`, `\t`, `\r` and `\\`.
  Localized/unknown keys are ignored; there is no locale matching.
  **`Exec` is untrusted verbatim data, never a shell command.** This API does not
  tokenize, expand field codes, resolve executables, or launch anything. A host
  must implement the desktop specification and construct a validated argument
  vector before launching; it must never pass this field to a shell.

Limits can be defined before including the headers:

| Macro | Default |
| --- | ---: |
| `MULIB_GUI_RESOURCE_LIMIT` | 4096 resources |
| `MULIB_GUI_CLIPBOARD_LIMIT` | 16 MiB |
| `MULIB_GUI_SETTINGS_LIMIT` | 4096 keys |
| `MULIB_GUI_SETTINGS_INPUT_LIMIT` | 1 MiB per read |
| `MULIB_GUI_DESKTOP_INPUT_LIMIT` | 64 KiB per parse |

Stream parsing does not rewind on failure. Output-stream writes are not
transactional. The caller provides/open/closes streams; these parsers neither
discover files nor grant sandbox permissions. Other desktop/portal headers are
still their documented local bookkeeping implementations.

## Lua RetGTK mutation contract

Use `append`, `remove`, `set_child`, `set`, and `destroy` rather than directly
editing structural fields. Constructors and `set` reject reserved structure and
method names. Cycle/depth checks occur before attachment; `set_child(nil)` clears
a container. A removed widget survives; `destroy()` detaches it, recursively
destroys still-owned children, disconnects its listeners, and is idempotent.
Destroy callbacks may detach children to retain them. All remaining destroy
listeners and child cleanup run even if one callback throws, after which the
first error propagates.

`on(name, fn)` still returns `fn`. `off(name, fn)` disconnects every registration
of that function for that signal. Emission snapshots registrations:

* a disconnected listener is skipped even during the current emission;
* newly added listeners wait for the next emission (including a nested one);
* destroying the emitting widget stops its remaining non-destroy listeners;
* ordinary listener errors propagate after restoring the recursion guard.

Bindings return an idempotent disconnect function, disconnect on endpoint
destruction, and restore their recursion guard after errors.

Each paint snapshots the **whole tree** before callbacks. New children appear
next frame; removed/reparented/destroyed nodes are skipped without invalidating
iteration. A removed ancestor or replaced application root invalidates the
remaining old subtree immediately. Hidden ancestors suppress drawing and input;
disabled ancestors suppress input, including list rows, notebook tabs and scroll
wheel handling. Interaction rectangles are intersected with ancestor bounds even
without host clip hooks. Actual drawing clipping requires both `imm_clip_push`
and `imm_clip_pop`. Paint errors unwind clip scopes and application frame guards.

Defaults in `retgtk.core` bound tree depth to 128, snapshot nodes to 8192,
listeners per widget to 1024, and signal recursion to 64. IDs stay in RetGTK's
reserved exact-integer range below ImmGTK extension IDs. List ID caches retain
only the current viewport. Table painting accepts at most 1024 columns and emits
at most 8192 cells per frame; invalid row heights/counts fail explicitly.
These are safety limits, not million-row model/view parity or full editor/IME
support.

## Explicit host scheduling

Existing `app:frame(input)` and the legacy global `retgtk_run(app)` hook remain.
`frame` returns whether it painted; recursive frames/loops are rejected.
The legacy hook owns its own scheduling and limits.

An optional host adapter supplies a deterministic, bounded loop:

```lua
local app = require('retgtk').application({
    host = {
        next_frame = function(host, app)
            -- Wait for a real event/tick here, then return its input table.
            -- Return nil to close; nil does not mean "poll again".
            return nil
        end,
        present = function(host, app)
            -- Called once after each successfully painted frame.
        end,
    },
    max_frames = 10000,
})
-- app:set_window(window)
local ticks, reason = app:run() -- reason: "closed", "quit", or "limit"
```

Every non-nil host input consumes one tick, even if no window is painted.
`app:quit()` prevents further polling; the loop cap is 1..1,000,000 ticks.
Host callbacks must themselves return in bounded time; the adapter cannot
preempt them. It never opens devices/files, spawns subprocesses, or synthesizes
an OS event loop. Host hooks still need an actual platform/native integration.

## Regression tests

From the repository root:

```sh
cc -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -pedantic \
  -Ikernels tests/test-gfx-gui.c -lm -o /tmp/opencode/test-gfx-gui
/tmp/opencode/test-gfx-gui
cc -std=c11 -g -O1 -Wall -Wextra -Werror -Wno-misleading-indentation \
  -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Ikernels tests/test-gfx-gui.c -lm -o /tmp/opencode/test-gfx-gui-san
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 /tmp/opencode/test-gfx-gui-san
lua5.4 tests/test-gfx-retgtk-lua.lua
luajit tests/test-gfx-retgtk-lua.lua
```

C tests include allocator failure, aliasing, reference overflow, destructor
reentry, byte/entry limits, transactional stream rejection, MIME wildcards, and
untrusted Exec preservation. Lua tests use deterministic mock primitives for
signal/tree mutation, lifecycle errors, clipping, disabled/hidden input, viewport
bounds, binding recovery, ID limits, builder cycles, frame errors and host
EOF/quit/cap behavior. These tests do not certify Mune compatibility or live
Wayland/X11/D-Bus/portal behavior.
