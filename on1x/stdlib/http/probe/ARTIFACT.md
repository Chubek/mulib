# Explicit atomic HTML publication

`std.http.probe.artifact` is optional. Call
`basis_http_probe_save_html(slot, document)` from an application that wants to
publish the **canonical, unsanitized HTML** as a separate effect. `slot` is a
symbolic identifier, not a path. This does not change artifact retention or
save any documents implicitly.

Link `artifact.c`, the `std.html` native companion, and exactly one
implementation of MuLIB `mulib-workspace-fs.h` (`MULIB_IMPL_WORKSPACE_FS`,
POSIX C11). The trusted host must define
[`on1x_http_probe_artifact_authorize_v1`](artifact.h). There is no default or
fallback. It must map the slot to one exact destination using an already
opened, caller-owned private workspace directory. The writer checks that
the directory belongs to the effective user and has no group/other access.
The host must ensure that directory (including any nested destination) is
private against uncooperative writers, bind mounts, and hard-link aliases.
The slot grants no filesystem authority. Do not give this raw bridge to
untrusted programs or reuse it as a general arbitrary-byte write primitive.

The operation limits markup to 256 KiB and creates a private file without
replacing an existing one. MuLIB stages and synchronizes the content, then
publishes it atomically in the destination directory. `ArtifactSaved` means
publication and synchronization succeeded. `ArtifactUncertain` means it may
already be visible: inspect the authorized destination before taking further
action. `ArtifactRejected` covers malformed slots, denial, size and
serialization limits, collisions and I/O failures. This is single-file
visibility, not an atomic multi-artifact transaction or a bounded I/O deadline.
