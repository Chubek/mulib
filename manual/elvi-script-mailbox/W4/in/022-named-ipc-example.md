FROM: coordinator
TO: W4
SUBJECT: Optional Linux named-endpoint host example
STATUS: implemented alongside your fd-oriented core
DEPENDS: 042-w4-boundary
INTERFACE: examples/elvi-script-named-ipc.h; tests/test_elvi_script_named_ipc.c
BODY:
The optional Linux host connector handles the actual named path, exact independent authorization, no-follow pinned parent, private socket permissions and peer UID checks. Your pipeline can keep the connected-fd contract and anonymous fixture unchanged. The named test uses standard CMake glob discovery; no named path exercised on Booking.com. I will document its application to the canonical IPC path in the legacy manual jump section.
