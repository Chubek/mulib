"""Check the first bounded v3 marshalling subset, without authorizing emission.

This build-tool API consumes ffic's LLSynth trees and checked discovery
records. W06 must supply canonical nominal shapes; they are never defined by
an override. A checked plan is not a descriptor and has no runtime ingress.
"""
from __future__ import annotations

from typing import NamedTuple

import ffic
from ffic import (DIGEST, FRAMED_IPC_SYMBOLS, LEGACY_TARGET, MAX_ENTRIES, MAX_PARAMS,
                  SURFACE, ident, lexeme, module_name, nodes, parse_root, reason,
                  require, scalar_mapping, string, symbol_allowed, wire_uint)

MAX_OPS = 64
MAX_LOCALS = 32
MAX_CAPACITY = 256
INT = ['integer', 'int', 3, 32, 'signed', 4, 4]
DOUBLE = ['real', 'double', 'binary64', 64, 8, 8]
USIZE = ['integer', 'unsigned-long', 4, 64, 'unsigned', 8, 8]

# Operand sorts and arity are part of the wire contract. The validator below
# separately checks the value type, dominance and exact ABI bindings.
OPS = {
    'check_i64': ('arg', 'int', 'int'),
    'check_f64': ('arg', 'word'),
    'snapshot': ('arg', 'local', 'int', 'int'),
    'same_length': ('local', 'local'),
    'slice_domain': ('local', 'word'),
    'scalar_arg': ('arg', 'native'),
    'slice_data': ('local', 'native'),
    'slice_length': ('local', 'native'),
    'out_slot': ('local', 'native'),
    'init_field': ('local', 'field', 'word'),
    'call': ('local',),
    'status_eq': ('local', 'int'),
    'out_field': ('local', 'field', 'local'),
    'check_field': ('local', 'word'),
    'ok_record': None,  # one local per canonical result field, in that order
    'reject': ('word',),  # error continuation for every rejection/status failure
}


class Parameter(NamedTuple):
    name: str
    mode: str
    shape: tuple


class Candidate(NamedTuple):
    fingerprint: str
    parameters: tuple[Parameter, ...]
    result: tuple
    operations: tuple


class RichOverrides(NamedTuple):
    target: tuple[str, str, str, str]
    module: str
    selections: tuple[str, ...]
    obligations: tuple[tuple[str, str | None], ...]
    plans: dict[str, Candidate]


class NominalShape(NamedTuple):
    """A consumer-owned record, sum constructor, or empty-payload error enum."""
    kind: str
    fields: tuple[tuple[str, str], ...] = ()
    cases: tuple[str, ...] = ()


class VerifiedPlan(NamedTuple):
    candidate: Candidate
    sha256: str


def surface(node, module, depth=1):
    require(depth <= 3, 'surface shape nesting limit exceeded')
    kind = lexeme(node.children[0])
    args = node.children[1].children
    if kind in SURFACE:
        require(not args and kind != 'void', 'invalid scalar surface shape')
        return (kind,)
    if kind == 'slice':
        require(len(args) == 5 and args[1].symbol == 'Word' and lexeme(args[1]) == 'f64',
                'only binary64 immutable snapshot slices are supported in the v3 plan subset')
        return (kind, 'f64', wire_uint(args[3], MAX_CAPACITY))
    require((kind == 'nominal' and len(args) == 3) or (kind == 'sum' and len(args) == 7),
            'unsupported rich surface type')
    require(args[1].symbol == 'QualifiedName', 'expected qualified nominal identity')
    p = args[1].children
    owner = lexeme(p[0]) + '.' + lexeme(p[2])
    require(owner == module, 'surface nominal belongs to a different module')
    name = owner + '.' + ident(lexeme(p[4]))
    if kind == 'nominal':
        return (kind, name)
    return (kind, name, surface(args[3], module, depth+1), surface(args[5], module, depth+1))


