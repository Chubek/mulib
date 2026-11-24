    .text
    .p2align 2
    .globl rv64_abi_oracle
    .type rv64_abi_oracle,@function
rv64_abi_oracle:
    andi t0, sp, 15
    bnez t0, .Lbad
    li t0, -11
    bne a0, t0, .Lbad
    li t0, 0x3ff8000000000000  # binary64 1.5, passed via a1 (not fa0)
    bne a1, t0, .Lbad
    li t0, -2147483647        # psABI: u32 is sign-extended from 32 bits to XLEN
    bne a2, t0, .Lbad
    li t0, 4
    bne a3, t0, .Lbad
    li t0, 5
    bne a4, t0, .Lbad
    li t0, 6
    bne a5, t0, .Lbad
    li t0, 7
    bne a6, t0, .Lbad
    li t0, 8
    bne a7, t0, .Lbad
    ld t1, 0(sp)              # first stacked parameter
    li t0, 9
    bne t1, t0, .Lbad
    li a0, 0xc004000000000000 # binary64 -2.5, returned via a0 (not fa0)
    ret
.Lbad:
    li a0, 0
    ret
    .size rv64_abi_oracle, .-rv64_abi_oracle
    .section .note.GNU-stack,"",@progbits
