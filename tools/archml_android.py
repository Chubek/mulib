"""Original DEX encoding tables reviewed against standards/android (AOSP docs).

DEX registers are frame indices, NOT a finite physical register bank. They are
encoded as immediate indices and accessed through explicit dex-read/dex-write
P-code operations; frame/type verification is the consumer's responsibility.
"""
from generate_archml import Profile, prop, q, ref, operand, read, bv


def dex():
    p = Profile('dex')
    p.filename = 'dex.am'
    p.add('target', 'dex', prop('bits', 32), prop('endian', 'little'),
          prop('default-features', ref('feature', 'base')),
          prop('sources', q('standards/android/android-bytecode.txt; android-dex.txt; android-art.txt; android-abi.txt. Original tables, not copied implementation code.')),
          prop('semantics', q('16-bit little-endian code units. Register operands are virtual frame indices. P-code reads pre-state; checked effects commit after faults. Branch operands are byte displacements from current instruction, encoded in code units (scale 2).')))
    p.feature('base', 'Selected DEX035 concrete integer/control/move/invoke forms.')
    p.add('mode', 'user', prop('bits', 32), prop('scope', q('Managed static or instance method frames; not native CPU execution.')))
    p.add('schedule', 'alu', prop('model', 'unspecified'))
    p.add('layout', 'code', prop('alignment', 2), prop('code-unit-bits', 16),
          prop('payload-alignment', 4), prop('rules', q('Data payloads cannot be executed or branched into. Payload encodings are outside this profile.')))
    p.add('coverage', 'profile', prop('status', 'selected-concrete-forms'),
          prop('contract', q('Concrete encodings do not verify methods, constant pools, register pairs, argument types or result adjacency. The MuLIB ART interpreter implements only the integer subset documented in mulib-art.h; other forms here are compiler metadata, not runtime support.')),
          prop('non-goals', q('No quickened opcodes, payloads, objects/GC implementation, floating arithmetic, dex2oat/OAT/VDEX format or complete ISA claim.')))
    ops = {
        'read': (1, 'Read an encoded register INDEX, literal or scaled displacement operand.'),
        'const': (2, 'width,value: construct bitvector modulo 2^width.'),
        'dex-read': (2, 'index,width: read frame register bits; width64 reads an adjacent pair, low word first. Check type and pair bounds.'),
        'dex-write': (3, 'index,width,value: write low width bits to a frame slot or adjacent pair. Both halves of a wide source are read before either destination is written.'),
        'dex-ref-move': (2, 'dest,src indices: copy managed reference, preserving GC roots; reference width is VM-defined.'),
        'dex-return': (2, 'kind,index: return int/wide/object register value, or void with index0. Verify method return type.'),
        'dex-result': (2, 'kind,dest: consume invoke result. Must immediately follow matching invoke; cannot be a branch target.'),
        'dex-neg': (2, 'width,value: modular two-complement negation.'),
        'dex-not': (2, 'width,value: ones complement at selected width.'),
        'dex-div': (3, 'width,a,b: signed integer quotient truncated toward zero; zero throws ArithmeticException; MIN/-1 returns MIN.'),
        'dex-rem': (3, 'width,a,b: signed remainder; zero throws ArithmeticException; MIN%-1 is zero.'),
        'dex-count': (2, 'width,value: mask shift distance to 31 for int or 63 for long.'),
        'dex-branch': (3, 'condition,target,fallthrough: choose checked executable byte PC. Both target and fallthrough must be instruction boundaries.'),
        'dex-test': (3, 'condition,a,b: signed eq/ne/lt/ge/gt/le on equal-width integer values; eq/ne also apply to references in verified reference contexts.'),
        'dex-invoke': (8, 'kind,method,word-count,c,d,e,f,g: invoke using up to5 encoded argument word registers. Wide args consume consecutive words; verify pool, signature, outs_size, receiver and access.'),
        'dex-invoke-range': (4, 'kind,method,word-count,first: invoke a contiguous argument-word range. Result is held separately until matching move-result.'),
        'pc': (0, 'Current byte PC.'), 'next-pc': (0, 'Byte PC plus concrete instruction length.'),
        'jump': (1, 'Set byte PC after checking instruction boundary and executable code.'),
        'nop': (0, 'No effects other than normal fallthrough.'),
        'bvadd': (3, 'width,a,b: modular addition.'), 'bvsub': (3, 'width,a,b: modular subtraction.'),
        'bvmul': (3, 'width,a,b: low modular product.'), 'bvand': (3, 'width,a,b: bitwise AND.'),
        'bvor': (3, 'width,a,b: bitwise OR.'), 'bvxor': (3, 'width,a,b: bitwise XOR.'),
        'bvshl': (3, 'width,a,count: bitvector left shift with explicit count.'),
        'bvashr': (3, 'width,a,count: signed bitvector right shift.'),
        'bvlshr': (3, 'width,a,count: unsigned bitvector right shift.')
    }
    for name, (arity, semantics) in ops.items():
        p.add('pcode-op', name, prop('arity', f'{arity} {arity}'), prop('semantics', q(semantics)))
    p.add('abi', 'dex-frame', prop('slot-bits', 32), prop('wide-slots', 2),
          prop('rules', q('Incoming argument words occupy the LAST ins_size registers in order. Instance receiver is first argument. Wide pairs need not be even aligned. Object null is all-zero. Registers are NOT a native calling convention.')))
    p.add('exception', 'precise', prop('rules', q('Division by zero throws managed ArithmeticException. Verification rejects uninitialized registers, mistyped operations and invalid targets. Try/catch metadata and stack unwinding belong to the runtime, not the encoder.')))
    for feature, contract in [
        ('art-jit', 'ART uses profile-guided native JIT compilation; code-cache/safepoint/GC integration is runtime-owned.'),
        ('art-aot', 'ART dex2oat emits native artifacts, not DEX bytecode. MuLIB emits portable C baseline AOT for its documented method subset, not OAT.'),
        ('art-gc', 'AOSP ART implements managed allocation, concurrent GC and reference barriers; these are metadata-only here, not implemented by the MuLIB integer VM.')]:
        p.feature(feature, contract, ['base'])
        p.add('extension', feature, prop('coverage', 'metadata-only'), prop('requires', ref('feature', feature)), prop('semantics', q(contract)))

    def reg(name, bits):
        return operand(name, bits)

    def r(name, width=32):
        return f'(dex-read {read(name)} {width})'

    def w(name, value, width=32):
        return f'(dex-write {read(name)} {width} {value})'

    def ins(name, opcode, units, operands, fields, code, **extra):
        base = bytes([opcode, 0]) + bytes(units * 2 - 2)
        p.instruction(name, base.hex(), operands, fields, code, name, **extra)

    ins('nop', 0, 1, [], [], '(nop)')
    for group, opcode, width in [('move', 1, 32), ('move-wide', 4, 64), ('move-object', 7, 0)]:
        for suffix, units, dbits, sbits, fields in [
            ('', 1, 4, 4, [('dst', 8, 4, 0), ('src', 12, 4, 0)]),
            ('/from16', 2, 8, 16, [('dst', 8, 8, 0), ('src', 16, 16, 0)]),
            ('/16', 3, 16, 16, [('dst', 16, 16, 0), ('src', 32, 16, 0)])]:
            code = f'(dex-ref-move {read("dst")} {read("src")})' if not width else w('dst', r('src', width), width)
            ins(group + suffix, opcode, units, [reg('dst', dbits), reg('src', sbits)], fields, code)
            opcode += 1
    for opcode, kind in [(0x0a, 'int'), (0x0b, 'wide'), (0x0c, 'object')]:
        ins('move-result' + ('' if kind == 'int' else '-' + kind), opcode, 1, [reg('dst', 8)], [('dst', 8, 8, 0)], f'(dex-result {kind} {read("dst")})', effects='runtime')
    ins('return-void', 0x0e, 1, [], [], '(dex-return void 0)', effects='control')
    for opcode, kind in [(0x0f, 'int'), (0x10, 'wide'), (0x11, 'object')]:
        ins('return' + ('' if kind == 'int' else '-' + kind), opcode, 1, [reg('src', 8)], [('src', 8, 8, 0)], f'(dex-return {kind} {read("src")})', effects='control')
    for name, opcode, units, dbits, bits, scale, pos in [
        ('const/4', 0x12, 1, 4, 4, 1, 12), ('const/16', 0x13, 2, 8, 16, 1, 16),
        ('const', 0x14, 3, 8, 32, 1, 16), ('const/high16', 0x15, 2, 8, 16, 1 << 16, 16),
        ('const-wide/16', 0x16, 2, 8, 16, 1, 16), ('const-wide/32', 0x17, 3, 8, 32, 1, 16),
        ('const-wide', 0x18, 5, 8, 64, 1, 16), ('const-wide/high16', 0x19, 2, 8, 16, 1 << 48, 16)]:
        width = 64 if 'wide' in name else 32
        ins(name, opcode, units, [reg('dst', dbits), operand('literal', bits, True, scale)],
            [('dst', 8, dbits, 0), ('literal', pos, bits, 0)], w('dst', read('literal'), width))
    for name, opcode, units, bits, pos in [('goto', 0x28, 1, 8, 8), ('goto/16', 0x29, 2, 16, 16), ('goto/32', 0x2a, 3, 32, 16)]:
        ins(name, opcode, units, [operand('disp', bits, True, 2, exclude=() if units == 3 else (0,))],
            [('disp', pos, bits, 0)], f'(jump (bvadd 64 (pc) {read("disp")}))', effects='control')
    for j, condition in enumerate(['eq', 'ne', 'lt', 'ge', 'gt', 'le']):
        for zero in [False, True]:
            operands = [reg('a', 8 if zero else 4)] + ([] if zero else [reg('b', 4)]) + [operand('disp', 16, True, 2, exclude=(0,))]
            fields = [('a', 8, 8 if zero else 4, 0)] + ([] if zero else [('b', 12, 4, 0)]) + [('disp', 16, 16, 0)]
            test = f'(dex-test {condition} {r("a")} ' + ('(const 32 0)' if zero else r('b')) + ')'
            ins('if-' + condition + ('z' if zero else ''), 0x32 + j + (6 if zero else 0), 2, operands, fields,
                f'(dex-branch {test} (bvadd 64 (pc) {read("disp")}) (next-pc))', effects='control')
    for j, (name, operation) in enumerate([('add', 'add'), ('sub', 'sub'), ('mul', 'mul'), ('div', 'div'), ('rem', 'rem'),
                                         ('and', 'and'), ('or', 'or'), ('xor', 'xor'), ('shl', 'shl'), ('shr', 'ashr'), ('ushr', 'lshr')]):
        for wide in [False, True]:
            width = 64 if wide else 32
            opcode = 0x90 + j + (11 if wide else 0)
            right = r('b', 32 if j >= 8 else width)
            if j >= 8:
                right = f'(dex-count {width} {right})'
            op = 'dex-' + operation if operation in ('div', 'rem') else 'bv' + operation
            for twoaddr in [False, True]:
                operands = [reg('dst', 4 if twoaddr else 8)] + ([] if twoaddr else [reg('a', 8)]) + [reg('b', 4 if twoaddr else 8)]
                fields = [('dst', 8, 4, 0), ('b', 12, 4, 0)] if twoaddr else [('dst', 8, 8, 0), ('a', 16, 8, 0), ('b', 24, 8, 0)]
                value = f'({op} {width} {r("dst" if twoaddr else "a", width)} {right})'
                ins(name + ('-long' if wide else '-int') + ('/2addr' if twoaddr else ''), opcode + (0x20 if twoaddr else 0), 1 if twoaddr else 2,
                    operands, fields, w('dst', value, width), effects='may-throw' if j in (3, 4) else 'pure')
    for name, opcode, operation, width in [('neg-int', 0x7b, 'neg', 32), ('not-int', 0x7c, 'not', 32), ('neg-long', 0x7d, 'neg', 64), ('not-long', 0x7e, 'not', 64)]:
        ins(name, opcode, 1, [reg('dst', 4), reg('src', 4)], [('dst', 8, 4, 0), ('src', 12, 4, 0)], w('dst', f'(dex-{operation} {width} {r("src", width)})', width))
    for name, opcode in [('virtual', 0x6e), ('super', 0x6f), ('direct', 0x70), ('static', 0x71), ('interface', 0x72)]:
        operands = [operand('count', 4, exclude=tuple(range(6, 16))), operand('method', 16)] + [reg(n, 4) for n in ['c', 'd', 'e', 'f', 'g']]
        fields = [('count', 12, 4, 0), ('method', 16, 16, 0), ('c', 32, 4, 0), ('d', 36, 4, 0), ('e', 40, 4, 0), ('f', 44, 4, 0), ('g', 8, 4, 0)]
        ins('invoke-' + name, opcode, 3, operands, fields,
            '(dex-invoke ' + name + ' ' + ' '.join(read(n) for n in ['method', 'count', 'c', 'd', 'e', 'f', 'g']) + ')', effects='runtime')
        ins('invoke-' + name + '/range', opcode + 6, 3,
            [operand('count', 8), operand('method', 16), reg('first', 16)], [('count', 8, 8, 0), ('method', 16, 16, 0), ('first', 32, 16, 0)],
            f'(dex-invoke-range {name} {read("method")} {read("count")} {read("first")})', effects='runtime')
    return p