def operand(node):
    value = node.children[0]
    if value.symbol == 'SignedNumber':
        number = wire_uint(value.children[-1], 1 << 63)
        if len(value.children) == 2:
            require(number != 0, 'noncanonical negative zero')
            return ('int', -number)
        require(number < 1 << 63, 'marshalling literal outside i64')
        return ('int', number)
    word = ident(lexeme(value))
    index = node.children[1].children
    if not index:
        return ('word', word)
    limits = dict(arg=MAX_PARAMS, local=MAX_LOCALS, native=MAX_PARAMS, field=ffic.MAX_TAG_ITEMS)
    require(word in limits, 'unknown marshalling slot sort')
    return word, wire_uint(index[1], limits[word]-1)


def candidate(node, module):
    p = node.children
    fingerprint = string(lexeme(p[5]))
    require(DIGEST.fullmatch(fingerprint), 'invalid native graph fingerprint')
    require(lexeme(p[13]) == 'unknown', 'unsupported marshalling effect')
    parameters = []
    for n in nodes(p[8], 'SurfaceParam'):
        mode, name = (lexeme(n.children[i]) for i in (0, 1))
        shape = surface(n.children[3], module)
        require(len(parameters) < MAX_PARAMS and name not in {a.name for a in parameters},
                'surface parameter budget or duplicate')
        require((mode == 'value' and shape[0] in SURFACE) or (mode == 'snapshot' and shape[0] == 'slice'),
                'unsupported surface passing mode or argument shape')
        parameters.append(Parameter(ident(name), mode, shape))
    result = surface(p[11], module)
    require(result[0] == 'sum' and result[2][0] == 'nominal' and result[3][0] == 'nominal',
            'v3 plan subset requires nominal Result<Record,Error>')
    ops = []
    for n in nodes(p[16], 'PlanOp'):
        require(len(ops) < MAX_OPS, 'marshalling operation limit exceeded')
        name = lexeme(n.children[0])
        require(name in OPS, f'unsupported marshalling operation: {name}')
        args = []
        for raw in nodes(n.children[2], 'Operand'):
            require(len(args) < MAX_LOCALS, 'marshalling operand limit exceeded')
            args.append(operand(raw))
        signature = tuple(k for k, _ in args)
        require(signature == OPS[name] if OPS[name] is not None else
                bool(signature) and all(t == 'local' for t in signature),
                f'invalid marshalling operands: {name}')
        ops.append((name, tuple(v for _, v in args)))
    require(ops, 'empty marshalling plan')
    return Candidate(fingerprint, tuple(parameters), result, tuple(ops))


def parse_rich_overrides(parser, raw, path):
    """Parse authored v3 plans and non-authorizing module audit obligations."""
    version, target, body = parse_root(parser, raw, path)
    require(version == 3 and body.symbol == 'Extension', 'expected a version 3 extend document')
    module = module_name(body.children)
    selections, plans, obligations = set(), {}, {}
    for i, wrap in enumerate(nodes(body.children[5], 'OverrideItem')):
        require(i < MAX_ENTRIES, 'override entry limit exceeded')
        item = wrap.children[0]
        p = item.children
        if item.symbol == 'Obligation':
            code = reason(p[1])
            require(code not in obligations, 'duplicate discovery obligation')
            obligations[code] = ffic.audit_text(p[2])
            continue
        require(item.symbol in ('Selection', 'Adapt'), 'scalar guards use a version 2 override')
        symbol = ident(lexeme(p[3]))
        require(symbol_allowed(module, symbol) or
                (module == 'mulib.ipc_framed' and target == LEGACY_TARGET and symbol in FRAMED_IPC_SYMBOLS),
                'C symbol outside canonical declaration source')
        registry = selections if item.symbol == 'Selection' else plans
        require(symbol not in registry, 'duplicate rich selection or plan')
        if item.symbol == 'Selection':
            selections.add(symbol)
        else:
            plans[symbol] = candidate(item, module)
    require(plans.keys() <= selections, 'marshalling plan requires explicit ownership selection')
    return RichOverrides(target, module, tuple(sorted(selections)), tuple(sorted(obligations.items())), plans)


