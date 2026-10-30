# binfile

Borrowed binary span, no file opening/reading or ownership transfer. `open`
accepts NULL only for empty bytes. `slice` checks subtraction-based bounds and
can return a one-past-end zero-length span; output unchanged on error. Both
return 1 success, 0 invalid. Source must outlive every slice.

`format` recognizes ELF/ar magic only, returning UNKNOWN otherwise. It does not
certify structural validity. Use ELF/ar readers for actual validation.
Switch: `MULIB_IMPL_BINFILE`. Example: `examples/modules/binfile.c`.
