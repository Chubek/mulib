# Contract profile example

From the MuLIB root, compile the standalone C-side ownership example:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Ikernels \
  on1x/examples/contracts/contract_usage.c -lm -o /tmp/opencode/contract-usage
/tmp/opencode/contract-usage
# [42]
python3 on1x/tools/on1x-contracts.py lint
python3 on1x/tools/on1x-contracts.py build-plan json vec parsec
```

The implementation switches are in one C translation unit. The example shows
conditional JSON transfer, child-borrow expiry, separately owned serialized
text, byte-vector cleanup and borrowed Parsec descriptors with owned captures.
It is deliberately **not** a claimed safe On1x facade.

To discover the real declarations and check all profiles against ExoML, follow
the [snapshot example](../../docs/contracts.md#real-extraction-and-a-reproducible-snapshot).
Use a fresh evidence directory; existing output is never replaced. The shipped
annotations and every resulting report still say `safe_export: false`.
