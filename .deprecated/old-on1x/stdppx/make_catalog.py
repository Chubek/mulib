#!/usr/bin/env python3
"""Regenerate catalog/docs from this package's checked implementation inventory."""
import json
import argparse
from pathlib import Path
p=Path(__file__).parent
cli=argparse.ArgumentParser(description=__doc__)
cli.add_argument('--refresh-designs',action='store_true',help='also regenerate planned design documents')
options=cli.parse_args()
original={
'identity':('AST identity and schema round-trip','none'),
'checks':('assert_eq/assert_ne intrinsic wrappers','none'),
'control':('when/unless/while/until statement expansion','none'),
'unroll':('bounded repeat() expansion','none'),
'derive':('function arity/signature metadata only','none'),
'fold':('trap-preserving integer literal folding','none')}
additional={
'pipe':('apply(value,f,g,...) left-to-right unary calls','none'),
'compose':('apply(value,outer,inner,...) right-to-left unary calls','none'),
'cond':('branches(condition,Block,...,fallbackBlock)','none'),
'tap':('call(value,callback) with single evaluation','type inference for fresh parameter'),
'assertions':('assert_true/false, assert_eq/ne, single-evaluation between','none'),
'trace':('entry([message]) function entry logs','io_stderr'),
'logging':('debug/info/warn/error string statements','io_stderr and text_concat'),
'format':('string(template,{0},{1},...) string-only formatting','text_concat'),
'embed':('text(relative_filename) compile-time resource embed','ON1X_PPX_EMBED_ROOT'),
'regex':('full/prefix/suffix literal string matcher; no regex metacharacters','text_len and text_slice'),
'contract':('requires(condition) function preconditions','checked bool/intrinsic assert'),
'match_int':('cases(state,integer,Block,...,defaultBlock)','i64'),
'state_machine':('transition(state,event,from,on,to,...) for i64','i64'),
'cfg':('enabled/disabled(feature) conditional function declarations','ON1X_PPX_FEATURES'),
'test':('case() zero-arg void test runner generation','none'),
'return_guard':('nonnegative postcondition on i64 returns','none'),
'timing':('elapsed() instrumentation of scalar functions','link runtime/on1x_ppx_timing.c'),
'ffi_bind':('raw_scalar metadata admission for exact simple raw C ABI','ON1X_PPX_FFI_MANIFEST and target')}
blocked={
'letops':('Monadic/applicative binding expansion','Option/Result ADTs, typed lowering and appropriate effects'),
'derive_eq':('Structural equality derivation','Record/variant declaration constructors, typed trait coherence'),
'derive_ord':('Total ordering derivation','ADTs and exhaustive field ordering semantics'),
'derive_hash':('Stable hashing derivation','ADTs and hash seed/ordering policy'),
'derive_show':('Readable structured formatting','ADTs, recursive display and cycle policy'),
'derive_debug':('Debug-formatted structured values','ADTs, field metadata and privacy rules'),
'derive_clone':('Safe resource cloning','Ownership/borrow semantics, fallible cloning contracts'),
'derive_json':('JSON encoders and decoders','ADTs, Result errors, ownership-safe JSON bindings'),
'derive_binary':('Binary codec and versioned schemas','ADTs, byte arrays and serialization invariants'),
'derive_lens':('Record-field lenses','Records and generated field types'),
'derive_builder':('Named-field builders','Records, defaults, field validation, ownership'),
'derive_visitor':('Per-ADT visitor dispatch','Variant constructors and typed recursion'),
'derive_fold':('Typed catamorphism and recursion schemes','ADTs and type-level recursion mapping'),
'derive_arbitrary':('Property-testing generator','ADTs, PRNG, sized recursive generators'),
'ffi_safe':('Verified ownership-safe MuLIB wrappers','ExoML reviewed contracts, Exotract ABI, Chisel/Wetstone gates'),
'async':('Async/await syntax lowering','Typed continuation/CPS pass, scheduler and Plaid'),
'actor':('Actor/message-processing syntax','ADTs, async channels, ownership and Plaid')}
entries=[]
native=set(original)|{'pipe','compose','assertions','cond','format','logging','state_machine','tap','trace','LuaEmbed','GrammarEmbed'}
for d,status in [(original,'existing'),(additional,'implemented-python'),(blocked,'design-only')]:
    for name,(description,need) in sorted(d.items()):
        entries.append({'name':name,'status':status,'description':description,'prerequisites':need,'entrypoint':f'{name}.py' if status!='design-only' else None,'native':('native/'+name+'.on1x') if name in native else None})
