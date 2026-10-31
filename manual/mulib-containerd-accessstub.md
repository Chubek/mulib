# mulib-containerd-accessstub

## NAME AND INTEGRATION

Explicit authenticated-principal authorization for lifecycle commands. Define
`MULIB_IMPL_CONTAINERD_ACCESSSTUB` and the `conainerd-ipc` closure. Example:
`examples/modules/containerd-accessstub.c`.

## PRINCIPALS AND RULES

Principal contains subject, UID, GID and authenticated flag. The application or
transport establishes these values; command payloads never grant authentication.
This module does not exchange passwords/tokens or authenticate peers itself.

`init(policy,capacity,default_allow)` creates an empty bounded ordered rule list.
Zero capacity is valid for fallback-only policy. `add` copies a rule with subject,
optional exact container ID, operation bitset, any_subject flag and allow/deny.
Bit N-1 represents opcode N, and `MULIB_CONTAINERD_ACCESS_ALL` covers all seven
known commands. Unknown bits and empty operation masks are rejected.

Empty container_id matches any valid ID. any_subject matches any authenticated
subject; it never bypasses authentication. UID/GID are available to the embedding
application but this policy matches the subject field. First matching rule wins.
`remove(index)` compacts the list while preserving order.

## CHECKING

`check(policy,principal,command,&matched)` validates identity/opcode, rejects
unauthenticated principals, then scans ordered rules or applies the configured
fallback. Success is OK; denial EACCESS. matched is SIZE_MAX for fallback,
unauthenticated or invalid input. allowed/denied counters saturate at UINT64_MAX.

Checks do not execute commands, alter container generations or acquire object
leases. The control bridge invokes authorization before dispatch and correlates
the resulting reply. Already-executed request replays use the connection's cached
reply; policy changes do not reexecute an old command.

## OWNERSHIP AND LIMITS

Rules are owned values; no principal/command pointer is retained by a check.
`free` releases rule storage. Applications externally serialize policy updates and
checks. There is no implicit administrator/root bypass, wildcard pattern grammar,
network authentication service or durable audit output. CT errors cover malformed
rules, capacity, allocation and denied access.
