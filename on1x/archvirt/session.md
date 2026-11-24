# Local ArchVirt sessions

`on1xc vmm connect ID` connects a MuLIB line editor to a local ArchVirt guest.
`ID` is the exact 64-character lowercase key registered for the guest ELF by
the compiler. A first connection can launch a cached guest; later connections
use the already-live guest, even after its cache entry is cleared. Connect
without a live VM or a verified cached image fails without fallback.

At the `vmm>` prompt, enter `send VALUE` to submit one signed decimal 64-bit
integer, `:help` for the command summary, or `:quit` to disconnect without
stopping the VM. Guest standard output and error are distinct streams. A new
console replays up to 32 KiB of recent output, then displays live output.
Guest control and non-ASCII bytes are escaped on a terminal; redirected
nonterminal output preserves the original bytes. The editor itself controls
cursor movement, line editing, completion, and per-connection history.

`on1xc vmm connect ID --detached [--param VALUE ...]` leaves the VM running
without a console. If a live session already exists, parameters go to that
same session. `--param` is repeatable (up to 64 per invocation) and can also
be used when attaching a console. Acknowledgment means a whole validated
batch reached the runner input pipe; it does **not** guarantee the guest
read or processed those values. The guest must implement the interactive
parameter interface. A failed or timed-out send is never retried.

`on1xc vmm stop ID` terminates a live session and clears its discovery entry;
it also clears the stale endpoint of a guest that already exited. A failed,
finished, or expired *previously live* session is not silently restarted
by repeated sends or connects: stop its ID first to allow a fresh launch.

## Private launcher contract

The compiler delegates to:

```text
python3 -IB session.py --runner RUNNER --repl REPL [--image ELF] connect ID [--detached] [--param VALUE ...]
python3 -IB session.py --runner RUNNER --repl REPL stop ID
```

`RUNNER` and `REPL` are trusted absolute installed paths; the helper is
`on1x-vmm-repl`, built from `session_host.cpp` and `session_console.c` with the
MuLIB root include directory. `ELF` is an optional verified, private staged
artifact supplied by the compiler. On a new start the helper pins the
image before acknowledging startup and invokes exactly `RUNNER --interactive
/proc/self/fd/N`. The compiler may unlink a private stage after the helper
returns; the runner retains the opened descriptor. An inherited guest image
descriptor is also supported. A live attachment doesn't open the staged image,
and no shell or direct host execution of the guest is involved.

## Local transport and limits

The supervisor discovers one live process per UID and ID in
`/tmp/on1x-archvirt-UID`, which must be an owned mode-0700 directory under a
root-owned sticky `/tmp`. Endpoint inodes must be owned mode-0700 Unix
sockets. Discovery and publication use descriptor-relative names and a namespace
lock. Clients open socket inodes without following symlinks and connect
through `/proc/self/fd`; each side checks the peer UID. No network socket or
shell is used. This is local ownership, not isolation from another process
already running with the same UID or evidence of a guest sandbox.

The Linux `SOCK_SEQPACKET` protocol bounds each record to 4 KiB, requests
to 64 ASCII integers, open consoles to 16 per session, and live sessions
to eight per UID. Discovery permits 64 live/stale entries; `stop` removes
stale records. The supervisor drops slow output clients rather than blocking
the guest, stops after 1 MiB of total runner output, and applies five-minute
input-idle and 30-minute absolute limits. The runner may apply stricter
limits. Commands have a bounded acknowledgment wait. The supervisor reaps
its runner on normal stop, quota failure, and expiry, and a parent-death
signal kills the runner if the supervisor crashes. Linux procfs and
`close_range` support are required.

## Host-protocol test

Use the native helper binary with:

```sh
python3 -IB on1x/archvirt/session_test.py \
  --repl /absolute/path/to/on1x-vmm-repl --work-dir /absolute/scratch
```

The test uses an explicit subprocess fixture, not an ArchVirt substitute
in the installed tools. It covers detached startup, concurrent attachment,
private staged image unlink, terminal editing and control-byte escaping,
i64 bounds, oversized packets, stale handling, output limits, and stop.
Actual guest execution is a separate integration test.
