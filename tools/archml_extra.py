"""Reviewed, deliberately bounded profiles from the local ArchTk infobank.

Imported by generate_archml.py; no source corpus or network needed to regenerate.
Bit positions in the tables are architectural LSB indices, converted to wire order.
"""
from generate_archml import Profile, prop, q, ref, refs, operand, read, write, bv, parallel


def op(p, name, arity, semantics):
    p.add('pcode-op', name, prop('arity', f'{arity} {arity}'), prop('semantics', q(semantics)))


def base(name, bits, endian, sources, scope):
    p = Profile(name)
    p.add('target', name, prop('bits', bits), prop('endian', endian),
          prop('default-features', ref('feature', 'base')), prop('sources', q(sources)),
          prop('semantics', q('Operands read pre-state. Effects commit after fault checks; normal fallthrough advances PC by encoded length. Widths are bits; offsets are bytes.')))
    p.add('coverage', 'profile', prop('status', 'selected-concrete-forms'), prop('contract', q(scope)),
          prop('non-goals', q('Not a complete ISA, ABI lowering implementation, scheduler or emulator. No calling convention, privileged state, relocation or extension coverage is implied.')))
    p.feature('base', scope)
    p.add('mode', 'user', prop('bits', bits), prop('scope', q(scope)))
    p.add('schedule', 'alu', prop('model', 'unspecified'), prop('rules', q('No measured latency or throughput claims.')))
    p.add('memory', 'normal', prop('endian', endian), prop('address-bits', bits),
          prop('rules', q('Only the explicitly encoded memory forms are supported; mapping, access checks and execution environment remain consumer responsibilities.')))
    p.add('layout', 'code', prop('alignment', 1 if name == 'wasm' else 2 if name == 'avr32' else 4))
    p.add('syntax', 'assembly', prop('rules', q('Assembly properties are descriptive templates, not a text assembler.')))
    for n, a, s in [
        ('read', 1, 'Read the named operand; zero-register reads are zero. Signed immediates extend to operation width.'),
        ('write', 2, 'Write operand with truncation to its register width and declared write policy.'),
        ('reg', 1, 'Read named implicit architectural register.'),
        ('set-reg', 2, 'Write named implicit architectural register.'),
        ('parallel', 2, 'Evaluate both effects in pre-state and commit together after fault checks.'),
        ('const', 2, 'Arguments width,value: bitvector value modulo 2^width.'),
        ('pc', 0, 'Current instruction address.'),
        ('jump', 1, 'Set PC to target; enforce architectural alignment and access checks.'),
        ('nop', 0, 'No state change except normal fallthrough.'),
        ('trap', 1, 'Raise named synchronous exception without normal fallthrough.'),
        ('load', 4, 'Arguments width,address,endian,alignment: checked memory read.'),
        ('store', 5, 'Arguments width,address,value,endian,alignment: checked memory write of low width bits.'),
        ('sext', 3, 'Arguments source-width,destination-width,value: sign extend.'),
        ('zext', 3, 'Arguments source-width,destination-width,value: zero extend.'),
        ('eq', 2, 'Bitvector equality.'), ('slt', 2, 'Signed less than.'),
        ('ult', 2, 'Unsigned less than.'), ('bool-int', 2, 'Arguments width,predicate: zero or one bitvector.'),
    ]:
        op(p, n, a, s)
    for n, s in [('add', 'modular sum'), ('sub', 'modular difference'), ('mul', 'low modular product'),
                 ('and', 'bitwise AND'), ('or', 'bitwise OR'), ('xor', 'bitwise XOR'),
                 ('shl', 'logical left shift'), ('lshr', 'logical right shift'), ('ashr', 'arithmetic right shift')]:
        op(p, 'bv'+n, 3, f'Arguments width,a,b: {s}; reduce inputs/result modulo 2^width. Shifts use the explicitly supplied count.')
    return p


def registers(p, count, bits, zero=False):
    for i in range(count):
        p.storage(f'r{i}', bits)
        p.reg(f'r{i}', bits, f'r{i}', i, write='discard' if zero and i == 0 else 'replace')
    p.regclass('GPR', [f'r{i}' for i in range(count)], bits)


