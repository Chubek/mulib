# ElviScript IPC v1

The application owns and explicitly authorizes a connected Unix-domain
stream descriptor and uses it for one run, in one direction. Neither
the framing layer nor ElviScript authenticates arbitrary peers; the host
must arrange a trusted connection (for example, an inherited socketpair).
The library never closes the descriptor. Its owner must close it at the
end of the run or on error, without reusing it or replaying results.

Every event uses [ipc-framed](mulib-ipc-framed.md): a four-byte big-endian
payload length followed by the payload below. Each payload starts with a
version (`1`) and a one-byte kind:

| Kind | Value | Remaining payload |
| --- | ---: | --- |
| `RESULT` | 1 | Name, URL, price, location. Each is a two-byte big-endian length followed immediately by that many UTF-8 bytes. |
| `DIAGNOSTIC` | 2 | One numeric code (configuration 1, resource 2, fetch 3, response 4, logic 5, policy 6). |
| `COMPLETE` | 3 | One byte with the exact number of preceding results (0–64). |
| `FAILURE` | 4 | One byte matching the preceding diagnostic code. |

Field byte limits are 511, 2047, 127 and 255, respectively. Name and URL
are nonempty. Fields disallow C0 controls, DEL and embedded NUL.
The maximum payload is 2950 bytes. The decoder rejects unknown versions or
kinds, malformed lengths, invalid UTF-8, inconsistent counts, extra bytes
in a frame, and other invalid sequences. Never use result URLs as commands
or to initiate new requests from the receiver.

Results are tentative until `COMPLETE`. `FAILURE` and missing completion
(including timeout, malformed frame, truncation or EOF) discard tentative
output. Diagnostics are fixed codes, not script or response text. The
receiver stops at the first terminal event; the one-run descriptor is not
a multiplexed stream.

Each peer uses its own absolute monotonic deadline (1–30000 ms from
initialization). A blocked write, read, or partial disconnect can poison
the stream; the caller closes it. This timeout does not preempt external
callbacks. The pipeline requires its TLS backend to enforce independent
network I/O bounds.

The [receiver example](../examples/elvi-script-receiver.c) accepts an
inherited descriptor number (at least 3). After completion it prints TSV
with backslash and non-ASCII bytes escaped as `\xHH`. It opens no socket of
its own and displays nothing from incomplete result streams.
