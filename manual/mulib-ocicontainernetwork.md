# ocicontainernetwork

IPv4 address/prefix/optional gateway plan. `init` uses strict shared IPv4 literal
parsing, prefix 0..32; a supplied gateway must belong to the subnet and differ
from the address. Returns 1 success, 0 invalid, unchanged output on error. Value
object owns no allocations. Does not reject every subnet-specific reserved host
address; a real network backend must enforce its address policy.

No interfaces, bridges, NAT, DNS, DHCP, namespace changes or network privileges.
Switches: `MULIB_IMPL_OCICONTAINERNETWORK`, `MULIB_IMPL_INET4`, `MULIB_IMPL_PARSEC`.
Example: `examples/modules/ocicontainernetwork.c`.