def fields_be(fields, length=4):
    # Split at byte boundaries: wire bit zero is not the architectural word LSB.
    out = []
    for name, dst, width, src in fields:
        for j in range(width):
            b = dst + j
            out.append((name, (length - 1 - b//8)*8 + b%8, 1, src+j))
    return out


def ins(p, name, word, operands, fields, code, asm, length=4, **extra):
    p.instruction(name, word.to_bytes(length, 'big').hex(), operands,
                  fields_be(fields, length), code, asm, **extra)


def ppc64():
    p = base('ppc64', 64, 'big',
             'ArchTk/infobank/Capstone-Arch/PowerPC/PPCGenDisassemblerTables.inc; LLVM-Target/PowerPC/PPCInstrInfo.td, PPCInstr64Bit.td, PPCInstrFormats.td.',
             'Big-endian 64-bit PowerPC, selected non-recording integer forms (Rc=0, OE=0). No ELFv1/ELFv2 ABI assumption; no VMX/VSX, FP, prefixed or privileged encodings.')
    registers(p, 32, 64)
    for n, w in [('pc', 64), ('lr', 64), ('ctr', 64), ('xer', 64), ('cr', 32)]:
        p.storage(n, w); p.reg(n, w, n, 0)
    op(p, 'ppc-base', 1, 'Read named base operand, except encoding r0 denotes constant zero here (not for ordinary arithmetic/logical operands).')
    rd = operand('rd', 5, cls='GPR', access='write')
    ra = operand('ra', 5, cls='GPR'); rb = operand('rb', 5, cls='GPR')
    for n, xo, operation in [('add', 266, 'add'), ('subf', 40, 'sub'), ('mulld', 233, 'mul')]:
        a, b = (read('rb'), read('ra')) if n == 'subf' else (read('ra'), read('rb'))
        ins(p, n, (31 << 26) | (xo << 1), [rd, ra, rb], [('rd',21,5,0),('ra',16,5,0),('rb',11,5,0)],
            write('rd', bv(operation,64,a,b)), n+' {rd}, {ra}, {rb}')
    for n, xo, operation in [('and',28,'and'),('or',444,'or'),('xor',316,'xor')]:
        ins(p,n,(31<<26)|(xo<<1),[rd,ra,rb],[('rd',16,5,0),('ra',21,5,0),('rb',11,5,0)],
            write('rd',bv(operation,64,read('ra'),read('rb'))),n+' {rd}, {ra}, {rb}')
    for n, opcode, shifted in [('addi',14,False),('addis',15,True),('ori',24,False),('oris',25,True),('xori',26,False),('xoris',27,True)]:
        logical = n.startswith(('or','xor'))
        value = read('imm')
        if shifted: value = bv('shl',64,value,'(const 64 16)')
        source = read('ra') if logical else '(ppc-base ra)'
        fs = [('rd',16 if logical else 21,5,0),('ra',21 if logical else 16,5,0),('imm',0,16,0)]
        ins(p,n,opcode<<26,[rd,ra,operand('imm',16,not logical)],fs,
            write('rd',bv('xor' if n.startswith('xor') else 'or' if logical else 'add',64,source,value)),n+' {rd}, {ra}, {imm}')
    for n, opcode, width, store, ds in [('lbz',34,8,False,False),('lhz',40,16,False,False),('lwz',32,32,False,False),
                                       ('ld',58,64,False,True),('stb',38,8,True,False),('sth',44,16,True,False),('stw',36,32,True,False),('std',62,64,True,True)]:
        addr = bv('add',64,'(ppc-base ra)',read('disp'))
        value = f'(load {width} {addr} big {width//8})'
        if width < 64: value = f'(zext {width} 64 {value})'
        code = f'(store {width} {addr} {read("rd")} big {width//8})' if store else write('rd',value)
        ins(p,n,opcode<<26,[operand('rd',5,cls='GPR',access='read' if store else 'write'),ra,operand('disp',14 if ds else 16,True,4 if ds else 1)],
            [('rd',21,5,0),('ra',16,5,0),('disp',2 if ds else 0,14 if ds else 16,0)],code,n+' {rd}, {disp}({ra})',effects='memory')
    for n, lk in [('b',0),('bl',1)]:
        code = f'(jump {bv("add",64,"(pc)",read("disp"))})'
        if lk: code = parallel('(set-reg lr (bvadd 64 (pc) (const 64 4)))',code)
        ins(p,n,(18<<26)|lk,[operand('disp',24,True,4)],[('disp',2,24,0)],code,n+' {target}',effects='control',
            implicit_def=refs('register',['lr']) if lk else '')
    ins(p,'nop',0x60000000,[],[],'(nop)','nop')
    p.add('exception','memory',prop('rules',q('Access/alignment faults must be checked before writes. Non-recording arithmetic leaves CR and XER unchanged.')))
    return p


def mips():
    p = base('mips',32,'big',
             'ArchTk/infobank/Capstone-Arch/Mips/MipsGenDisassemblerTables.inc; LLVM-Target/Mips/MipsInstrInfo.td and MipsInstrFormats.td.',
             'Big-endian MIPS32 pre-Release-6 integer subset, fixed 32-bit instructions. Delayed BEQ/BNE/JR are explicit; no MIPS16, microMIPS, FP, privileged encodings or ABI claim.')
    registers(p,32,32,zero=True)
    aliases = ['zero','at','v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','s0','s1','s2','s3','s4','s5','s6','s7','t8','t9','k0','k1','gp','sp','fp','ra']
    for i,n in enumerate(aliases): p.reg(n,32,f'r{i}',i,write='discard' if i==0 else 'replace')
    for n in ['pc','hi','lo']:
        p.storage(n,32); p.reg(n,32,n,0)
    op(p,'mips-delayed-branch',2,'Arguments predicate,target: execute the instruction at PC+4 unconditionally, then select target if predicate else PC+8. Target is computed from branch pre-state. A control transfer in the delay slot is unsupported/UNPREDICTABLE. Delay-slot faults report branch EPC with BD=1; no ordinary immediate jump semantics.')
    rd=operand('rd',5,cls='GPR',access='write'); rs=operand('rs',5,cls='GPR'); rt=operand('rt',5,cls='GPR')
    for n,funct,operation in [('addu',0x21,'add'),('subu',0x23,'sub'),('and',0x24,'and'),('or',0x25,'or'),('xor',0x26,'xor'),('slt',0x2a,None),('sltu',0x2b,None)]:
        val=bv(operation,32,read('rs'),read('rt')) if operation else f'(bool-int 32 ({"slt" if n=="slt" else "ult"} (read rs) (read rt)))'
        ins(p,n,funct,[rd,rs,rt],[('rd',11,5,0),('rs',21,5,0),('rt',16,5,0)],write('rd',val),n+' {rd}, {rs}, {rt}')
    for n,funct,operation in [('sll',0,'shl'),('srl',2,'lshr'),('sra',3,'ashr')]:
        ins(p,n,funct,[rd,rt,operand('shamt',5)],[('rd',11,5,0),('rt',16,5,0),('shamt',6,5,0)],write('rd',bv(operation,32,read('rt'),read('shamt'))),n+' {rd}, {rt}, {shamt}')
    for n,opcode,operation,sign in [('addiu',9,'add',True),('andi',12,'and',False),('ori',13,'or',False),('xori',14,'xor',False)]:
        ins(p,n,opcode<<26,[rd,rs,operand('imm',16,sign)],[('rd',16,5,0),('rs',21,5,0),('imm',0,16,0)],write('rd',bv(operation,32,read('rs'),read('imm'))),n+' {rd}, {rs}, {imm}')
    ins(p,'lui',15<<26,[rd,operand('imm',16)],[('rd',16,5,0),('imm',0,16,0)],write('rd',bv('shl',32,read('imm'),'(const 32 16)')),'lui {rd}, {imm}')
    for n,opcode,w,sign,store in [('lb',32,8,True,False),('lbu',36,8,False,False),('lh',33,16,True,False),('lhu',37,16,False,False),('lw',35,32,False,False),('sb',40,8,False,True),('sh',41,16,False,True),('sw',43,32,False,True)]:
        addr=bv('add',32,read('rs'),read('disp')); val=f'(load {w} {addr} big {w//8})'
        if w<32: val=f'({"sext" if sign else "zext"} {w} 32 {val})'
        code=f'(store {w} {addr} (read rd) big {w//8})' if store else write('rd',val)
        ins(p,n,opcode<<26,[operand('rd',5,cls='GPR',access='read' if store else 'write'),rs,operand('disp',16,True)],
            [('rd',16,5,0),('rs',21,5,0),('disp',0,16,0)],code,n+' {rd}, {disp}({rs})',effects='memory')
    for n,opcode in [('beq',4),('bne',5)]:
        pred='(eq (read rs) (read rt))' if n=='beq' else '(eq (bool-int 32 (eq (read rs) (read rt))) (const 32 0))'
        ins(p,n,opcode<<26,[rs,rt,operand('disp',16,True,4)],[('rs',21,5,0),('rt',16,5,0),('disp',0,16,0)],
            f'(mips-delayed-branch {pred} (bvadd 32 (bvadd 32 (pc) (const 32 4)) (read disp)))',n+' {rs}, {rt}, {target}',effects='control delay-slot')
    ins(p,'jr',8,[rs],[('rs',21,5,0)],'(mips-delayed-branch (const 1 1) (read rs))','jr {rs}',effects='control delay-slot')
    for n,word,code in [('nop',0,'(nop)'),('syscall0',12,'(trap syscall)'),('break0',13,'(trap breakpoint)')]:
        ins(p,n,word,[],[],code,n)
    p.add('exception','precise',prop('rules',q('Natural alignment required for halfword/word memory forms. ADDU/SUBU/ADDIU wrap without overflow trap. Delay-slot exceptions retain branch EPC and BD state.')))
    return p


def avr32():
    p=base('avr32',32,'big',
           'ArchTk/infobank/Ghidra-CPU/Atmel/data/languages/avr32a.slaspec, avr32a_logic_operations.sinc, avr32a_data_transfer.sinc, avr32a_arithmetic_operations.sinc, avr32a_system_control.sinc.',
           'AVR32A, not 8-bit AVR: selected 16-bit register and immediate forms. PC operands are excluded to avoid implicit PC-read/branch variants. No DSP, SIMD, coprocessor, memory or privileged forms.')
    registers(p,16,32)
    for n,i in [('sp',13),('lr',14),('pc',15)]: p.reg(n,32,f'r{i}',i)
    p.storage('sr',32); p.reg('sr',32,'sr',0)
    op(p,'avr-nz',1,'Update SR.N (bit2) from result bit31 and SR.Z (bit1) from result==0; preserve all other SR bits.')
    op(p,'avr-add-flags',2,'Arguments a,b: compute r=(a+b) modulo 2^32; SR.C bit0=unsigned carry, Z bit1=r==0, N bit2=msb(r), V bit3=signed overflow. Preserve other SR bits.')
    rd=operand('rd',4,cls='GPR',access='read-write',exclude=(15,)); rs=operand('rs',4,cls='GPR',exclude=(15,))
    for n,word,operation in [('add',0,'add'),('and',0x60,'and'),('or',0x40,'or'),('eor',0x50,'xor')]:
        value=bv(operation,32,read('rd'),read('rs'))
        flags='(avr-add-flags (read rd) (read rs))' if n=='add' else f'(avr-nz {value})'
        ins(p,n,word,[rd,rs],[('rd',0,4,0),('rs',9,4,0)],parallel(write('rd',value),flags),n+' {rd}, {rs}',length=2,
            implicit_use=refs('register',['sr']),implicit_def=refs('register',['sr']))
    dest=operand('rd',4,cls='GPR',access='write',exclude=(15,))
    ins(p,'mov-rr',0x90,[dest,rs],[('rd',0,4,0),('rs',9,4,0)],write('rd',read('rs')),'mov {rd}, {rs}',length=2)
    ins(p,'mov-ri',0x3000,[dest,operand('imm',8,True)],[('rd',0,4,0),('imm',4,8,0)],write('rd',read('imm')),'mov {rd}, {imm}',length=2)
    value=bv('xor',32,read('rd'),'(const 32 0xffffffff)')
    ins(p,'com',0x5cd0,[rd],[('rd',0,4,0)],parallel(write('rd',value),f'(avr-nz {value})'),'com {rd}',length=2,
        implicit_use=refs('register',['sr']),implicit_def=refs('register',['sr']))
    ins(p,'nop',0xd703,[],[],'(nop)','nop',length=2)
    ins(p,'breakpoint',0xd673,[],[],'(trap breakpoint)','breakpoint',length=2)
    p.add('exception','breakpoint',prop('rules',q('Breakpoint transfers to the execution environment debug/exception handler; no normal fallthrough.')))
    return p


def wasm():
    p=base('wasm',32,'little',
           'ArchTk/infobank/Capstone-Arch/WASM/WASMDisassembler.c opcode table; Binaryen-WASM/wasm-stack.cpp and wasm-validator.cpp for stack typing and binary emission.',
           'WebAssembly MVP wasm32 stack machine. Selected no-immediate integer operations and canonical single-byte LEB constants only. Not a register ISA. No general LEB128, structured control, module serialization, SIMD or threads encoder.')
    op(p,'wasm-bin',2,'Arguments operation,width: pop b then a from typed operand stack, push width-bit modular a op b. add/sub/mul/and/or/xor wrap; shl/shr_s/shr_u mask b by width-1; rotates mask count and rotate width bits. Validation requires two operands of the stated type; stack underflow/type mismatch is invalid code, not a runtime trap.')
    op(p,'wasm-unary',2,'Arguments operation,width: pop one width-bit integer, push clz/ctz/popcnt at same width, or eqz as i32 0/1. clz(0)=ctz(0)=width.')
    op(p,'wasm-const',2,'Arguments width,value: push integer bitvector modulo 2^width onto operand stack.')
    op(p,'wasm-drop',0,'Pop the top typed value; requires a valid nonempty operand stack.')
    op(p,'wasm-select',0,'Pop i32 condition c, then value b, then same-typed value a; push a if c!=0 else b. MVP numeric value types only.')
    for width,start in [(32,0x67),(64,0x79)]:
        for i,n in enumerate(['clz','ctz','popcnt']):
            p.instruction(f'i{width}.{n}',f'{start+i:02x}',[],[],f'(wasm-unary {n} {width})',f'i{width}.{n}',effects='operand-stack')
        for offset,n in [(3,'add'),(4,'sub'),(5,'mul'),(10,'and'),(11,'or'),(12,'xor'),(13,'shl'),(14,'shr_s'),(15,'shr_u'),(16,'rotl'),(17,'rotr')]:
            p.instruction(f'i{width}.{n}',f'{start+offset:02x}',[],[],f'(wasm-bin {n} {width})',f'i{width}.{n}',effects='operand-stack')
        opcode=0x41 if width==32 else 0x42
        p.instruction(f'i{width}.const-sleb7',f'{opcode:02x}00',[operand('value',7,True)],[('value',8,7,0)],
                      f'(wasm-const {width} (read value))',f'i{width}.const {{value}}',effects='operand-stack',
                      constraints='Only canonical one-byte signed LEB128 values -64..63; continuation bit fixed zero.')
        p.instruction(f'i{width}.eqz', '45' if width==32 else '50',[],[],f'(wasm-unary eqz {width})',f'i{width}.eqz',effects='operand-stack')
    for n,byte,code in [('unreachable','00','(trap unreachable)'),('nop','01','(nop)'),('drop','1a','(wasm-drop)'),('select','1b','(wasm-select)')]:
        p.instruction(n,byte,[],[],code,n,effects='control' if n=='unreachable' else 'operand-stack')
    p.add('exception','trap',prop('rules',q('unreachable always traps. Validation and module/function context are required separately; an encodable instruction stream is not necessarily a valid WebAssembly module.')))
    return p


def profiles():
    return [ppc64,mips,avr32,wasm]
