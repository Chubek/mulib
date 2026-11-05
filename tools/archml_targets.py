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
               'LLVM-Target/AMDGPU/SOPInstructions.td (SOP1_Real, SOP2_Real32, SOPP_Real_32 and gfx6 opcode tables); SIRegisterInfo.td.',
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


def profiles():
    return [amdgpu, arm, nvptx, spirv, xtensa, bpf, directx, m68k, hexagon]
