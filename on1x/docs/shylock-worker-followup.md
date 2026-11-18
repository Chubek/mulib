# Follow-up task: Shylock worker and data-plane integration

This task belongs in the Shylock integration, not in the general-purpose MuLIB
sandbox kernels. No Shylock code changes are part of the kernel integration.

- Define separate worker roles for independent work, with a trusted data-plane
  service and a supervisor that owns plugin routing, manifests, and permissions.
  Keep credentials, raw sockets, and database handles in trusted services.
- Pin the trusted worker bootstrap and its dependencies. Complete and verify
  required isolation before loading untrusted plugins. Do not start work if a
  required control cannot be enforced.
- Give workers only the typed broker capabilities they need. Define each
  request's scope, limits, ownership, deadline, and result, including uncertain
  commits. Validate and authorize every request on the host.
- Keep callbacks, native handles, and resource cleanup in trusted glue until
  On1x can verify those obligations. Exotract discovery alone is not admission.

Acceptance: run real worker processes and cover startup refusal, worker death,
missed deadlines, malformed frames, revoked capabilities, and rollback of
staged effects. Record actual kernel enforcement and run a targeted security
review before permitting hostile third-party plugins in production.
