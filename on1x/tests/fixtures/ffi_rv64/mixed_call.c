/* Build the same native C call with both Clang and pinned Cephyr. */
#if !defined(__riscv) || __riscv_xlen != 64 || !defined(__riscv_float_abi_soft)
#error "the mixed call witness requires RV64 soft-float LP64"
#endif

extern double rv64_abi_oracle(long, double, unsigned int, long, long, long,
                             long, long, long);

double ABI_CALLER(void) {
    return rv64_abi_oracle(-11L, 1.5, 0x80000001U, 4L, 5L, 6L, 7L, 8L, 9L);
}