entries.append({'name':'LuaEmbed','status':'implemented-native',
                'description':'Bounded compile-time Mune Lua, captured stdout, schema-checked AST streams/programs and chaining',
                'prerequisites':'MuLIB mune via the canonical single-TU planner; explicit PPX authorization',
                'entrypoint':None,'native':'native/LuaEmbed.on1x'})
entries.append({'name':'GrammarEmbed','status':'implemented-native',
                'description':'Perl 6-like grammar/token/rule constructs; liblr NFA/DFA lexers and LR1/LALR1/LL1 parser generation',
                'prerequisites':'LibStone firestone/liblr at PPX build time; explicit PPX authorization',
                'entrypoint':None,'native':'native/GrammarEmbed.on1x'})
result={'package':'on1x/stdppx','protocol':'on1x.sexpr-ast/v1','status_note':'The 24 Python processors and 17 ordinary native processors have compiler-generated C11 execution tests. LuaEmbed and GrammarEmbed are native-only. See VERIFICATION.md for measured configurations. Design-only entries are not implementations.','counts':{'existing_python':len(original),'new_python':len(additional),'design_only':len(blocked),'native_sources':len(native)},'processors':entries}
(p/'CATALOG.json').write_text(json.dumps(result,indent=2)+'\n')
rows=['| Processor | Status | What it does |','|---|---|---|']
for x in entries:rows.append(f"| `{x['name']}` | {x['status']} | {x['description']} |")
(p/'docs/CATALOG.md').write_text('# Complete PPX catalog\n\n'+'\n'.join(rows)+'\n\n`implemented-python` and `implemented-native` processors have protocol and compiler-generated C11 execution tests. LuaEmbed and GrammarEmbed are native-only. See `VERIFICATION.md` for measured configurations. `design-only` means documentation exists but no processor executable is shipped.\n')
for name,(purpose,need) in blocked.items():
    if not options.refresh_designs:continue
    text=f'''# {name} — proposed PPX, NOT IMPLEMENTED

**Goal:** {purpose}. This entry is a specification and development checklist, not an executable processor.

## Dependencies

{need}. On1x's present PPX wire tree contains functions, statements and expressions, **not** record or variant declarations. The PPX runs before inference, so it cannot assert inferred type, effect, ownership, ABI or lifetime facts.

## Implementation contract

1. Extend On1x grammar/ASDL and the PPX schema only when necessary, with a versioned migration and backward-compatible input validation.
2. Define precise user-facing syntax; write successful and failing examples. Do not allow unregistered extension carriers to reach core HIR.
3. Implement a bounded, pure AST transformation for syntactic work. If typed information is necessary, lower an explicit marker into a compiler-owned post-elaboration pass instead of guessing.
4. Preserve hygiene, evaluation order, source spans, imports and ownership; fail closed if the source feature is unsupported.
5. Validate the resulting AST against `grammar/PPX.schema.json`, perform `on1xc --emit-c` round trips, execute binaries under AddressSanitizer and UndefinedBehaviorSanitizer where applicable, and add negative compiler tests.

## Admission criteria

Do not mark this processor implemented until it has actual source code, a declared protocol/version, passing integration tests, documented type and effect behavior, and a target/backend compatibility statement. Do not treat ExoML contract metadata as a proof of safe FFI behavior.
'''
    (p/'planned'/f'{name}.md').write_text(text)
print('Catalog:',len(entries),'processors,',len(original)+len(additional),'executable Python,',len(blocked),'design only')
