#!/usr/bin/env python3
"""Compare ArchML's C encoder with independent LLVM MC assembler encodings."""
import argparse
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

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
        add('riscv64', name, [10, 11, 12], name+' a0, a1, a2')
        if name in ['add', 'sub', 'sll', 'srl', 'sra', 'mul', 'div', 'divu', 'rem', 'remu']:
            add('riscv64', name+'w', [10, 11, 12], name+'w a0, a1, a2')
    for name in ['addi', 'slti', 'sltiu', 'andi', 'ori', 'xori']:
        add('riscv64', name, [10, 11, -123], name+' a0, a1, -123')
    for name in ['slli', 'srli', 'srai']:
        add('riscv64', name, [10, 11, 63], name+' a0, a1, 63')
    for name in ['ld', 'lw', 'lh', 'lb', 'lwu', 'lhu', 'lbu']:
        add('riscv64', name, [10, 2, -8], name+' a0, -8(sp)')
    for name in ['sd', 'sw', 'sh', 'sb']:
        add('riscv64', name, [2, 10, -8], name+' a0, -8(sp)')
    for name in ['amoadd', 'amoswap', 'amoxor', 'amoor', 'amoand', 'amomin', 'amomax', 'amominu', 'amomaxu']:
        for suffix in ['w', 'd']:
            add('riscv64', name+'.'+suffix, [10, 11, 12, 3], name+'.'+suffix+'.aqrl a0, a2, (a1)')
    for width, suffix in [(32, 'w'), (64, 'd')]:
        add('riscv64', 'lr'+str(width), [10, 11, 2], 'lr.'+suffix+'.aq a0, (a1)')
        add('riscv64', 'sc'+str(width), [10, 11, 12, 1], 'sc.'+suffix+'.rl a0, a2, (a1)')
    for name in ['fadd.d', 'fsub.d', 'fmul.d', 'fdiv.d']:
        add('riscv64', name, [10, 11, 12, 0], name+' fa0, fa1, fa2, rne')
    add('riscv64', 'fld', [10, 2, -8], 'fld fa0, -8(sp)')
    add('riscv64', 'fsd', [2, 10, -8], 'fsd fa0, -8(sp)')
    add('riscv64', 'fsgnj.d', [10, 11, 12], 'fsgnj.d fa0, fa1, fa2')
    add('riscv64', 'fmv.x.d', [10, 11], 'fmv.x.d a0, fa1')
    add('riscv64', 'fmv.d.x', [10, 11], 'fmv.d.x fa0, a1')
    for name, asm in [('ecall', 'ecall'), ('ebreak', 'ebreak'), ('fence-rw-rw', 'fence rw,rw'), ('fence.i', 'fence.i')]:
        add('riscv64', name, [], asm)
    for name in ['add', 'subf', 'mulld', 'and', 'or', 'xor']:
        add('ppc64', name, [3, 4, 5], name+' 3, 4, 5')
    for name in ['addi', 'addis', 'ori', 'oris', 'xori', 'xoris']:
        add('ppc64', name, [3, 4, 1234], name+' 3, 4, 1234')
    add('ppc64', 'addi', [3, 0, -123], 'addi 3, 0, -123')
    for name in ['lbz', 'lhz', 'lwz', 'ld', 'stb', 'sth', 'stw', 'std']:
        add('ppc64', name, [31, 0, -16], name+' 31, -16(0)')
    for name in ['b', 'bl']:
        add('ppc64', name, [16], name+' 16')
    add('ppc64', 'nop', [], 'nop')
    for name in ['addu', 'subu', 'and', 'or', 'xor', 'slt', 'sltu']:
        add('mips', name, [2, 4, 5], name+' $2, $4, $5')
    for name in ['sll', 'srl', 'sra']:
        add('mips', name, [2, 5, 31], name+' $2, $5, 31')
    for name in ['addiu', 'andi', 'ori', 'xori']:
        add('mips', name, [2, 4, 1234], name+' $2, $4, 1234')
    add('mips', 'lui', [2, 65535], 'lui $2, 65535')
    for name in ['lb', 'lbu', 'lh', 'lhu', 'lw', 'sb', 'sh', 'sw']:
        add('mips', name, [2, 29, -16], name+' $2, -16($29)')
    for name in ['beq', 'bne']:
        add('mips', name, [4, 5, -4], name+' $4, $5, -4')
    add('mips', 'jr', [31], 'jr $31')
    for form, assembly in [('nop', 'nop'), ('syscall0', 'syscall 0'), ('break0', 'break 0')]:
        add('mips', form, [], assembly)
    for name in ['add', 'sub', 'and', 'orr', 'eor']:
        add('arm', name+'-rr', [12, 13, 14], name+' r12, sp, lr')
        add('arm', name+'-imm8', [12, 13, 255], name+' r12, sp, #255')
    add('arm', 'mov-rr', [12, 14], 'mov r12, lr')
    add('arm', 'mov-imm8', [12, 255], 'mov r12, #255')
    for name in ['ldr', 'str', 'ldrb', 'strb']:
        add('arm', name, [12, 13, 4092], name+' r12, [sp, #4092]')
    for name in ['b', 'bl']:
        add('arm', name, [-8], name+' #-8')
    add('arm', 'nop', [], 'nop')
    for name in ['s_add_u32', 's_sub_u32', 's_and_b32', 's_or_b32', 's_xor_b32']:
        add('amdgpu', name, [103, 102, 101], name+' s103, s102, s101')
    add('amdgpu', 's_mov_b32', [103, 102], 's_mov_b32 s103, s102')
    add('amdgpu', 's_nop0', [], 's_nop 0')
    add('amdgpu', 's_endpgm', [], 's_endpgm')
    for name in ['add', 'sub', 'and', 'or', 'xor']:
        add('hexagon', name, [31, 29, 30], 'r31 = '+name+('(r30,r29)' if name=='sub' else '(r29,r30)'))
    add('hexagon', 'tfr', [31, 29], 'r31 = r29')
    add('hexagon', 'nop', [], 'nop')
    for width, prefix in [(64, 'r'), (32, 'w')]:
        for name, symbol in [('add', '+='), ('sub', '-='), ('mul', '*='), ('or', '|='), ('and', '&='),
                             ('lsh', '<<='), ('rsh', '>>='), ('xor', '^='), ('mov', '='), ('arsh', 's>>=')]:
            add('bpf', f'{name}{width}-rr', [9, 10], f'{prefix}9 {symbol} {prefix}10')
            imm = (63 if width==64 else 31) if name in ['lsh', 'rsh', 'arsh'] else 1234
            add('bpf', f'{name}{width}-ri', [9, imm], f'{prefix}9 {symbol} {imm}')
    add('bpf', 'mov64-ri', [9, -1], 'r9 = -1')
    add('bpf', 'lddw', [9, 0x1122334455667788], 'r9 = 0x1122334455667788 ll')
    for width in [8, 16, 32, 64]:
        add('bpf', 'ldx'+str(width), [9, 10, -16], f'r9 = *(u{width} *)(r10 - 16)')
        add('bpf', 'stx'+str(width), [9, 10, -16], f'*(u{width} *)(r10 - 16) = r9')
    for name, symbol in [('jeq', '=='), ('jlt', '<'), ('jslt', 's<')]:
        add('bpf', name+'-rr', [9, 10, -8], f'if r9 {symbol} r10 goto -1')
        add('bpf', name+'-ri', [9, -1, 16], f'if r9 {symbol} -1 goto +2')
    add('bpf', 'ja', [-8], 'goto -1')
    add('bpf', 'exit', [], 'exit')
    return cases


