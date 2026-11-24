#!/usr/bin/env python3
"""Check v3 numerical plan typing, dominance, and fail-closed publication."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import ffic
import ffi_plan
from test_rich import TARGET, INT, REAL

MODULE = 'mulib.histprice'
SYMBOL = 'mulib_histprice_ema'
NOMINALS = {
    MODULE + '.Result': ffi_plan.NominalShape('result'),
    MODULE + '.Forecast': ffi_plan.NominalShape('record', (('point', 'f64'), ('variance', 'f64'))),
    MODULE + '.Error': ffi_plan.NominalShape('empty_enum', cases=('Rejected',)),
}


def plan_text(target, symbol, fingerprint):
    # A verifier control only. No production override or authority.
    return f'''ffi 3; {target}
extend {MODULE} {{
    select c::{symbol};
    obligation caller_native_contract audit "Successful initialization requires review.";
    adapt c::{symbol} fingerprint "{fingerprint}"
        surface (snapshot values:slice(f64,256), value alpha:f64, value horizon:i64)
        -> sum({MODULE}.Result,nominal({MODULE}.Forecast),nominal({MODULE}.Error))
        effects unknown plan {{
            snapshot(arg(0),local(0),1,256);
            slice_domain(local(0),finite_nonnegative);
            check_f64(arg(1),zero_to_one);
            check_i64(arg(2),1,1000000);
            slice_data(local(0),native(0));
            slice_length(local(0),native(1));
            scalar_arg(arg(1),native(2));
            scalar_arg(arg(2),native(3));
            out_slot(local(1),native(4));
            init_field(local(1),field(0),nan);
            init_field(local(1),field(1),nan);
            call(local(2));
            status_eq(local(2),1);
            out_field(local(1),field(0),local(3));
            check_field(local(3),finite_nonnegative);
            out_field(local(1),field(1),local(4));
            check_field(local(4),is_nan);
            ok_record(local(3),local(4));
            reject(Rejected);
        }}
}}'''


def verify(parser, module, target_line):
    entry = next(e for e in module['entries'] if e['symbol'] == SYMBOL)
    original = plan_text(target_line, SYMBOL, entry['native_fingerprint'])
    rich = ffi_plan.parse_rich_overrides(parser, original.encode(), 'authored.ffi')
    checked = ffi_plan.verify_plan(rich, module, SYMBOL, NOMINALS)
    assert checked.candidate == rich.plans[SYMBOL] and ffic.DIGEST.fullmatch(checked.sha256)
    return original


def run(parser):
    ulong = 'integer(unsigned-long,4,64,unsigned,8,8)'
    graph = f'''ffi 3; {TARGET}
foreign {MODULE} {{
    provider mulib "histprice"; header "mulib-histprice.h";
    graph 0 {{
        tag 0 struct "forecast" complete (16,8) {{
            field "point" at 0 : {REAL};
            field "variance" at 8 : {REAL};
        }}
    }}
    discover ema = c::{SYMBOL} native external declaration graph 0
        function-type(fixed,(pointer(64,8,8,qualified(const,{REAL})),{ulong},{REAL},
                             {ulong},pointer(64,8,8,tag-ref(0))),{INT})
        reason rich_lowering_unavailable;
}}'''
    module, = ffic.parse_document(parser, graph.encode(), 'generated.ffi')
    original = verify(parser, module, TARGET)
    parsed = ffi_plan.parse_rich_overrides(parser, original.encode(), 'authored.ffi')
    assert parsed.obligations == (('caller_native_contract', 'Successful initialization requires review.'),)
    assert 'id' not in module['entries'][0]
    checked = ffi_plan.verify_plan(parsed, module, SYMBOL, NOMINALS)
    for before, after, same in (('alpha:f64', 'renamed:f64', True), ('status_eq(local(2),1)', 'status_eq(local(2),0)', False)):
        later = ffi_plan.parse_rich_overrides(parser, original.replace(before, after).encode(), 'changed.ffi')
        assert (ffi_plan.verify_plan(later, module, SYMBOL, NOMINALS).sha256 == checked.sha256) == same

    rejects = [
        (f'select c::{SYMBOL};', '', 'ownership selection'),
        ('effects unknown', 'effects trusted', 'unsupported marshalling effect'),
        ('snapshot values:slice(f64,256)', 'consume values:owner(mulib.histprice.Pair)', 'unsupported rich surface'),
        ('arg(0),local(0),1,256', 'local(0),arg(0),1,256', 'invalid marshalling operands'),
        ('arg(0),local(0),1,256', 'arg(0),local(32),1,256', 'graph integer overflow'),
        ('arg(0),local(0),1,256', 'arg(0),local(0),257,256', 'snapshot bound'),
        ('arg(0),local(0),1,256', 'arg(0),local(0),1,255', 'snapshot bound'),
        ('slice(f64,256)', 'slice(f64,257)', 'graph integer overflow'),
        ('slice_data(local(0),native(0))', 'slice_data(local(1),native(0))', 'unassigned'),
        ('slice_data(local(0),native(0))', 'slice_data(local(0),native(1))', 'native slot type mismatch'),
        ('scalar_arg(arg(2),native(3))', 'scalar_arg(arg(2),native(2))', 'already assigned'),
        ('check_i64(arg(2),1,1000000);', '', 'missing prior nonnegative check'),
        ('check_i64(arg(2),1,1000000);', 'check_i64(arg(2),1000000,1);', 'inverted integer domain'),
        ('check_f64(arg(1),zero_to_one);', 'check_f64(arg(1),unchecked);', 'unknown floating-point'),
        ('slice_length(local(0),native(1));', '', 'unbound native'),
        ('out_slot(local(1),native(4));', 'out_slot(local(0),native(4));', 'local reassigned'),
        ('init_field(local(1),field(1),nan);', '', 'uninitialized output field'),
        ('init_field(local(1),field(1),nan);', 'init_field(local(1),field(0),nan);', 'already initialized'),
        ('call(local(2));', 'out_field(local(1),field(0),local(3)); call(local(2));', 'before call'),
        ('status_eq(local(2),1);', '', 'exact int status branch'),
        ('status_eq(local(2),1);', 'status_eq(local(0),1);', 'exact int status branch'),
        ('status_eq(local(2),1);', 'status_eq(local(2),2147483648);', 'exact int status branch'),
        ('status_eq(local(2),1);', 'call(local(2)); status_eq(local(2),1);', 'exact int status branch'),
        ('out_field(local(1),field(1),local(4));', 'out_field(local(1),field(3),local(4));', 'initialized field'),
        ('out_field(local(1),field(1),local(4));', 'out_field(local(1),field(1),local(3));', 'local reassigned'),
        ('ok_record(local(3),local(4));', 'ok_record(local(3));', 'canonical record'),
        ('reject(Rejected);', 'reject(Forged);', 'canonical error continuation'),
        ('reject(Rejected);', 'reject(Rejected); call(local(5));', 'after marshalling terminal'),
        ('reject(Rejected);', '', 'incomplete marshalling control flow'),
        ('call(local(2));', 'owner_loan(arg(0),native(0)); call(local(2));', 'unsupported marshalling operation'),
        ('call(local(2));', 'callback_arg(arg(0),native(0)); call(local(2));', 'unsupported marshalling operation'),
        ('check_i64(arg(2),1,1000000);', 'check_i64(arg(2),1,1000000);' * 64, 'operation limit'),
        ('mulib.histprice.Forecast', 'mulib.priceopt.Forecast', 'different module'),
        (module['entries'][0]['native_fingerprint'], '0'*64, 'stale native graph fingerprint'),
    ]
    for old, new, text in rejects:
        assert old in original
        mutated = original.replace(old, new, 1)
        try:
            altered = ffi_plan.parse_rich_overrides(parser, mutated.encode(), 'invalid.ffi')
            ffi_plan.verify_plan(altered, module, SYMBOL, NOMINALS)
        except ffic.Invalid as exc:
            assert text in str(exc), (text, str(exc))
        else:
            raise AssertionError('accepted invalid plan: ' + text)
    for invalid, desc in (
        (lambda: ffi_plan.verify_plan(parsed, module, SYMBOL, {}), 'unknown or incompatible canonical nominal'),
        (lambda: next(ffic.parse_document(parser, original.encode(), 'unmerged.ffi')), 'unmerged override'),
    ):
        try:
            invalid()
        except ffic.Invalid as exc:
            assert desc in str(exc)
        else:
            raise AssertionError('accepted invalid plan: ' + desc)
    audit = ffi_plan.parse_rich_overrides(parser, (f'ffi 3; {TARGET} extend mulib.json {{'
        'obligation conditional_tree_ownership audit "Ownership depends on construction."; }').encode(), 'audit.ffi')
    assert audit.obligations == (('conditional_tree_ownership', 'Ownership depends on construction.'),)
    assert not audit.plans and not audit.selections
    print(f'PASS: typed numerical candidate, authored audit, {len(rejects) + 2} rejected plans; no executable publication.')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--parser', type=Path, required=True)
    args = ap.parse_args()
    run(ffic.load_parser(args.parser))
