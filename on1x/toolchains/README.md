# On1x SDK toolchain policy

The SDK does not treat a provider name, `driver_id`, command, environment
variable, or `PATH` entry as executable authority.

An SDK 2 toolchain provider copies a validated target profile, logical driver
identity, version, roles, priority, and SHA-256 identity into the shared
registry. Separately, a host-owned `On1xSDK_ToolchainResolver` may authorize an
exact canonical absolute path for that driver/role/digest tuple. Resolution:

1. matches the complete validated target and one requested tool role;
2. deterministically chooses the unique highest-priority provider;
3. requires an exact host binding and digest match;
4. returns an owned inert resolution record.

It never searches, opens, shells out to, or executes the path. Equal-ranked
providers fail as ambiguous. Relative paths, `.`/`..`, empty components, shell
metacharacters, arbitrary command strings, and mismatched digests reject.
Changing installation roots requires a new host binding; provider metadata
remains path-independent.

No host C compiler is labeled a native On1x backend. Current executable backend
artifacts are the verified HIR/MIR adapters documented in
[`../backends/README.md`](../backends/README.md).
