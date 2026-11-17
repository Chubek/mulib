"""Selected LLVM-target facts reviewed in /mnt/lambda/ArchTk/infobank/.

These are original, bounded ArchML tables, not a TableGen translation. Sources
are recorded per profile; regeneration does not access the research corpus.
Native encodings use architectural bit positions except where wire order is
specified explicitly (BPF, SPIR-V, and M68k extension words).
"""
from generate_archml import prop, q, ref, refs, operand, read, write, bv, parallel
from archml_extra import base, op, registers, ins


def target(name, bits, endian, sources, scope, **kwargs):
    p = base(name, bits, endian, sources, scope, **kwargs)
    p.filename = name + '.am'
    for w in [1, 8, 16, 32, 64]:
        p.add('type', f'i{w}', prop('bits', w), prop('kind', 'integer'),
              prop('alignment', max(1, w // 8)))
    p.add('legalize', 'unlisted', prop('action', 'diagnose'),
          prop('recipe', q('Only listed forms are supported. Expand unsupported operations only after proving target semantics, or diagnose; no host runtime or ABI fallback is implied.')))
    return p


def special(p, name, bits):
    p.storage(name, bits)
    p.reg(name, bits, name, 0)


def metadata(p, name, description):
    p.feature(name, description, ['base'])
    p.add('extension', name, prop('requires', ref('feature', name)),
          prop('coverage', 'metadata-only'), prop('semantics', q(description)))


def arm():
    p = target('arm', 32, 'little',
               'LLVM-Target/ARM/ARMInstrInfo.td (AsI1_bin_irs, AI_ldr1, AI_str1, HINT); ARMInstrFormats.td; ARMRegisterInfo.td.',
               'ARMv7 A32 little-endian user-state subset, not AArch64 or Thumb. AL-only non-flag-setting arithmetic, unshifted register operands, unrotated imm8, positive offset non-writeback memory, B/BL and NOP. All explicit PC register operands excluded; no interworking, VFP/NEON or ABI claim.')
    registers(p, 16, 32)
    for n, i in [('sp', 13), ('lr', 14), ('pc', 15)]:
        p.reg(n, 32, f'r{i}', i)
    special(p, 'cpsr', 32)
    p.add('predication', 'a32', prop('rules', q('All encoded forms fix cond=14 (AL). Arithmetic fixes S=0, preserving CPSR. Other condition fields and flag-setting variants are not selectable. A32 PC-relative branches use current PC+8; link writes LR=PC+4.')))
    rd = operand('rd', 4, cls='GPR', access='write', exclude=(15,))
    rn = operand('rn', 4, cls='GPR', exclude=(15,))
    rm = operand('rm', 4, cls='GPR', exclude=(15,))
    for n, opcode, operation in [('add', 4, 'add'), ('sub', 2, 'sub'),
                                  ('and', 0, 'and'), ('orr', 12, 'or'), ('eor', 1, 'xor')]:
        for immediate in [False, True]:
            last = operand('imm', 8) if immediate else rm
            field = ('imm', 0, 8, 0) if immediate else ('rm', 0, 4, 0)
            word = 0xe0000000 | (opcode << 21) | (int(immediate) << 25)
            p.instruction(n + ('-imm8' if immediate else '-rr'), word, [rd, rn, last],
                          [('rd', 12, 4, 0), ('rn', 16, 4, 0), field],
                          write('rd', bv(operation, 32, read('rn'), read('imm' if immediate else 'rm'))),
                          n + ' {rd}, {rn}, ' + ('#{imm}' if immediate else '{rm}'))
    for immediate in [False, True]:
        p.instruction('mov-imm8' if immediate else 'mov-rr', 0xe3a00000 if immediate else 0xe1a00000,
                      [rd, operand('imm', 8) if immediate else rm],
                      [('rd', 12, 4, 0), ('imm', 0, 8, 0) if immediate else ('rm', 0, 4, 0)],
                      write('rd', read('imm' if immediate else 'rm')),
                      'mov {rd}, ' + ('#{imm}' if immediate else '{rm}'))
    for n, width, store in [('ldr', 32, False), ('str', 32, True),
                             ('ldrb', 8, False), ('strb', 8, True)]:
        addr = bv('add', 32, read('rn'), read('offset'))
        value = f'(load {width} {addr} little {width // 8})'
        if width == 8:
            value = f'(zext 8 32 {value})'
        code = f'(store {width} {addr} (read rd) little {width // 8})' if store else write('rd', value)
        p.instruction(n, 0xe5800000 | (int(not store) << 20) | (int(width == 8) << 22),
                      [operand('rd', 4, cls='GPR', access='read' if store else 'write', exclude=(15,)), rn, operand('offset', 12)],
                      [('rd', 12, 4, 0), ('rn', 16, 4, 0), ('offset', 0, 12, 0)],
                      code, n + ' {rd}, [{rn}, #{offset}]', effects='memory',
                      constraints='This subset requires natural alignment even where the execution environment permits unaligned accesses.')
    for n, link in [('b', False), ('bl', True)]:
        code = '(jump (bvadd 32 (bvadd 32 (pc) (const 32 8)) (read disp)))'
        if link:
            code = parallel('(set-reg lr (bvadd 32 (pc) (const 32 4)))', code)
        p.instruction(n, 0xeb000000 if link else 0xea000000, [operand('disp', 24, True, 4)],
                      [('disp', 0, 24, 0)], code, n + ' {target}', effects='control',
                      implicit_def=refs('register', ['lr']) if link else '')
    p.instruction('nop', 0xe320f000, [], [], '(nop)', 'nop')
    p.add('exception', 'access', prop('rules', q('Require A32 user execution and natural alignment for this subset. Memory permission faults precede destination updates. Arithmetic wraps and leaves CPSR unchanged.')))
    metadata(p, 'thumb', 'T16/T32 state and interworking require separate instruction encodings; not supplied here.')
    metadata(p, 'neon', 'Advanced SIMD and VFP state/encodings are not supplied by this integer A32 subset.')
    return p


def amdgpu():
    p = target('amdgpu', 64, 'little',
               'LLVM-Target/AMDGPU/SOPInstructions.td (SOP1_Real, SOP2_Real32, SOPP_Real_32 and gfx6 opcode tables); SIRegisterInfo.td; GCNSubtarget.h (wave32 generation distinction).',
               'GCN gfx600 wave64 SALU subset. Only ordinary s0..s103 register sources/destinations: no inline constants, literal words or special-register operand encodings. No VALU, memory instructions, HSA ABI, R600, RDNA or cross-generation encoding claim.')
    for i in range(104):
        p.storage(f's{i}', 32)
        p.reg(f's{i}', 32, f's{i}', i)
    p.regclass('SGPR32', [f's{i}' for i in range(104)], 32)
    special(p, 'scc', 1)
    special(p, 'pc', 64)
    op(p, 'gcn-scc', 3, 'Arguments operation,a,b at width32. Set SCC to unsigned carry for add, unsigned borrow (a<b) for sub, or result!=0 for and/or/xor; preserve other state.')
    op(p, 'gcn-endpgm', 0, 'Terminate the current wave; no normal fallthrough. Kernel completion and outstanding-memory obligations belong to the execution environment.')
    rd = operand('sdst', 7, cls='SGPR32', access='write')
    a = operand('src0', 8, cls='SGPR32')
    b = operand('src1', 8, cls='SGPR32')
    for n, opcode, operation in [('s_add_u32', 0, 'add'), ('s_sub_u32', 1, 'sub'),
                                  ('s_and_b32', 14, 'and'), ('s_or_b32', 16, 'or'), ('s_xor_b32', 18, 'xor')]:
        p.instruction(n, 0x80000000 | (opcode << 23), [rd, a, b],
                      [('sdst', 16, 7, 0), ('src0', 0, 8, 0), ('src1', 8, 8, 0)],
                      parallel(write('sdst', bv(operation, 32, read('src0'), read('src1'))),
                               f'(gcn-scc {operation} (read src0) (read src1))'),
                      n + ' {sdst}, {src0}, {src1}', implicit_def=ref('register', 'scc'), effects='flags')
    p.instruction('s_mov_b32', 0xbe800300, [rd, a], [('sdst', 16, 7, 0), ('src0', 0, 8, 0)],
                  write('sdst', read('src0')), 's_mov_b32 {sdst}, {src0}')
    p.instruction('s_nop0', 0xbf800000, [], [], '(nop)', 's_nop 0')
    p.instruction('s_endpgm', 0xbf810000, [], [], '(gcn-endpgm)', 's_endpgm', effects='control')
    p.add('predication', 'wave64', prop('rules', q('SALU instructions execute once per wave, not per active lane; the listed SALU forms are not masked by EXEC. Wave divergence/EXEC manipulation and vector lane state are outside the concrete subset.')))
    metadata(p, 'valu', 'Per-lane VGPR arithmetic under EXEC, VCC carry/predicate effects, and generation-specific encodings require a separate vector backend.')
    metadata(p, 'rdna-wave32', 'GFX10 and later targets can support wave32, changing EXEC/VCC width and several lane-mask operations. This profile remains fixed to gfx600 wave64; no RDNA encoding is selectable.')
    return p


def xtensa():
    p = target('xtensa', 32, 'little',
               'LLVM-Target/Xtensa/XtensaInstrInfo.td (ArithLogic_RRR, ADDX/SUBX, ADDI, MOVI, Load_RRI8, Store_II8); XtensaInstrFormats.td; XtensaRegisterInfo.td.',
               'Little-endian Xtensa core 24-bit integer subset. Visible a0..a15 registers, no window rotation, density, configurable extensions, FP, branch or calling convention coverage.', alignment=1)
    for i in range(16):
        p.storage(f'a{i}', 32)
        p.reg(f'a{i}', 32, f'a{i}', i)
    p.regclass('AR', [f'a{i}' for i in range(16)], 32, ['a1'])
    p.reg('sp', 32, 'a1', 1)
    special(p, 'pc', 32)
    rd = operand('rd', 4, cls='AR', access='write')
    rs = operand('rs', 4, cls='AR')
    rt = operand('rt', 4, cls='AR')
    for n, opcode, operation in [('add', 8, 'add'), ('sub', 12, 'sub'), ('and', 1, 'and'), ('or', 2, 'or'), ('xor', 3, 'xor')]:
        p.instruction(n, (opcode << 20).to_bytes(3, 'little').hex(), [rd, rs, rt],
                      [('rd', 12, 4, 0), ('rs', 8, 4, 0), ('rt', 4, 4, 0)],
                      write('rd', bv(operation, 32, read('rs'), read('rt'))), n + ' {rd}, {rs}, {rt}')
    for operation, start in [('add', 9), ('sub', 13)]:
        for shift in [1, 2, 3]:
            n = operation + 'x' + str(1 << shift)
            p.instruction(n, ((start + shift - 1) << 20).to_bytes(3, 'little').hex(), [rd, rs, rt],
                          [('rd', 12, 4, 0), ('rs', 8, 4, 0), ('rt', 4, 4, 0)],
                          write('rd', bv(operation, 32, bv('shl', 32, read('rs'), f'(const 32 {shift})'), read('rt'))),
                          n + ' {rd}, {rs}, {rt}')
    p.instruction('addi', '02c000', [rd, rs, operand('imm', 8, True)],
                  [('rd', 4, 4, 0), ('rs', 8, 4, 0), ('imm', 16, 8, 0)],
                  write('rd', bv('add', 32, read('rs'), read('imm'))), 'addi {rd}, {rs}, {imm}')
    p.instruction('movi', '02a000', [rd, operand('imm', 12, True)],
                  [('rd', 4, 4, 0), ('imm', 16, 8, 0), ('imm', 8, 4, 8)],
                  write('rd', read('imm')), 'movi {rd}, {imm}')
    for n, opcode, width, signed, store in [('l8ui', 0, 8, False, False), ('l16ui', 1, 16, False, False),
                                            ('l16si', 9, 16, True, False), ('l32i', 2, 32, False, False),
                                            ('s8i', 4, 8, False, True), ('s16i', 5, 16, False, True), ('s32i', 6, 32, False, True)]:
        addr = bv('add', 32, read('rs'), read('offset'))
        value = f'(load {width} {addr} little {width // 8})'
        if width < 32:
            value = f'({"sext" if signed else "zext"} {width} 32 {value})'
        code = f'(store {width} {addr} (read rd) little {width // 8})' if store else write('rd', value)
        p.instruction(n, (2 | (opcode << 12)).to_bytes(3, 'little').hex(),
                      [operand('rd', 4, cls='AR', access='read' if store else 'write'), rs, operand('offset', 8, scale=width // 8)],
                      [('rd', 4, 4, 0), ('rs', 8, 4, 0), ('offset', 16, 8, 0)], code,
                      n + ' {rd}, {rs}, {offset}', effects='memory')
    p.add('exception', 'alignment', prop('rules', q('Require naturally aligned loads/stores and check access permissions before commit. Arithmetic wraps without condition flags. Implementations may have additional optional alignment support, not selected here.')))
    metadata(p, 'density', 'Optional 16-bit code-density forms require separate templates; this profile encodes only 24-bit instructions.')
    metadata(p, 'windowed', 'Window rotation, window overflow/underflow and call4/8/12 are not modeled by the flat visible-register subset.')
    return p


def bpf():
    p = target('bpf', 64, 'little',
               'LLVM-Target/BPF/BPFInstrFormats.td; BPFInstrInfo.td (ALU_RR/RI, JMP_RR/RI, LD_IMM64, LOAD/STORE); BPFRegisterInfo.td; MCTargetDesc/BPFMCCodeEmitter.cpp.',
               'Little-endian eBPF, not classic BPF. Selected 64/32-bit ALU, ordinary loads/stores, plain 64-bit constants, short branches and EXIT. r10 is read-only frame pointer. No helper calls, pseudo/map constants, atomics, packet accesses, verifier or kernel-version guarantee.', alignment=8)
    for i in range(11):
        p.storage(f'r{i}', 64)
        p.reg(f'r{i}', 64, f'r{i}', i)
    p.regclass('GPR', [f'r{i}' for i in range(11)], 64, ['r10'])
    # The physical frame pointer cannot be written even by explicit operands.
    # Read-only class membership is supplemented by per-destination exclusions.
    for i in range(11):
        p.reg(f'w{i}', 32, f'r{i}', i, write='zero-extend')
    p.regclass('GPR32', [f'w{i}' for i in range(11)], 32, ['w10'])
    special(p, 'pc', 64)
    op(p, 'bpf-branch', 2, 'Arguments predicate,byte-displacement: PC=PC+8+displacement if true, else PC+8. Displacements are multiples of eight. Target must be a valid first instruction slot, never the continuation of LDDW; verifier enforces control flow and termination separately.')
    op(p, 'bpf-exit', 0, 'Return r0 to the eBPF caller/execution environment; no normal fallthrough. Program type determines return interpretation. No helper-call clobber or kernel policy is inferred.')
    for width, cls, alu in [(64, 'GPR', 7), (32, 'GPR32', 4)]:
        for n, opcode, operation in [('add', 0, 'add'), ('sub', 1, 'sub'), ('mul', 2, 'mul'),
                                      ('or', 4, 'or'), ('and', 5, 'and'), ('lsh', 6, 'shl'),
                                      ('rsh', 7, 'lshr'), ('xor', 10, 'xor'), ('mov', 11, None), ('arsh', 12, 'ashr')]:
            for immediate in [False, True]:
                name = f'{n}{width}-' + ('ri' if immediate else 'rr')
                shift = n in ['lsh', 'rsh', 'arsh']
                immediate_bits = (6 if width == 64 else 5) if shift else 32
                rhs = read('imm' if immediate else 'src')
                if shift:
                    rhs = bv('and', width, rhs, f'(const {width} {width - 1})')
                value = rhs if operation is None else bv(operation, width, read('dst'), rhs)
                p.instruction(name, bytes([(opcode << 4) | alu | (0 if immediate else 8)]).hex() + '00' * 7,
                              [operand('dst', 4, cls=cls, access='write' if n == 'mov' else 'read-write', exclude=(10,)),
                               operand('imm', immediate_bits, signed=width == 64 and not shift) if immediate else operand('src', 4, cls=cls)],
                              [('dst', 8, 4, 0), ('imm', 32, immediate_bits, 0) if immediate else ('src', 12, 4, 0)],
                              write('dst', value), f'{{dst}} {"=" if n == "mov" else n + "="} {{{"imm" if immediate else "src"}}}')
    p.instruction('lddw', '18000000000000000000000000000000',
                  [operand('dst', 4, cls='GPR', access='write', exclude=(10,)), operand('imm', 64)],
                  [('dst', 8, 4, 0), ('imm', 32, 32, 0), ('imm', 96, 32, 32)],
                  write('dst', read('imm')), '{dst} = {imm} ll')
    for width, size in [(8, 0x10), (16, 8), (32, 0), (64, 0x18)]:
        for store in [False, True]:
            addr = bv('add', 64, read('base'), read('offset'))
            value = f'(load {width} {addr} little {width // 8})'
            if width < 64:
                value = f'(zext {width} 64 {value})'
            code = f'(store {width} {addr} (read value) little {width // 8})' if store else write('value', value)
            p.instruction(('stx' if store else 'ldx') + str(width), f'{0x60 | size | (3 if store else 1):02x}' + '00' * 7,
                          [operand('value', 4, cls='GPR', access='read' if store else 'write', exclude=() if store else (10,)),
                           operand('base', 4, cls='GPR'), operand('offset', 16, True)],
                          [('value', 12 if store else 8, 4, 0), ('base', 8 if store else 12, 4, 0), ('offset', 16, 16, 0)],
                          code, ('*u' + str(width) + '({base}+{offset}) = {value}') if store else ('{value} = *u' + str(width) + '({base}+{offset})'), effects='memory')
    for n, opcode, condition in [('jeq', 1, 'eq'), ('jlt', 10, 'ult'), ('jslt', 12, 'slt')]:
        for immediate in [False, True]:
            p.instruction(n + ('-ri' if immediate else '-rr'), f'{(opcode << 4) | 5 | (0 if immediate else 8):02x}' + '00' * 7,
                          [operand('lhs', 4, cls='GPR'), operand('imm', 32, True) if immediate else operand('rhs', 4, cls='GPR'), operand('disp', 16, True, 8)],
                          [('lhs', 8, 4, 0), ('imm', 32, 32, 0) if immediate else ('rhs', 12, 4, 0), ('disp', 16, 16, 0)],
                          f'(bpf-branch ({condition} (read lhs) (read {"imm" if immediate else "rhs"})) (read disp))',
                          n + ' {lhs}, ' + ('{imm}' if immediate else '{rhs}') + ', {target}', effects='control')
    p.instruction('ja', '0500000000000000', [operand('disp', 16, True, 8)], [('disp', 16, 16, 0)],
                  '(bpf-branch (const 1 1) (read disp))', 'goto {target}', effects='control')
    p.instruction('exit', '9500000000000000', [], [], '(bpf-exit)', 'exit', effects='control', implicit_use=ref('register', 'r0'))
    p.add('exception', 'verifier', prop('rules', q('Loading/encoding does not perform eBPF verification. Consumers must enforce pointer provenance, stack bounds/initialization, natural alignment, readable memory, branch slot boundaries and program-type policy before execution. ALU32 zero-extends to the shared 64-bit storage.')))
    return p


def m68k():
    p = target('m68k', 32, 'big',
               'LLVM-Target/M68k/M68kInstrArithmetic.td (MxBiArOp_R_RR_xEA/EAd); M68kInstrData.td (MxMoveEncoding); M68kInstrControl.td (NOP, RTS); M68kRegisterInfo.td.',
               '68000-compatible big-endian register-direct integer subset at byte/word/long widths, plus NOP/RTS. Data registers only for ALU/MOVE; short writes preserve upper bits. No indexed/extension addressing, supervisor operations, ColdFire, FP or ABI claim.', alignment=2)
    for i in range(8):
        p.storage(f'd{i}', 32)
        p.reg(f'd{i}', 32, f'd{i}', i)
        for width, suffix in [(8, 'b'), (16, 'w')]:
            p.reg(f'd{i}{suffix}', width, f'd{i}', i, write='preserve')
        p.storage(f'a{i}', 32)
        p.reg(f'a{i}', 32, f'a{i}', i)
    p.reg('sp', 32, 'a7', 7)
    for width, suffix in [(8, 'b'), (16, 'w'), (32, '')]:
        p.regclass(f'D{width}', [f'd{i}{suffix}' for i in range(8)], width)
    special(p, 'ccr', 8)
    special(p, 'pc', 32)
    op(p, 'm68k-flags', 4, 'Arguments operation,width,a,b. Result is a+b for add, a-b for sub, b for move, otherwise a bitwise-op b. Set N=sign(result), Z=result==0. Add/sub set V=signed overflow and C=X=unsigned carry/borrow. Logical/MOVE clear V/C and preserve X. Preserve other CCR bits. Width truncation precedes flag calculation.')
    for width, size, movesize in [(8, 0, 1), (16, 1, 3), (32, 2, 2)]:
        for n, opcode, operation in [('add', 13, 'add'), ('sub', 9, 'sub'), ('and', 12, 'and'),
                                      ('or', 8, 'or'), ('eor', 11, 'xor'), ('move', 0, None)]:
            reverse = n == 'eor'
            word = movesize << 12 if n == 'move' else (opcode << 12) | (size << 6) | (0x100 if reverse else 0)
            dst = operand('dst', 3, cls=f'D{width}', access='write' if n == 'move' else 'read-write')
            src = operand('src', 3, cls=f'D{width}')
            value = read('src') if n == 'move' else bv(operation, width, read('dst'), read('src'))
            flags = f'(m68k-flags {operation or "move"} {width} {"(const " + str(width) + " 0)" if n == "move" else read("dst")} (read src))'
            ins(p, f'{n}{width}', word, [dst, src],
                [('dst', 0 if reverse else 9, 3, 0), ('src', 9 if reverse else 0, 3, 0)],
                parallel(write('dst', value), flags), n + '.' + {8: 'b', 16: 'w', 32: 'l'}[width] + ' {src}, {dst}', length=2,
                implicit_use=ref('register', 'ccr'), implicit_def=ref('register', 'ccr'), effects='flags')
    ins(p, 'nop', 0x4e71, [], [], '(nop)', 'nop', length=2)
    ins(p, 'rts', 0x4e75, [], [], parallel('(jump (load 32 (reg sp) big 2))', '(set-reg sp (bvadd 32 (reg sp) (const 32 4)))'),
        'rts', length=2, implicit_use=ref('register', 'sp'), implicit_def=ref('register', 'sp'), effects='memory control')
    p.add('exception', 'access', prop('rules', q('68000 word/long memory accesses require even addresses; RTS checks the stack read and target before effects commit in this model. Actual bus/address-error exception frames and partial bus side effects are outside this normal-execution subset.')))
    return p


def hexagon():
    p = target('hexagon', 32, 'little',
               'LLVM-Target/Hexagon/HexagonDepInstrInfo.td (A2_add/sub/and/or/xor/tfr/nop); HexagonDepInstrFormats.td (Enc_5ab2be, Enc_bd6011, Enc_5e2823); HexagonInstrFormats.td; MCTargetDesc/HexagonBaseInfo.h (INST_PARSE_PACKET_END).',
               'Hexagon V60 scalar integer subset, each instruction is a complete single-instruction packet (parse bits 15:14=11). No multi-instruction packets, duplex, new-value forwarding, predicates, extenders, HVX, memory operations or ABI claim.')
    registers(p, 32, 32)
    for n, i in [('sp', 29), ('fp', 30), ('lr', 31)]:
        p.reg(n, 32, f'r{i}', i)
    special(p, 'pc', 32)
    rd = operand('rd', 5, cls='GPR', access='write')
    rs = operand('rs', 5, cls='GPR')
    rt = operand('rt', 5, cls='GPR')
    for n, prefix, operation in [('add', 0b11110011000, 'add'), ('sub', 0b11110011001, 'sub'),
                                  ('and', 0b11110001000, 'and'), ('or', 0b11110001001, 'or'), ('xor', 0b11110001011, 'xor')]:
        # SUB's first textual source is Rt, unlike the commutative forms.
        a, b = (read('rt'), read('rs')) if n == 'sub' else (read('rs'), read('rt'))
        p.instruction(n, (prefix << 21) | 0xc000, [rd, rs, rt],
                      [('rd', 0, 5, 0), ('rs', 16, 5, 0), ('rt', 8, 5, 0)],
                      write('rd', bv(operation, 32, a, b)),
                      '{rd} = ' + n + ('({rt},{rs})' if n == 'sub' else '({rs},{rt})'))
    p.instruction('tfr', (0b01110000011 << 21) | 0xc000, [rd, rs],
                  [('rd', 0, 5, 0), ('rs', 16, 5, 0)], write('rd', read('rs')), '{rd} = {rs}')
    p.instruction('nop', 0x7f00c000, [], [], '(nop)', 'nop')
    p.add('schedule', 'packet', prop('rules', q('Never concatenate these words inside a larger packet or change parse bits without a packetizer and slot/hazard validation. Every listed word ends its own packet; normal fallthrough advances four bytes. No silicon timing model is supplied.')))
    metadata(p, 'hvx', 'HVX vector state, length, predicates and packet resource constraints require a separate backend.')
    return p


def spirv():
    p = target('spirv', 32, 'little',
               'LLVM-Target/SPIRV/SPIRVInstrInfo.td (BinOpTypedGen and simple ops); SPIRVInstrFormats.td; MCTargetDesc/SPIRVMCCodeEmitter.cpp (word count/opcode and result-type-before-result-ID emission).',
               'Selected SPIR-V word sequences, little-endian serialization, nominal 32-bit pointer environment. IDs are nonzero 32-bit numbers, not physical registers. No complete module/header, capability inference, type checking, structured CFG validation, general variadic instruction or environment validation.',
               semantics='Instruction operands are literal encoded IDs, not register values. P-code resolves IDs/types in the module environment and binds result IDs; ordinary flow follows the validated block. Binary word counts are explicit. Module validation is required separately.')
    op(p, 'spv-bin', 5, 'Arguments operation,result-type-ID,result-ID,lhs-ID,rhs-ID. Resolve operands in the module ID environment; bind the result at the declared integer scalar/vector type. add/sub/mul wrap per component; bitwise operations act per bit. Shift counts must be less than component width or the result is undefined; no implicit count mask. Check result and source types per SPIR-V rules before evaluation.')
    op(p, 'spv-return', 0, 'Return from a void function. Requires a structurally valid block and function; no normal fallthrough.')
    op(p, 'spv-unreachable', 0, 'Marks an unreachable block terminator. Execution is undefined, not a guaranteed hardware trap. Do not infer an observable trap or normal fallthrough.')
    ids = [operand(n, 32, exclude=(0,)) for n in ['type', 'result', 'lhs', 'rhs']]
    for n, opcode, operation in [('OpIAdd', 128, 'add'), ('OpISub', 130, 'sub'), ('OpIMul', 132, 'mul'),
                                  ('OpShiftRightLogical', 194, 'lshr'), ('OpShiftRightArithmetic', 195, 'ashr'),
                                  ('OpShiftLeftLogical', 196, 'shl'), ('OpBitwiseOr', 197, 'or'),
                                  ('OpBitwiseXor', 198, 'xor'), ('OpBitwiseAnd', 199, 'and')]:
        p.instruction(n, ((5 << 16) | opcode).to_bytes(4, 'little').hex() + '00' * 16,
                      ids, [(name, 32 * (i + 1), 32, 0) for i, name in enumerate(['type', 'result', 'lhs', 'rhs'])],
                      f'(spv-bin {operation} (read type) (read result) (read lhs) (read rhs))',
                      '{result} = ' + n + ' {type} {lhs} {rhs}', effects='ssa-id-environment')
    for n, opcode, code in [('OpNop', 0, '(nop)'), ('OpReturn', 253, '(spv-return)'), ('OpUnreachable', 255, '(spv-unreachable)')]:
        p.instruction(n, ((1 << 16) | opcode).to_bytes(4, 'little').hex(), [], [], code, n, effects='control' if opcode else 'pure')
    p.add('layout', 'module', prop('rules', q('Supply the SPIR-V header, capabilities, addressing/memory model, types, definitions, functions and block structure separately. Unique result IDs must be below header bound. encode/decode only checks each local word template and nonzero IDs.')))
    return p


def value(name, width, access='read'):
    return f'({name} (kind value) (bits {width}) (signed false) (scale 1) (access {access}) (type {ref("type", f"i{width}")}))'


def nvptx():
    p = target('nvptx', 64, 'little',
               'LLVM-Target/NVPTX/NVPTXInstrInfo.td (I3, ADD, SUB, MULT, BITWISE); NVPTXInstrFormats.td; NVPTXRegisterInfo.td.',
               'NVPTX64 virtual ISA scalar unpredicated integer operations, emitted as PTX text. No NVIDIA SASS bytes or finite physical register bank. Selected arithmetic/bitwise forms only; module directives, PTX/SM version selection, register declarations, predication, memory spaces and kernel ABI remain consumer responsibilities.',
               alignment=1, status='selected-nonbinary-operations',
               semantics='P-code reads/writes typed virtual values in the current thread. No binary PC or encoded instruction length exists in PTX text. Consumer emits typed virtual registers and valid PTX module context.')
    for width in [16, 32, 64]:
        for mnemonic, operation in [('add.s', 'add'), ('sub.s', 'sub'), ('mul.lo.s', 'mul')]:
            n = mnemonic + str(width)
            p.operation(n, [value('dst', width, 'write'), value('lhs', width), value('rhs', width)],
                        write('dst', bv(operation, width, read('lhs'), read('rhs'))),
                        n + ' {dst}, {lhs}, {rhs};', 'text', effects='thread-local-values')
    for width in [32, 64]:
        for n in ['and', 'or', 'xor']:
            p.operation(f'{n}.b{width}', [value('dst', width, 'write'), value('lhs', width), value('rhs', width)],
                        write('dst', bv(n, width, read('lhs'), read('rhs'))),
                        f'{n}.b{width} {{dst}}, {{lhs}}, {{rhs}};', 'text', effects='thread-local-values')
    p.add('predication', 'thread', prop('rules', q('Only unpredicated forms are described. PTX virtual registers are per-thread typed values; a backend must not map them to a guessed finite SASS register set. Warp convergence and predicated effects require additional semantics.')))
    p.add('addressing', 'spaces', prop('rules', q('Generic, global, shared, local, constant and parameter spaces are distinct. This arithmetic-only subset encodes no memory instruction and implies no conversion among spaces.')))
    metadata(p, 'tensor', 'Tensor operations require explicit PTX and SM version, layout, synchronization and convergence contracts; no forms supplied.')
    return p


def directx():
    p = target('directx', 32, 'little',
               'LLVM-Target/DirectX/DXIL.td (SMax/SMin/UMax/UMin, ThreadId/GroupId/ThreadIdInGroup/FlattenedThreadIdInGroup); DXILOpBuilder.cpp; DXILWriter/DXILBitcodeWriter.cpp.',
               'DirectX DXIL 1.0+ scalar i32 operation metadata, not DXBC tokens or native GPU machine code. Explicit dx.op LLVM IR call templates and opcode arguments, not fixed byte encodings. No bitcode/container emitter, shader signatures/resources, validator, feature-level guarantee or physical register allocation.',
               alignment=1, status='selected-nonbinary-operations',
               semantics='P-code consumes typed shader SSA values, never physical registers. DXIL opcodes are i32 call arguments, not encoded native instructions. Emit LLVM IR in a valid DXIL module and run environment-specific validation separately.')
    op(p, 'dx-minmax', 3, 'Arguments operation,a,b at i32: smax/smin compare two-complement signed values; umax/umin compare unsigned values. Return the selected input bit pattern. No overflow, flags or memory effects.')
    op(p, 'dx-system-id', 2, 'Arguments kind,component: return the i32 dispatch thread/group/thread-in-group ID component, or flattened thread-in-group index for kind flattened. Components are 0..2; flattened has no component. Valid only in the stages declared on the operation, with valid dispatch/group-size metadata.')
    for n, opcode in [('smax', 37), ('smin', 38), ('umax', 39), ('umin', 40)]:
        p.operation(n + '.i32', [value('dst', 32, 'write'), value('lhs', 32), value('rhs', 32)],
                    write('dst', f'(dx-minmax {n} (read lhs) (read rhs))'),
                    f'{{dst}} = call i32 @dx.op.binary.i32(i32 {opcode}, i32 {{lhs}}, i32 {{rhs}})',
                    'llvm-ir', dxil_opcode=opcode, minimum_dxil=q('1.0'), stages='all', effects='pure')
    for n, opcode, kind in [('threadId', 93, 'thread'), ('groupId', 94, 'group'),
                             ('threadIdInGroup', 95, 'local'), ('flattenedThreadIdInGroup', 96, 'flattened')]:
        operands = [value('dst', 32, 'write')]
        if kind != 'flattened':
            operands.append(operand('component', 2, exclude=(3,)))
        p.operation(n + '.i32', operands,
                    write('dst', f'(dx-system-id {kind} ' + ('none)' if kind == 'flattened' else '(read component))')),
                    f'{{dst}} = call i32 @dx.op.{n}.i32(i32 {opcode}' + (')' if kind == 'flattened' else ', i32 {component})'),
                    'llvm-ir', dxil_opcode=opcode, minimum_dxil=q('1.0'),
                    stages='compute mesh amplification node',
                    constraints=q('Stage availability also depends on the shader-model/DXIL version; use compute for DXIL 1.0. Component must be a literal 0, 1 or 2.'), effects='shader-system-values')
    p.add('layout', 'container', prop('rules', q('DXIL is serialized LLVM bitcode inside a DXContainer with signatures and shader metadata. The numeric dxil-opcode properties are not independently serializable byte opcodes. No binary encoder or container writer is supplied by this profile.')))
    return p


def loongarch64():
    p = target('loongarch64', 64, 'little',
               'LLVM-Target/LoongArch/LoongArchInstrInfo.td (ALU_3R, ALU_2RI12, LOAD/STORE_2RI12, BrCC_2RI16, Br_I26, JIRL); LoongArchInstrFormats.td; LoongArchRegisterInfo.td.',
               'LoongArch LA64 little-endian base-integer subset. Selected 64-bit arithmetic, logical, variable-shift, immediate, naturally aligned load/store and direct/indirect control forms. r0 discards writes. No LA32, FP, LSX/LASX, atomics, privileged state, relocations or ABI claim.')
    registers(p, 32, 64, zero=True)
    aliases = ['zero', 'ra', 'tp', 'sp', 'a0', 'a1', 'a2', 'a3',
               'a4', 'a5', 'a6', 'a7', 't0', 't1', 't2', 't3',
               't4', 't5', 't6', 't7', 't8', None, 'fp', 's0',
               's1', 's2', 's3', 's4', 's5', 's6', 's7', 's8']
    for i, name in enumerate(aliases):
        if name:
            p.reg(name, 64, f'r{i}', i, write='discard' if i == 0 else 'replace')
    special(p, 'pc', 64)
    op(p, 'la-branch', 2, 'Arguments predicate,byte-displacement. If true set PC=PC+displacement, otherwise PC=PC+4. The displacement is signed and four-byte aligned. There is no delay slot.')
    rd = operand('rd', 5, cls='GPR', access='write')
    rj = operand('rj', 5, cls='GPR')
    rk = operand('rk', 5, cls='GPR')
    for name, word, operation in [('add.d', 0x00108000, 'add'),
                                   ('sub.d', 0x00118000, 'sub'),
                                   ('and', 0x00148000, 'and'),
                                   ('or', 0x00150000, 'or'),
                                   ('xor', 0x00158000, 'xor')]:
        p.instruction(name, word, [rd, rj, rk],
                      [('rd', 0, 5, 0), ('rj', 5, 5, 0), ('rk', 10, 5, 0)],
                      write('rd', bv(operation, 64, read('rj'), read('rk'))),
                      name + ' {rd}, {rj}, {rk}')
    for name, word, operation in [('sll.d', 0x00188000, 'shl'),
                                   ('srl.d', 0x00190000, 'lshr'),
                                   ('sra.d', 0x00198000, 'ashr')]:
        count = bv('and', 64, read('rk'), '(const 64 63)')
        p.instruction(name, word, [rd, rj, rk],
                      [('rd', 0, 5, 0), ('rj', 5, 5, 0), ('rk', 10, 5, 0)],
                      write('rd', bv(operation, 64, read('rj'), count)),
                      name + ' {rd}, {rj}, {rk}')
    for name, word, operation, signed in [('addi.d', 0x02c00000, 'add', True),
                                           ('andi', 0x03400000, 'and', False),
                                           ('ori', 0x03800000, 'or', False),
                                           ('xori', 0x03c00000, 'xor', False)]:
        p.instruction(name, word, [rd, rj, operand('imm', 12, signed)],
                      [('rd', 0, 5, 0), ('rj', 5, 5, 0), ('imm', 10, 12, 0)],
                      write('rd', bv(operation, 64, read('rj'), read('imm'))),
                      name + ' {rd}, {rj}, {imm}')
    for name, word, store in [('ld.d', 0x28c00000, False),
                               ('st.d', 0x29c00000, True)]:
        address = bv('add', 64, read('base'), read('disp'))
        code = (f'(store 64 {address} (read value) little 8)' if store
                else write('value', f'(load 64 {address} little 8)'))
        p.instruction(name, word,
                      [operand('value', 5, cls='GPR',
                               access='read' if store else 'write'),
                       operand('base', 5, cls='GPR'),
                       operand('disp', 12, True)],
                      [('value', 0, 5, 0), ('base', 5, 5, 0),
                       ('disp', 10, 12, 0)],
                      code, name + ' {value}, {disp}({base})', effects='memory')
    for name, word, condition in [('beq', 0x58000000, 'eq'),
                                   ('bne', 0x5c000000, 'ne')]:
        if condition == 'eq':
            predicate = '(eq (read lhs) (read rhs))'
        else:
            predicate = '(eq (bool-int 64 (eq (read lhs) (read rhs))) (const 64 0))'
        p.instruction(name, word,
                      [operand('lhs', 5, cls='GPR'),
                       operand('rhs', 5, cls='GPR'),
                       operand('disp', 16, True, 4)],
                      [('lhs', 5, 5, 0), ('rhs', 0, 5, 0),
                       ('disp', 10, 16, 0)],
                      f'(la-branch {predicate} (read disp))',
                      name + ' {lhs}, {rhs}, {target}', effects='control')
    for name, word, link in [('b', 0x50000000, False),
                              ('bl', 0x54000000, True)]:
        code = '(jump (bvadd 64 (pc) (read disp)))'
        if link:
            code = parallel('(set-reg ra (bvadd 64 (pc) (const 64 4)))', code)
        p.instruction(name, word, [operand('disp', 26, True, 4)],
                      [('disp', 10, 16, 0), ('disp', 0, 10, 16)],
                      code, name + ' {target}', effects='control',
                      implicit_def=ref('register', 'ra') if link else '')
    p.instruction('jirl', 0x4c000000,
                  [rd, rj, operand('disp', 16, True, 4)],
                  [('rd', 0, 5, 0), ('rj', 5, 5, 0),
                   ('disp', 10, 16, 0)],
                  parallel(write('rd', '(bvadd 64 (pc) (const 64 4))'),
                           '(jump (bvadd 64 (read rj) (read disp)))'),
                  'jirl {rd}, {rj}, {disp}', effects='control')
    p.instruction('nop', 0x03400000, [], [], '(nop)', 'nop')
    p.add('exception', 'access',
          prop('rules', q('The listed LD.D/ST.D forms require eight-byte alignment and valid mapped access before commit. Branch and JIRL targets require four-byte alignment. Arithmetic wraps; this subset has no integer condition flags.')))
    metadata(p, 'lsx-lasx', 'LSX/LASX vector state, lane semantics and encodings are not supplied by this scalar LA64 profile.')
    return p


def avr8():
    p = target('avr8', 16, 'little',
               'LLVM-Target/AVR/AVRInstrInfo.td (ADDRdRr, SUBRdRr, AND/OR/EORRdRr, MOVRdRr, LDI/SUBI/ANDI/ORIRdK, NOP); AVRInstrFormats.td (FRdRr, FRdK); AVRRegisterInfo.td; AVRDevices.td.',
               'Classic AVR 8-bit register arithmetic with a 16-bit address profile. Selected two-byte register/register and high-register/immediate forms plus NOP. No AVR32, reduced-core register remapping, program/data memory, branches, extended PC, device I/O or ABI claim.',
               alignment=2)
    for i in range(32):
        p.storage(f'r{i}', 8)
        p.reg(f'r{i}', 8, f'r{i}', i)
    p.regclass('GPR8', [f'r{i}' for i in range(32)], 8)
    for i in range(16):
        p.reg(f'r{i + 16}h', 8, f'r{i + 16}', i)
    p.regclass('HREG8', [f'r{i}h' for i in range(16, 32)], 8)
    special(p, 'sreg', 8)
    op(p, 'avr8-flags', 4, 'Arguments operation,a,b,result at width8. ADD sets H,V,N,Z,C and S=N xor V. SUB/SUBI set H,V,N,Z,C as an eight-bit subtraction and S=N xor V. AND/OR/EOR set N and Z, clear V, set S=N, and preserve H and C. Preserve T and I.')
    dst = operand('dst', 5, cls='GPR8', access='read-write')
    src = operand('src', 5, cls='GPR8')
    for name, word, operation in [('add', 0x0c00, 'add'),
                                   ('sub', 0x1800, 'sub'),
                                   ('and', 0x2000, 'and'),
                                   ('or', 0x2800, 'or'),
                                   ('eor', 0x2400, 'xor')]:
        result = bv(operation, 8, read('dst'), read('src'))
        p.instruction(name, word.to_bytes(2, 'little').hex(), [dst, src],
                      [('dst', 4, 5, 0), ('src', 0, 4, 0),
                       ('src', 9, 1, 4)],
                      parallel(write('dst', result),
                               f'(avr8-flags {name} (read dst) (read src) {result})'),
                      name + ' {dst}, {src}', effects='flags',
                      implicit_use=ref('register', 'sreg'),
                      implicit_def=ref('register', 'sreg'))
    p.instruction('mov', (0x2c00).to_bytes(2, 'little').hex(),
                  [operand('dst', 5, cls='GPR8', access='write'), src],
                  [('dst', 4, 5, 0), ('src', 0, 4, 0),
                   ('src', 9, 1, 4)],
                  write('dst', read('src')), 'mov {dst}, {src}')
    for name, opcode, operation in [('ldi', 0xe, None),
                                     ('subi', 0x5, 'sub'),
                                     ('andi', 0x7, 'and'),
                                     ('ori', 0x6, 'or')]:
        high = operand('dst', 4, cls='HREG8',
                       access='write' if name == 'ldi' else 'read-write')
        imm = operand('imm', 8)
        result = read('imm') if operation is None else bv(operation, 8, read('dst'), read('imm'))
        code = write('dst', result)
        extra = {}
        effects = 'pure'
        if operation:
            code = parallel(code, f'(avr8-flags {name} (read dst) (read imm) {result})')
            effects = 'flags'
            extra = {'implicit_use': ref('register', 'sreg'),
                     'implicit_def': ref('register', 'sreg')}
        p.instruction(name, (opcode << 12).to_bytes(2, 'little').hex(),
                      [high, imm],
                      [('dst', 4, 4, 0), ('imm', 0, 4, 0),
                       ('imm', 8, 4, 4)],
                      code, name + ' {dst}, {imm}', effects=effects, **extra)
    p.instruction('nop', '0000', [], [], '(nop)', 'nop')
    p.add('predication', 'status',
          prop('rules', q('Only the listed flag-writing forms update SREG. MOV, LDI and NOP preserve it. Register operands read pre-state, including tied destinations.')))
    metadata(p, 'reduced-core', 'AVR tiny/reduced-core devices remap the visible register file and have device-specific instruction availability; no such remapping is selected.')
    return p


def msp430():
    p = target('msp430', 16, 'little',
               'LLVM-Target/MSP430/MSP430InstrInfo.td (MOV16rr, ADD/SUB/AND/BIS/BIC/XOR16rr, JMP, NOP); MSP430InstrFormats.td (IForm16, I16rr, CJForm); MSP430RegisterInfo.td.',
               'Classic 16-bit MSP430 register-direct subset over allocatable r4..r15, plus JMP and NOP. Special-register constant-generator encodings are excluded from selectable operands. No MSP430X 20-bit extension words, byte forms, memory modes, interrupts or ABI claim.',
               alignment=2)
    for i in range(16):
        p.storage(f'r{i}', 16)
        p.reg(f'r{i}', 16, f'r{i}', i)
    p.regclass('GPR16', [f'r{i}' for i in range(4, 16)], 16)
    for name, index in [('pc', 0), ('sp', 1), ('sr', 2), ('cg', 3)]:
        p.reg(name, 16, f'r{index}', index)
    op(p, 'msp430-flags', 4, 'Arguments operation,a,b,result at width16. ADD sets C,Z,N,V from unsigned carry, zero, sign and signed overflow. SUB sets C for no borrow and corresponding Z,N,V. AND sets Z/N, clears V and sets C iff result is nonzero. XOR sets Z/N, sets C iff result is nonzero and sets V iff both input signs are one. Preserve non-arithmetic SR fields.')
    op(p, 'msp430-jump', 1, 'Argument byte-displacement. Set PC=PC+2+displacement. The signed displacement is even and ranges from -1024 through 1022 bytes.')
    dst_rw = operand('dst', 4, cls='GPR16', access='read-write')
    src = operand('src', 4, cls='GPR16')
    p.instruction('mov', (0x4000).to_bytes(2, 'little').hex(),
                  [operand('dst', 4, cls='GPR16', access='write'), src],
                  [('dst', 0, 4, 0), ('src', 8, 4, 0)],
                  write('dst', read('src')), 'mov {src}, {dst}')
    for name, opcode, operation, changes_flags in [
            ('add', 0x5, 'add', True), ('sub', 0x8, 'sub', True),
            ('and', 0xf, 'and', True), ('bis', 0xd, 'or', False),
            ('bic', 0xc, 'and-not', False), ('xor', 0xe, 'xor', True)]:
        rhs = read('src')
        if operation == 'and-not':
            rhs = bv('xor', 16, rhs, '(const 16 65535)')
            result = bv('and', 16, read('dst'), rhs)
        else:
            result = bv(operation, 16, read('dst'), rhs)
        code = write('dst', result)
        extra = {}
        effects = 'pure'
        if changes_flags:
            code = parallel(code, f'(msp430-flags {name} (read dst) (read src) {result})')
            effects = 'flags'
            extra = {'implicit_use': ref('register', 'sr'),
                     'implicit_def': ref('register', 'sr')}
        p.instruction(name, (opcode << 12).to_bytes(2, 'little').hex(),
                      [dst_rw, src],
                      [('dst', 0, 4, 0), ('src', 8, 4, 0)],
                      code, name + ' {src}, {dst}', effects=effects, **extra)
    p.instruction('jmp', (0x3c00).to_bytes(2, 'little').hex(),
                  [operand('disp', 10, True, 2)],
                  [('disp', 0, 10, 0)], '(msp430-jump (read disp))',
                  'jmp {target}', effects='control')
    p.instruction('nop', '0343', [], [], '(nop)', 'nop')
    p.add('exception', 'control',
          prop('rules', q('JMP uses the PC of the instruction plus two as its base and has no delay slot. This profile does not expose r0..r3 through ordinary register operands, avoiding PC/SR/constant-generator reinterpretation.')))
    metadata(p, 'msp430x', 'MSP430X adds 20-bit addressing and extension-word forms; those encodings and address semantics are not supplied.')
    return p


def sparc():
    p = target('sparc', 32, 'big',
               'LLVM-Target/Sparc/SparcInstrInfo.td (F3_12 ADD/SUB/AND/OR/XOR, SETHIi, BA, NOP); SparcInstrFormats.td (F2_1, F2_2, F3_1, F3_2); SparcRegisterInfo.td.',
               'SPARC V8 big-endian visible integer-window subset. Non-cc register/register and simm13 arithmetic/logical forms, SETHI, delayed unconditional BA and NOP. The 32 names represent the current window view; SAVE/RESTORE and window traps are excluded. No V9, FP/VIS, memory, privileged state or ABI claim.')
    registers(p, 32, 32, zero=True)
    aliases = ([f'g{i}' for i in range(8)] +
               [f'o{i}' for i in range(6)] + ['sp', 'o7'] +
               [f'l{i}' for i in range(8)] +
               [f'i{i}' for i in range(6)] + ['fp', 'i7'])
    for i, name in enumerate(aliases):
        p.reg(name, 32, f'r{i}', i, write='discard' if i == 0 else 'replace')
    special(p, 'pc', 32)
    op(p, 'sparc-delayed-branch', 1, 'Argument byte-displacement. Execute the instruction at PC+4, then set PC=PC+displacement. A control transfer in the delay slot is outside this profile. Delay-slot faults are attributed according to SPARC trap state, not modeled as an ordinary immediate jump.')
    rd = operand('rd', 5, cls='GPR', access='write')
    rs1 = operand('rs1', 5, cls='GPR')
    rs2 = operand('rs2', 5, cls='GPR')
    for name, op3, operation in [('add', 0, 'add'), ('sub', 4, 'sub'),
                                  ('and', 1, 'and'), ('or', 2, 'or'),
                                  ('xor', 3, 'xor')]:
        base_word = (2 << 30) | (op3 << 19)
        pcode = write('rd', bv(operation, 32, read('rs1'), read('rs2')))
        ins(p, name + '-rr', base_word, [rd, rs1, rs2],
            [('rd', 25, 5, 0), ('rs1', 14, 5, 0), ('rs2', 0, 5, 0)],
            pcode, name + ' %{rs1}, %{rs2}, %{rd}')
        pcode = write('rd', bv(operation, 32, read('rs1'), read('imm')))
        ins(p, name + '-ri', base_word | (1 << 13),
            [rd, rs1, operand('imm', 13, True)],
            [('rd', 25, 5, 0), ('rs1', 14, 5, 0), ('imm', 0, 13, 0)],
            pcode, name + ' %{rs1}, {imm}, %{rd}')
    ins(p, 'sethi', 4 << 22,
        [rd, operand('imm', 22)],
        [('rd', 25, 5, 0), ('imm', 0, 22, 0)],
        write('rd', bv('shl', 32, read('imm'), '(const 32 10)')),
        'sethi {imm}, %{rd}')
    ins(p, 'ba', (8 << 25) | (2 << 22),
        [operand('disp', 22, True, 4)], [('disp', 0, 22, 0)],
        '(sparc-delayed-branch (read disp))', 'ba {target}',
        effects='control delay-slot')
    ins(p, 'nop', 0x01000000, [], [], '(nop)', 'nop')
    p.add('layout', 'register-window',
          prop('rules', q('g0..g7, o0..o7, l0..l7 and i0..i7 are aliases for the currently visible encodings 0..31. This flat subset does not rotate windows. A backend must model SAVE/RESTORE, CWP and window overflow/underflow before selecting those operations.')))
    metadata(p, 'v9', 'SPARC V9 widens integer state and adds new control, branch-prediction and privileged forms; this V8 subset does not select them.')
    return p


def systemz():
    p = target('systemz', 64, 'big',
               'LLVM-Target/SystemZ/SystemZInstrInfo.td (LGR, AGR, SGR, NGR, OGR, XGR, LGHI, BCR/NOPR); SystemZInstrFormats.td (InstRRE, InstRIa, InstRR); SystemZRegisterInfo.td.',
               'z/Architecture 64-bit big-endian register-only subset. Selected RRE moves/arithmetic/logical forms, LGHI and NOPR. Arithmetic/logical operations update the condition code. No memory addressing, branches other than the nonbranching NOPR mask, vector/floating facilities, privileged state, ABI or processor-level claim.',
               alignment=2)
    for i in range(16):
        p.storage(f'r{i}', 64)
        p.reg(f'r{i}', 64, f'r{i}', i)
    p.regclass('GPR64', [f'r{i}' for i in range(16)], 64)
    special(p, 'cc', 2)
    special(p, 'pc', 64)
    op(p, 'systemz-cc', 4, 'Arguments operation,width,a,b. Signed add/sub set CC=0 for zero, 1 for negative, 2 for positive, 3 for signed overflow. AND/OR/XOR set CC=0 for zero and CC=1 otherwise. Preserve other program-status-word fields.')
    dst_write = operand('dst', 4, cls='GPR64', access='write')
    dst_rw = operand('dst', 4, cls='GPR64', access='read-write')
    src = operand('src', 4, cls='GPR64')
    ins(p, 'lgr', 0xb9040000, [dst_write, src],
        [('dst', 4, 4, 0), ('src', 0, 4, 0)],
        write('dst', read('src')), 'lgr %{dst}, %{src}')
    for name, opcode, operation in [('agr', 0xb908, 'add'),
                                     ('sgr', 0xb909, 'sub'),
                                     ('ngr', 0xb980, 'and'),
                                     ('ogr', 0xb981, 'or'),
                                     ('xgr', 0xb982, 'xor')]:
        result = bv(operation, 64, read('dst'), read('src'))
        ins(p, name, opcode << 16, [dst_rw, src],
            [('dst', 4, 4, 0), ('src', 0, 4, 0)],
            parallel(write('dst', result),
                     f'(systemz-cc {operation} 64 (read dst) (read src))'),
            name + ' %{dst}, %{src}', effects='flags',
            implicit_def=ref('register', 'cc'))
    ins(p, 'lghi', 0xa7090000,
        [dst_write, operand('imm', 16, True)],
        [('dst', 20, 4, 0), ('imm', 0, 16, 0)],
        write('dst', read('imm')), 'lghi %{dst}, {imm}')
    ins(p, 'nopr', 0x0700, [src], [('src', 0, 4, 0)],
        '(nop)', 'nopr %{src}', length=2)
    p.add('predication', 'condition-code',
          prop('rules', q('The listed AGR/SGR/NGR/OGR/XGR forms define the two-bit condition code. LGR, LGHI and NOPR preserve it. NOPR fixes BCR mask=0, so no register value can cause a branch.')))
    metadata(p, 'vector', 'The z/Architecture vector facility has distinct 128-bit registers, masks and facility requirements; no vector forms are supplied.')
    return p


def evm():
    p = target('evm', 64, 'big',
               'Capstone-Arch/EVM/EVMDisassembler.c opcode table; EVMMappingInsn.inc stack-pop/push/gas table; Radare2-Opcodes/evm/evm.c and evm.h opcode/length tables.',
               'Ethereum Virtual Machine 256-bit stack-machine subset represented through ArchML v1\'s 64-bit scalar encoder interface. Selected arithmetic, comparison, bitwise, POP, JUMPDEST, PUSH0/PUSH1..PUSH8, DUP1..16 and SWAP1..16 bytecodes. PUSH9..PUSH32 exceed the scalar operand width and are excluded. No gas schedule, memory/storage, calls, logs, jumps, fork activation, validation or transaction-environment claim.',
               alignment=1,
               semantics='P-code operates on the EVM stack of 256-bit words. Normal fallthrough advances by the full opcode/immediate length. Every operation first checks fork availability, gas, stack underflow and the 1024-word stack limit; exceptional halt commits no partial stack effect.')
    p.add('type', 'i256', prop('bits', 256), prop('kind', 'integer'),
          prop('alignment', 32))
    p.add('layout', 'operand-stack',
          prop('element-bits', 256), prop('maximum-depth', 1024),
          prop('rules', q('Top-of-stack is operand a. Binary noncommutative arithmetic pops a then b and computes a op b; shift/byte/sign-extension operations interpret a as the index/count and b as the value. Stack checks and gas charging precede effects.')))
    op(p, 'evm-stop', 0, 'Halt successfully with empty return data and no normal fallthrough. Gas and enclosing call-frame result handling belong to the execution environment.')
    op(p, 'evm-bin', 1, 'Argument operation. Pop a then b. ADD/MUL/AND/OR/XOR wrap modulo 2^256. SUB/DIV/SDIV/MOD/SMOD compare or calculate a with b in EVM order; zero divisors yield zero and signed overflow follows EVM rules. LT/GT/SLT/SGT/EQ push 0 or 1. BYTE, SIGNEXTEND and SHL/SHR/SAR use a as index/count and b as value, with EVM out-of-range behavior. EXP is modular exponentiation.')
    op(p, 'evm-ternary', 1, 'Argument addmod or mulmod. Pop a,b,n and push the mathematically unbounded sum/product modulo n; push zero when n is zero. The intermediate is not truncated before modulus.')
    op(p, 'evm-unary', 1, 'Argument iszero or not. Pop a; ISZERO pushes one iff a is zero, otherwise zero. NOT pushes the 256-bit complement.')
    op(p, 'evm-push', 1, 'Push the supplied unsigned immediate, zero-extended to 256 bits. PUSH0 supplies zero. Immediate bytes are big-endian and part of the instruction; truncated bytecode is invalid.')
    op(p, 'evm-pop', 0, 'Discard the top stack word after underflow and gas checks.')
    op(p, 'evm-dup', 1, 'Argument depth 1..16. Copy that stack item to the top without removing the original; enforce input depth and 1024-word output limit.')
    op(p, 'evm-swap', 1, 'Argument depth 1..16. Exchange the top with the item depth positions below it; require at least depth+1 stack words.')
    p.instruction('stop', '00', [], [], '(evm-stop)', 'STOP',
                  effects='control')
    for name, opcode in [('add', 0x01), ('mul', 0x02), ('sub', 0x03),
                          ('div', 0x04), ('sdiv', 0x05), ('mod', 0x06),
                          ('smod', 0x07), ('exp', 0x0a),
                          ('signextend', 0x0b), ('lt', 0x10),
                          ('gt', 0x11), ('slt', 0x12), ('sgt', 0x13),
                          ('eq', 0x14), ('and', 0x16), ('or', 0x17),
                          ('xor', 0x18), ('byte', 0x1a), ('shl', 0x1b),
                          ('shr', 0x1c), ('sar', 0x1d)]:
        p.instruction(name, f'{opcode:02x}', [], [],
                      f'(evm-bin {name})', name.upper(),
                      effects='operand-stack')
    for name, opcode in [('addmod', 0x08), ('mulmod', 0x09)]:
        p.instruction(name, f'{opcode:02x}', [], [],
                      f'(evm-ternary {name})', name.upper(),
                      effects='operand-stack')
    for name, opcode in [('iszero', 0x15), ('not', 0x19)]:
        p.instruction(name, f'{opcode:02x}', [], [],
                      f'(evm-unary {name})', name.upper(),
                      effects='operand-stack')
    p.instruction('pop', '50', [], [], '(evm-pop)', 'POP',
                  effects='operand-stack')
    p.instruction('jumpdest', '5b', [], [], '(nop)', 'JUMPDEST',
                  effects='control-flow-marker')
    p.feature('push0', 'EIP-3855 PUSH0 concrete opcode; activation depends on the selected chain fork.', ['base'])
    p.add('extension', 'push0', prop('requires', ref('feature', 'push0')),
          prop('coverage', 'concrete'), prop('semantics', q('Opcode 0x5f pushes one zero word and consumes no immediate bytes. Fork activation is external.')))
    p.instruction('push0', '5f', [], [], '(evm-push 0)', 'PUSH0',
                  features=('push0',), effects='operand-stack')
    for width in range(1, 9):
        fields = [('value', 8 + byte * 8, 8, (width - 1 - byte) * 8)
                  for byte in range(width)]
        p.instruction(f'push{width}', f'{0x5f + width:02x}' + '00' * width,
                      [operand('value', width * 8)], fields,
                      '(evm-push (read value))',
                      f'PUSH{width} {{value}}', effects='operand-stack')
    for depth in range(1, 17):
        p.instruction(f'dup{depth}', f'{0x7f + depth:02x}', [], [],
                      f'(evm-dup {depth})', f'DUP{depth}',
                      effects='operand-stack')
        p.instruction(f'swap{depth}', f'{0x8f + depth:02x}', [], [],
                      f'(evm-swap {depth})', f'SWAP{depth}',
                      effects='operand-stack')
    p.add('exception', 'execution',
          prop('rules', q('Out of gas, stack underflow/overflow, unavailable fork opcodes and malformed truncated PUSH immediates cause exceptional halt. The profile records no gas constants because they vary by opcode, state and fork. Encoding a byte is not proof that a chain accepts it.')))
    metadata(p, 'stateful', 'Memory, storage, transient storage, environmental, call/create, log, return/revert and self-destruct bytecodes require gas, rollback and call-frame semantics not supplied here.')
    return p


def profiles():
    return [amdgpu, arm, nvptx, spirv, xtensa, bpf, directx, m68k, hexagon,
            loongarch64, avr8, msp430, sparc, systemz, evm]
