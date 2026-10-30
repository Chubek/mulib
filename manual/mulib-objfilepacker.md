# objfilepacker

`add` appends an opaque member to an initialized Unix ar byte buffer, writing
magic on first append. Names are 1..15 ASCII alnum/dot/underscore/hyphen bytes,
excluding dot/dot-dot. Metadata is deterministic (zero IDs/time, mode 100644).
Members are newline-padded to even size; size is bounded by the 10-digit ar field.

Returns 1 success, 0 invalid/overflow/OOM, failure-atomic and alias-safe. Existing
buffers are checked for magic/even length, not revalidated member-by-member.
No symbol index, GNU/BSD long names, thin archive, compression or file I/O.
Switches: `MULIB_IMPL_OBJFILEPACKER`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/objfilepacker.c`.
