# Module work-lane coordination

`tools/on1x-module-coord.c` is a Linux development utility, not part of the
compiler or installed runtime. It uses one private POSIX message queue per
role and a versioned, process-shared snapshot. Build and exercise it without
configuring the compiler:

```sh
mkdir -p build/module-coord
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -pthread \
  on1x/tools/on1x-module-coord.c -lrt -o build/module-coord/coord
python3 on1x/tests/test_module_coord.py build/module-coord/coord \
  --work-dir build/module-coord
```

The test creates fresh session IDs. It covers concurrent status publication,
FIFO delivery to a role, saturation and recovery, audit correlation, partial
initialization rollback, and cleanup/reinitialization. POSIX queues must be
available and the host must allow at least ten messages per queue.

## Running a session

Agree on an unused lowercase session ID and a dedicated audit path before
initializing. All workers use the same session and audit path. Do not replace
the executable or reuse the ID during a session.

```sh
export ON1X_MOD_AUDIT="$PWD/build/module-coord/audit.log"
C="$PWD/build/module-coord/coord"
S=module_review_01
"$C" "$S" init
"$C" "$S" contract 'r1: preserve the legacy wire; changes require acknowledgement'
"$C" "$S" state integration started
message_id=$("$C" "$S" send integration grammar proposal 'Confirm AST field names')
"$C" "$S" recv grammar
"$C" "$S" send grammar integration ack 'Confirmed' "$message_id"
"$C" "$S" recv integration
"$C" "$S" snapshot
```

- Roles are `grammar`, `modules`, `imports`, `visibility`, `signatures`,
  `asdl`, `ppx`, `stdlib`, and `integration`. Only the assigned worker reads
  its queue. Send to each intended role individually; publishing a snapshot
  does not send a notification.
- Use `proposal`, `ack`, `request`, `candidate`, and `report` messages as
  appropriate. Use the proposal's ID as the optional reply correlation.
  Publish concise status or the current contract separately. A release
  candidate report should include the commit ID, interface revision, tests
  actually run, and known limitations.
- Payloads are single-line, at most 639 bytes; kinds are tokens at most
  23 bytes. `recv` waits up to its optional timeout for the first message
  (default 50 ms), then drains currently queued messages. `send` returns 3
  when the destination stays full for three 250 ms attempts. Other roles
  and the shared snapshot remain usable while it waits. Retry explicitly
  once the receiver has drained the queue.
- Correlate audit events by ID, not line order: `recv` can log before `send`
  logs success. There is no transaction spanning the queue, snapshot, and
  audit file, and no delivery guarantee after a crash between these
  operations. The snapshot contains current values, not history.
- This is cooperative local IPC. Names are not credentials; workers running
  under the same user can act as any role. Keep the audit path in a
  worker-owned directory. Do not use this tool to pass secrets.

Only the coordinating owner should run `"$C" "$S" cleanup`, after all reports
and explicit agreement, with no workers using the queues. Cleanup is
idempotent and leaves the on-disk audit log intact. The kernel can retain
unnamed queues for processes that still hold descriptors; never run cleanup
concurrently with initialization or active workers.
