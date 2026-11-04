#!/usr/bin/env python3
"""Compare ArchML's C encoder with independent LLVM MC assembler encodings."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent


def fixtures():
    cases = []

    def add(arch, form, values, asm):
        cases.append((arch, form, values, asm))

    for width, dst, src in [(32, 'r9d', 'r10d'), (64, 'r9', 'r10')]:
        for name in ['add', 'sub', 'and', 'or', 'xor', 'cmp', 'test', 'mov', 'imul']:
            add('amd64', f'{name}{width}-rr', [9, 10], f'{name} {dst}, {src}')
        for name in ['add', 'sub', 'and', 'or', 'xor', 'cmp']:
            add('amd64', f'{name}{width}-ri', [9, 1024], f'{name} {dst}, 1024')
        for name in ['shl', 'shr', 'sar']:
            add('amd64', f'{name}{width}-ri', [9, 7], f'{name} {dst}, 7')
        add('amd64', f'load{width}-base-disp32', [9, 12, 1024], f'mov {dst}, [r12+1024]')
        add('amd64', f'store{width}-base-disp32', [9, 12, 1024], f'mov [r12+1024], {dst}')
        add('amd64', f'lea{width}-base-disp32', [9, 12, 1024], f'lea {dst}, [r12+1024]')
    add('amd64', 'mov64-ri', [8, 0x1122334455667788], 'movabs r8, 0x1122334455667788')
    for name in ['push64', 'pop64']:
        add('amd64', name, [9], name[:-2]+' r9')
    for name in ['idiv64', 'div64']:
        add('amd64', name, [9], name[:-2]+' r9')
    for name in ['jmp', 'call']:
        add('amd64', name+'-reg', [9], name+' r9')
    for bits, reg in [(8, 'r9b'), (16, 'r9w')]:
        for sign in [False, True]:
            add('amd64', f'load{bits}-'+('signed' if sign else 'unsigned'), [9, 12, 1024],
                ('movsx' if sign else 'movzx')+f' r9, {"byte" if bits == 8 else "word"} ptr [r12+1024]')
        add('amd64', f'store{bits}-base-disp32', [9, 12, 1024], f'mov [r12+1024], {reg}')
    for name in ['addsd', 'subsd', 'mulsd', 'divsd', 'addps']:
        add('amd64', name, [9, 10], f'{name} xmm9, xmm10')
    add('amd64', 'movsd-rr', [9, 10], 'movsd xmm9, xmm10')
    add('amd64', 'movsd-load', [9, 12, 1024], 'movsd xmm9, [r12+1024]')
    add('amd64', 'movsd-store', [9, 12, 1024], 'movsd [r12+1024], xmm9')
    add('amd64', 'movq-to-xmm', [9, 10], 'movq xmm9, r10')
    add('amd64', 'movq-from-xmm', [9, 10], 'movq r10, xmm9')
    for name in ['ret', 'nop', 'int3', 'ud2', 'mfence', 'lfence', 'sfence']:
        add('amd64', name, [], name)
    for width, prefix in [(32, 'w'), (64, 'x')]:
        for name in ['add', 'sub', 'adds', 'subs', 'and', 'orr', 'eor', 'ands', 'mul', 'udiv', 'sdiv', 'lslv', 'lsrv', 'asrv']:
            add('arm64', name+str(width), [0, 1, 2], f'{name} {prefix}0, {prefix}1, {prefix}2')
        for name in ['addi', 'subi']:
            add('arm64', name+str(width), [0, 1, 123], f'{name[:-1]} {prefix}0, {prefix}1, #123')
        for name in ['ldr', 'str']:
            add('arm64', name+str(width), [0, 31, 16], f'{name} {prefix}0, [sp, #16]')
        for name in ['movz', 'movk']:
            add('arm64', name+str(width), [0, 0x1234, 1], f'{name} {prefix}0, #0x1234, lsl #16')
    for bits, suffix in [(8, 'b'), (16, 'h')]:
        for sign in [False, True]:
            add('arm64', f'load{bits}-'+('signed' if sign else 'unsigned'), [0, 31, 16],
                ('ldrs' if sign else 'ldr')+suffix+f' {"x" if sign else "w"}0, [sp, #16]')
        add('arm64', f'store{bits}', [0, 31, 16], f'str{suffix} w0, [sp, #16]')
    for name in ['fadd', 'fsub', 'fmul', 'fdiv']:
        add('arm64', name+'64', [0, 1, 2], name+' d0, d1, d2')
    add('arm64', 'ldr-f64', [0, 31, 16], 'ldr d0, [sp, #16]')
    add('arm64', 'str-f64', [0, 31, 16], 'str d0, [sp, #16]')
    add('arm64', 'fmov64', [0, 1], 'fmov d0, d1')
    add('arm64', 'fmov-to-gpr', [0, 1], 'fmov x0, d1')
    add('arm64', 'fmov-from-gpr', [0, 1], 'fmov d0, x1')
    add('arm64', 'add-v4i32', [0, 1, 2], 'add v0.4s, v1.4s, v2.4s')
    for name in ['br', 'blr', 'ret']:
        add('arm64', name, [30], name+' x30')
        add('arm64', name, [31], name+' xzr')
    for name, asm in [('nop', 'nop'), ('dmb-ish', 'dmb ish'), ('brk0', 'brk #0')]:
        add('arm64', name, [], asm)
    for name in ['add', 'sub', 'sll', 'slt', 'sltu', 'xor', 'srl', 'sra', 'or', 'and', 'mul', 'div', 'divu', 'rem', 'remu']:
        add('risv', name, [10, 11, 12], name+' a0, a1, a2')
        if name in ['add', 'sub', 'sll', 'srl', 'sra', 'mul', 'div', 'divu', 'rem', 'remu']:
            add('risv', name+'w', [10, 11, 12], name+'w a0, a1, a2')
    for name in ['addi', 'slti', 'sltiu', 'andi', 'ori', 'xori']:
        add('risv', name, [10, 11, -123], name+' a0, a1, -123')
    for name in ['slli', 'srli', 'srai']:
        add('risv', name, [10, 11, 63], name+' a0, a1, 63')
    for name in ['ld', 'lw', 'lh', 'lb', 'lwu', 'lhu', 'lbu']:
        add('risv', name, [10, 2, -8], name+' a0, -8(sp)')
    for name in ['sd', 'sw', 'sh', 'sb']:
        add('risv', name, [2, 10, -8], name+' a0, -8(sp)')
    for name in ['amoadd', 'amoswap', 'amoxor', 'amoor', 'amoand', 'amomin', 'amomax', 'amominu', 'amomaxu']:
        for suffix in ['w', 'd']:
            add('risv', name+'.'+suffix, [10, 11, 12, 3], name+'.'+suffix+'.aqrl a0, a2, (a1)')
    for width, suffix in [(32, 'w'), (64, 'd')]:
        add('risv', 'lr'+str(width), [10, 11, 2], 'lr.'+suffix+'.aq a0, (a1)')
        add('risv', 'sc'+str(width), [10, 11, 12, 1], 'sc.'+suffix+'.rl a0, a2, (a1)')
    for name in ['fadd.d', 'fsub.d', 'fmul.d', 'fdiv.d']:
        add('risv', name, [10, 11, 12, 0], name+' fa0, fa1, fa2, rne')
    add('risv', 'fld', [10, 2, -8], 'fld fa0, -8(sp)')
    add('risv', 'fsd', [2, 10, -8], 'fsd fa0, -8(sp)')
    add('risv', 'fsgnj.d', [10, 11, 12], 'fsgnj.d fa0, fa1, fa2')
    add('risv', 'fmv.x.d', [10, 11], 'fmv.x.d a0, fa1')
    add('risv', 'fmv.d.x', [10, 11], 'fmv.d.x fa0, a1')
    for name, asm in [('ecall', 'ecall'), ('ebreak', 'ebreak'), ('fence-rw-rw', 'fence rw,rw'), ('fence.i', 'fence.i')]:
        add('risv', name, [], asm)
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--encoder', required=True)
    parser.add_argument('--llvm-mc', default='llvm-mc')
    args = parser.parse_args()
    cases = fixtures()
    for arch, form, values, assembly in cases:
        target = {'amd64': 'x86_64', 'arm64': 'aarch64', 'risv': 'riscv64'}[arch]
        preamble = '.intel_syntax noprefix\n' if arch == 'amd64' else '.option norvc\n' if arch == 'risv' else ''
        flags = ['-mattr=+m,+a,+f,+d,+zifencei'] if arch == 'risv' else []
        assembled = subprocess.run([args.llvm_mc, '-triple='+target, '-show-encoding', *flags],
                                   input=preamble+assembly+'\n', text=True, capture_output=True, check=True)
        match = re.search(r'encoding: \[([^\]]+)\]', assembled.stdout)
        if not match or not all(re.fullmatch(r'0x[0-9a-fA-F]{2}', b.strip()) for b in match[1].split(',')):
            raise RuntimeError(f'non-concrete oracle encoding: {assembly}: {assembled.stdout}')
        expected = ''.join(b.strip()[2:] for b in match[1].split(',')).lower()
        actual = subprocess.check_output([args.encoder, '--encode', str(ROOT/'library/archml'/f'{arch}.aml'),
                                          form, *map(str, values)], text=True).strip()
        if actual != expected:
            raise AssertionError(f'{arch}/{form}: {assembly}: ArchML {actual}, LLVM MC {expected}')
    print(f'ArchML: {len(cases)} independent LLVM MC encoding fixtures passed')


if __name__ == '__main__':
    main()