def verify_plan(overrides, module, symbol, nominals):
    """Check selected plan against one parsed v3 graph and known nominal types.

    Return the immutable candidate and its non-authorizing plan digest.
    The absence of an execution path is intentional until HIR, emitter and
    native planner support it.
    """
    require(module['version'] == 3 and overrides.module == module['module']
            and overrides.target == module['target'], 'marshalling module/target mismatch')
    require(symbol in overrides.selections and symbol in overrides.plans, 'missing selected marshalling plan')
    entries = [e for e in module['entries'] if e['symbol'] == symbol]
    require(len(entries) == 1, 'missing or duplicate projected marshalling symbol')
    entry = entries[0]
    plan = overrides.plans[symbol]
    require(entry['native_fingerprint'] == plan.fingerprint, 'stale native graph fingerprint')
    require(entry['declaration_source'][0] == 'public' and entry['linkage'] == 'external'
            and entry['definition'] in ('declaration', 'defined') and not entry['specifiers'],
            'unsupported native placement or declaration source for v3 marshalling subset')
    root = entry['native_type']
    require(root[:3] == ['function-type', True, False] and root[3] == INT,
            'marshalling subset requires fixed prototype and exact C int status')
    native = root[4]
    graph, = (g for g in module['native_graphs'] if g['id'] == entry['graph'])
    tags = {t['id']: t for t in graph['tags']}
    identities = (plan.result[1], plan.result[2][1], plan.result[3][1])
    result, success, error = (nominals.get(id_) for id_ in identities)
    require(result and result.kind == 'result' and success and success.kind == 'record'
            and error and error.kind == 'empty_enum', 'unknown or incompatible canonical nominal result')
    require(0 < len(success.fields) <= MAX_LOCALS and all(s in SURFACE and s != 'void' for _, s in success.fields),
            'unsupported canonical success fields')

    bound, locals_, used, snapshots = set(), {}, set(), set()
    initialized, output_slots = set(), set()
    nonnegative = set()
    status = None
    phase = 'prepare'
    for op, a in plan.operations:
        def surface_arg(i, type_):
            require(i < len(plan.parameters) and plan.parameters[i].shape[0] == type_, 'wrong marshalling argument kind')
            return plan.parameters[i].shape

        def arg(i):
            require(i < len(plan.parameters), 'marshalling argument out of bounds')
            return plan.parameters[i].shape[0]

        def native_arg(i, expected=None):
            require(i < len(native) and i not in bound, 'native slot out of bounds or already assigned')
            require(expected is None or native[i] == expected, 'native slot type mismatch')
            bound.add(i)
            return native[i]

        def assign(i, kind, shape=None):
            require(i not in locals_, 'marshalling local reassigned')
            locals_[i] = kind, shape

        def get(i, kind):
            require(i in locals_ and locals_[i][0] == kind, 'unassigned or wrong marshalling local type')
            return locals_[i][1]

        if phase == 'prepare':
            if op == 'check_i64':
                i, lo, hi = a
                surface_arg(i, 'i64')
                require(lo <= hi, 'inverted integer domain')
                if lo >= 0:
                    nonnegative.add(i)
            elif op == 'check_f64':
                i, domain = a
                surface_arg(i, 'f64')
                require(domain in ('zero_to_one', 'positive_to_one'), 'unknown floating-point argument domain')
            elif op == 'snapshot':
                i, slot, minimum, capacity = a
                _, _, actual = surface_arg(i, 'slice')
                require(0 <= minimum <= capacity == actual and i not in snapshots, 'invalid or duplicate snapshot bound')
                assign(slot, 'slice', i)
                snapshots.add(i)
            elif op == 'same_length':
                get(a[0], 'slice')
                get(a[1], 'slice')
                require(a[0] != a[1], 'length relationship needs two windows')
            elif op == 'slice_domain':
                get(a[0], 'slice')
                require(a[1] == 'finite_nonnegative', 'unknown snapshot domain')
            elif op == 'scalar_arg':
                i, j = a
                surface_kind, native_kind = arg(i), native_arg(j)
                require(surface_kind in SURFACE and
                        (scalar_mapping(surface_kind, native_kind)
                         or (surface_kind == 'i64' and i in nonnegative and native_kind == USIZE)),
                        'native scalar mapping mismatch or missing prior nonnegative check')
                used.add(i)
            elif op in ('slice_data', 'slice_length'):
                slot, j = a
                i = get(slot, 'slice')
                native_arg(j, ['pointer', 64, 8, 8, ['qualified', ['const'], DOUBLE]] if op == 'slice_data' else USIZE)
                used.add(i)
            elif op == 'out_slot':
                slot, j = a
                t = native_arg(j)
                require(t[0] == 'pointer' and t[4][0] == 'tag-ref', 'out slot needs mutable record pointer')
                tag = tags[t[4][1]]
                require(tag['kind'] == 'struct' and tag['complete'] and tag['name']
                        and all(f['name'] and f['bit_field'] is None and not f['flexible_array']
                                and f['type'][0] in ('integer', 'real') for f in tag['fields']),
                        'unsupported output record shape')
                assign(slot, 'out', tag['fields'])
                output_slots.add(slot)
            elif op == 'init_field':
                slot, field, value = a
                fs = get(slot, 'out')
                require(field < len(fs) and (slot, field) not in initialized, 'unknown or already initialized output field')
                require(value == 'zero' or (value == 'nan' and fs[field]['type'] == DOUBLE),
                        'unsupported output field initializer')
                initialized.add((slot, field))
            elif op == 'call':
                require(len(bound) == len(native) and used == set(range(len(plan.parameters))),
                        'unbound native or unused surface argument')
                require(output_slots and all((slot, i) in initialized for slot in output_slots
                                             for i in range(len(get(slot, 'out')))), 'uninitialized output field')
                status, = a
                assign(status, 'status')
                phase = 'called'
            else:
                raise ffic.Invalid('marshalling operation before call: ' + op)
        elif phase == 'called':
            require(op == 'status_eq' and a[0] == status and -(1 << 31) <= a[1] < 1 << 31,
                    'call must be immediately followed by exact int status branch')
            phase = 'success'
        elif phase == 'success':
            if op == 'out_field':
                slot, field, value = a
                fs = get(slot, 'out')
                require((slot, field) in initialized, 'output read requires initialized field')
                assign(value, 'value', fs[field]['type'])
            elif op == 'check_field':
                slot, domain = a
                t = get(slot, 'value')
                require((t == DOUBLE and domain in ('finite', 'finite_nonnegative', 'is_nan'))
                        or (t[0] == 'integer' and domain == 'positive'), 'unsupported output field domain')
            elif op == 'ok_record':
                require(len(a) == len(success.fields) and
                        all(scalar_mapping(s, get(v, 'value')) for v, (_, s) in zip(a, success.fields)),
                        'output values do not match canonical record fields')
                phase = 'error'
            else:
                raise ffic.Invalid('marshalling operation outside allowed success continuation: ' + op)
        elif phase == 'error':
            require(op == 'reject' and a[0] in error.cases, 'missing canonical error continuation')
            phase = 'done'
        else:
            raise ffic.Invalid('operation after marshalling terminal')
    require(phase == 'done', 'incomplete marshalling control flow')
    digest = ffic.sha(ffic.compact([
        'on1x.ffi-marshalling-plan/v3', list(module['target']), module['module'], symbol,
        plan.fingerprint, 'unknown', [(p.mode, p.shape) for p in plan.parameters],
        plan.result, [result, success, error], plan.operations,
    ]))
    return VerifiedPlan(plan, digest)