def spirv_fixtures(encoder, assembler):
    """SPIRV has no LLVM MC text parser; SPIRV-Tools supplies an independent one."""
    names = ['OpIAdd', 'OpISub', 'OpIMul', 'OpShiftRightLogical', 'OpShiftRightArithmetic',
             'OpShiftLeftLogical', 'OpBitwiseOr', 'OpBitwiseXor', 'OpBitwiseAnd']
    source = '''OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %30 "main"
OpExecutionMode %30 LocalSize 1 1 1
%1 = OpTypeInt 32 0
%2 = OpConstant %1 4
%3 = OpConstant %1 1
%20 = OpTypeVoid
%21 = OpTypeFunction %20
%30 = OpFunction %20 None %21
%31 = OpLabel
OpNop
'''
    for i, name in enumerate(names):
        source += f'%{i+4} = {name} %1 %2 %3\n'
    source += 'OpReturn\n%32 = OpLabel\nOpUnreachable\nOpFunctionEnd\n'
    with tempfile.TemporaryDirectory(prefix='archml-') as temp:
        asm, obj = Path(temp)/'test.spvasm', Path(temp)/'test.spv'
        asm.write_text(source)
        subprocess.run([assembler, '--preserve-numeric-ids', str(asm), '-o', str(obj)], check=True)
        data = obj.read_bytes()
    fixtures = {128: names[0], 130: names[1], 132: names[2],
                194: names[3], 195: names[4], 196: names[5],
                197: names[6], 198: names[7], 199: names[8],
                0: 'OpNop', 253: 'OpReturn', 255: 'OpUnreachable'}
    offset, count = 20, 0
    while offset < len(data):
        word, = struct.unpack_from('<I', data, offset)
        size, opcode = (word >> 16)*4, word & 0xffff
        assert size >= 4 and offset + size <= len(data)
        if opcode in fixtures:
            values = struct.unpack_from('<'+'I'*(size//4-1), data, offset+4)
            actual = subprocess.check_output([encoder, '--encode', str(ROOT/'library/archml/spirv.am'),
                                              fixtures[opcode], *map(str, values)], text=True).strip()
            assert actual == data[offset:offset+size].hex(), (fixtures[opcode], actual)
            count += 1
        offset += size
    assert count == 12
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--encoder', required=True)
    parser.add_argument('--llvm-mc', default='llvm-mc')
    parser.add_argument('--spirv-as', default=shutil.which('spirv-as'))
    args = parser.parse_args()
    cases = fixtures()
    for arch, form, values, assembly in cases:
        target = {'amd64': 'x86_64', 'arm64': 'aarch64', 'riscv64': 'riscv64', 'ppc64': 'powerpc64', 'mips': 'mips',
                  'arm': 'armv7', 'amdgpu': 'amdgcn', 'hexagon': 'hexagon', 'bpf': 'bpfel'}[arch]
        preamble = '.intel_syntax noprefix\n' if arch == 'amd64' else '.option norvc\n' if arch == 'riscv64' else '.set noreorder\n' if arch == 'mips' else ''
        flags = {'riscv64': ['-mattr=+m,+a,+f,+d,+zifencei'], 'mips': ['-mcpu=mips32r2'],
                 'amdgpu': ['-mcpu=gfx600'], 'hexagon': ['-mcpu=hexagonv60']}.get(arch, [])
        assembled = subprocess.run([args.llvm_mc, '-triple='+target, '-show-encoding', *flags],
                                   input=preamble+assembly+'\n', text=True, capture_output=True, check=True)
        match = re.search(r'encoding: \[([^\]]+)\]', assembled.stdout)
        if not match or not all(re.fullmatch(r'0x[0-9a-fA-F]{2}', b.strip()) for b in match[1].split(',')):
            raise RuntimeError(f'non-concrete oracle encoding: {assembly}: {assembled.stdout}')
        expected = ''.join(b.strip()[2:] for b in match[1].split(',')).lower()
        suffix = 'am' if arch in ['arm', 'amdgpu', 'hexagon', 'bpf'] else 'aml'
        actual = subprocess.check_output([args.encoder, '--encode', str(ROOT/'library/archml'/f'{arch}.{suffix}'),
                                          form, *map(str, values)], text=True).strip()
        if actual != expected:
            raise AssertionError(f'{arch}/{form}: {assembly}: ArchML {actual}, LLVM MC {expected}')
    print(f'ArchML: {len(cases)} independent LLVM MC encoding fixtures passed')
    if args.spirv_as:
        count = spirv_fixtures(args.encoder, args.spirv_as)
        print(f'ArchML: {count} independent SPIRV-Tools encoding fixtures passed')
    else:
        print('ArchML: SPIRV-Tools oracle skipped (spirv-as not installed)')


if __name__ == '__main__':
    main()
