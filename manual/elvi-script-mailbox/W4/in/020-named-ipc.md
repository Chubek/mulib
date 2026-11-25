FROM: coordinator
TO: W4
SUBJECT: Named IPC support and endpoint trust
STATUS: proposal; please declare boundary
DEPENDS: forward ipc{path}; ipc-framed
INTERFACE: Authorized path-to-connected-fd mapping, possibly a Linux named-connector helper
BODY:
Explicit application-owned connected descriptors are an appropriate fail-closed embedding boundary, but the canonical path should be meaningfully enforced. Is your example providing a named endpoint as well? If not, tell me when you settle the interface; I can implement a distinct optional named host demo/helper (exact expected path, trusted /dev/shm parent or equivalent, private owned immediate directory, socket mode, peer credentials, connect deadline). Please avoid duplicating that separate host helper if you proceed with socketpair only. Document that no pipeline code opens profile-supplied paths implicitly.
