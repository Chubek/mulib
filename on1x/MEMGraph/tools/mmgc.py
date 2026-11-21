#!/usr/bin/env python3
"""Validate MEMGraph/v1 and compile immutable C++ rule tables and a JSON plan.

The build uses only the generated LLSynth parse tree as source. Standalone
manifest/preview use without LibStone retains the legacy bootstrap parser.
Neither the manifest nor the C++ tables certify or execute a rewrite.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
from itertools import chain, islice
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

VERSION = 1
MAX_FILE = 256 * 1024
MAX_RULES = 512
MAX_PACKAGE_FILES = 256
MAX_PACKAGE_RULES = 4096
MAX_TREE_DEPTH = 32
EXECUTABLE_PROFILE = 'c11_fixed_array_v1'
STAGES = {"post_infer": 0, "post_ownership": 1, "pre_codegen": 2}
MODES = {"verify", "rewrite"}
PRESERVATION = {"value", "type", "evaluation", "effects", "ownership",
                "loans", "ffi", "traps", "provenance"}
# These names are host obligations, NOT claims that an author may establish.
REQUIRE = set("""
nominal_identity_resolved type_form_valid exact_module_identity
specialization_checked no_unsolved_holes cast_checked no_implicit_narrowing
target_abi_known container_layout_known bounds_proven_or_checked
index_effects_preserved provenance_valid capacity_contract_valid
mutable_buffer allocation_failure_preserved unique_owner source_initialized
not_previously_consumed no_live_loan drop_once destructor_correct
cleanup_edges_present active_payload_transferred result_identity_exact
owned_payload_transferred_once defer_order_preserved place_addressable
no_exclusive_conflict lexical_scope_valid no_other_live_loan parent_loan_live
child_scope_contained reborrow_access_valid safe_admission_independent
exact_cdecl_abi target_revision_pinned supported_effect_profile
error_contract_preserved callback_policy_valid immutable_call_scoped_input
no_embedded_nul byte_limit_checked no_foreign_retention
approved_static_lifetime null_result_checked immutable_static_result
bounded_result_scan explicit_unsafe_context raw_effects_conserved
typed_hir same_exact_type identity_cast_total no_hidden_effect
equivalent_validation_obligation same_bounds_snapshot no_intervening_write
copy_safe_scalar copy_no_trap loan_root_equal nested_scope_equal
owner_generation_unchanged no_move_or_drop_between identical_abi_check
pointer_ownership_verified owner_generation_current slice_lifetime_within_origin
slice_range_order_checked type_arguments_satisfy_constraints
 fixed_array_layout exact_i64_bounds emitted_bounds_check constant_bounds_in_range bounded_checked_i64_evaluation
""".split())
NEGATIVE = set("""
untrusted_type_claim unresolved_type known_freed_origin
place_exceeds_lexical_lifetime previously_consumed callback_or_closure_escape
loan_crosses_suspension missing_independent_review stale_target_or_signature
unsupported_pointer_ownership
""".split())
# An abstract host-produced MEMGraph, not raw On1x AST or a source parser.
# Captures stand for immutable graph snapshots, never raw C pointers.
ARITY = {
    "NominalValue": 1, "GenericInstance": 2, "Cast": 2,
    "ForgedTypeEvidence": 1, "UnknownType": 1, "Index": 2,
    "Slice": 2, "BufferMutate": 2, "Load": 1, "EscapePlace": 1,
    "Move": 1, "Drop": 1, "Take": 1, "Propagate": 1,
    "BorrowShared": 1, "BorrowMut": 1, "Reborrow": 1,
    "CaptureLoan": 1, "SuspendWithLoan": 1, "ForeignCall": 2,
    "UnsafeForeignCall": 2, "CStringBorrow": 1, "StaticCString": 1,
    "IdentityCast": 1, "TypeValidated": 1, "BoundsValidated": 1,
    "CopyScalar": 1, "ReadOnlyView": 1, "OwnerValidated": 1,
    "AbiCheckedScalar": 1,
    "CheckedArrayIndex": 2, "CheckedArraySlice": 3,
    "ProvedArrayIndex": 2, "ProvedArraySlice": 3,
}
PROVED_FOR = {'CheckedArrayIndex': 'ProvedArrayIndex', 'CheckedArraySlice': 'ProvedArraySlice'}
PROFILE_OPERATORS = set(PROVED_FOR) | set(PROVED_FOR.values())
ARRAY_COMMON = {'typed_hir', 'fixed_array_layout', 'exact_i64_bounds'}
ARRAY_CHECK = ARRAY_COMMON | {'emitted_bounds_check'}
ARRAY_REWRITE = ARRAY_COMMON | {'bounded_checked_i64_evaluation', 'constant_bounds_in_range',
                                'no_hidden_effect'}
LEX = re.compile(r'\s+|\#[^\n]*|"(?:[^"\\\r\n]|\\["\\nrt])*"|'
                 r'[A-Za-z_][A-Za-z_0-9]*|[0-9]+|[(),;$]')
IDENT = re.compile(r'[A-Za-z_][A-Za-z_0-9]*\Z')
NUM = re.compile(r'[0-9]+\Z')


class SpecError(Exception):
    pass


def die(message):
    raise SpecError(message)


def read_regular(path: Path) -> bytes:
    if path.is_symlink() or not path.is_file():
        die(f"file is not a regular non-symlink file: {path}")
    with path.open('rb') as stream:
        raw = stream.read(MAX_FILE + 1)
    if len(raw) > MAX_FILE:
        die(f"file exceeds {MAX_FILE} bytes: {path}")
    if b'\x00' in raw:
        die(f"embedded NUL in {path}")
    return raw


def decode_source(raw, name):
    try:
        return raw.decode('utf-8')
    except UnicodeDecodeError as exc:
        die(f"invalid UTF-8 in {name}: {exc}")


def source_error(name, line, message):
    die(f'{name}:{line}: {message}')


def validate_budget(budget, version, name, line):
    if version != VERSION:
        source_error(name, line, f'unsupported MEMGraph version {version}')
    if not (1 <= budget['steps'] <= 16777216 and 1 <= budget['nodes'] <= 2000000
            and 1 <= budget['depth'] <= 1024):
        source_error(name, line, 'budgets exceed permitted bounds or are zero')
    return budget


def validate_tree(name, line, op, args):
    if op not in ARITY:
        source_error(name, line, f'unknown graph operator {op!r}')
    if len(args) != ARITY[op]:
        source_error(name, line, f'{op} requires exactly {ARITY[op]} operands, got {len(args)}')
    return {'op': op, 'args': args}


def load_parser(path):
    spec = importlib.util.spec_from_file_location('memgraph_generated_parser', path)
    if spec is None or spec.loader is None:
        die(f'expected a generated LLSynth Python parser: {path}')
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module  # Required by the generated dataclasses.
    try:
        spec.loader.exec_module(module)
    finally:
        del sys.modules[spec.name]
    if not callable(getattr(module, 'parse', None)) or not hasattr(module, 'LanguageError'):
        die(f'missing LLSynth parse interface in {path}')
    return module


class ParseTree:
    """Transform the generated tree into semantic records, never tokenize source."""

    def __init__(self, name):
        self.name = name

    def string(self, node):
        try:
            return json.loads(node.token.lexeme)
        except json.JSONDecodeError:
            source_error(self.name, node.token.line, 'invalid escaped string')

    @staticmethod
    def list_nodes(node):
        # Includes / Declarations are left-recursive and may be large.
        result = []
        while node.children:
            node, item = node.children
            result.append(item)
        return reversed(result)

    @staticmethod
    def names(node):
        head, tail = node.children
        result = [head.value]
        while tail.children:
            _, item, tail = tail.children
            result.append(item.value)
        return result

    def tree(self, node, depth=0):
        head, *children = node.children
        if depth > MAX_TREE_DEPTH:
            source_error(self.name, head.token.line, f'tree recursion depth >{MAX_TREE_DEPTH}')
        if head.value == '$':
            return {'capture': children[0].value}
        args = []
        arguments = children[1].children
        if arguments:
            arg, tail = arguments
            while True:
                item, = arg.children
                args.append({'literal': self.string(item)} if item.symbol == 'STRING'
                            else self.tree(item, depth + 1))
                if not tail.children:
                    break
                _, arg, tail = tail.children
        return validate_tree(self.name, head.token.line, head.value, args)

    def declaration(self, node):
        rule, = node.children
        kind, name, _, priority, _, match, *tail = rule.children
        result = {'kind': kind.value, 'name': name.value, 'line': kind.token.line,
                  'priority': priority.value, 'match': self.tree(match)}
        if kind.value == 'rewrite':
            _, replace, _, conditions, _, preserves, _ = tail
            result.update(replace=self.tree(replace), preserves=self.names(preserves))
        else:
            _, conditions, _, diagnostic, _ = tail
            result['diagnostic'] = self.string(diagnostic)
        result['conditions'] = self.names(conditions)
        return result

    def document(self, root):
        header, document, _ = root.children
        _, version, _, _, _, steps, _, nodes, _, depth, _ = header.children
        budget = validate_budget({'steps': steps.value, 'nodes': nodes.value, 'depth': depth.value},
                                 version.value, self.name, version.token.line)
        if document.symbol == 'PassDocument':
            _, name, _, stage, _, mode, _, priority, _, profile, includes, _, _ = document.children
            result = {'kind': 'pass', 'name': name.value, 'stage': stage.token.lexeme,
                      'mode': mode.token.lexeme, 'priority': priority.value,
                      'includes': [self.string(i.children[1]) for i in self.list_nodes(includes)]}
            if profile.children:
                result['profile'] = profile.children[1].value
        elif document.symbol == 'PatternDocument':
            _, name, _, declarations, _, _ = document.children
            declarations = list(islice(self.list_nodes(declarations), MAX_RULES + 1))
            if not 1 <= len(declarations) <= MAX_RULES:
                source_error(self.name, name.token.line, f'pattern file needs 1..{MAX_RULES} declarations')
            result = {'kind': 'patterns', 'name': name.value,
                      'declarations': [self.declaration(d) for d in declarations]}
        else:
            die(f'{self.name}: unexpected generated parse tree {document.symbol}')
        return {'budget': budget, **result}


class BootstrapParser:
    """Compatibility only: never used when --parser or --llsynth is supplied."""

    def __init__(self, content: str, name: str):
        self.name = name
        self.tokens = []
        offset = 0
        line = 1
        while offset < len(content):
            m = LEX.match(content, offset)
            if not m:
                die(f"{name}:{line}: unexpected character {content[offset:offset + 18]!r}")
            raw = m.group(0)
            if not raw.isspace() and not raw.startswith('#'):
                self.tokens.append((raw, line))
            line += raw.count('\n')
            offset = m.end()
        self.tokens.append(('<eof>', line))
        self.pos = 0

    def peek(self):
        return self.tokens[self.pos][0]

    def pop(self):
        t = self.peek()
        if t == '<eof>':
            self.error('unexpected end of file')
        self.pos += 1
        return t

    def error(self, message):
        die(f"{self.name}:{self.tokens[self.pos][1]}: {message}; got {self.peek()!r}")

    def take(self, expected):
        if self.peek() != expected:
            self.error(f"expected {expected!r}")
        self.pos += 1

    def ident(self):
        t = self.peek()
        if not IDENT.fullmatch(t):
            self.error('expected identifier')
        self.pos += 1
        return t

    def number(self):
        t = self.peek()
        if not NUM.fullmatch(t):
            self.error('expected unsigned number')
        digits = t.lstrip('0')
        if len(digits) > 19 or (len(digits) == 19 and digits > '9223372036854775807'):
            source_error(self.name, self.tokens[self.pos][1], 'number exceeds signed i64')
        self.pos += 1
        return int(digits or '0')

    def string(self):
        t = self.peek()
        if not t.startswith('"'):
            self.error('expected string')
        self.pos += 1
        try:
            return json.loads(t)
        except json.JSONDecodeError:
            self.error('invalid escaped string')

    def names(self):
        result = [self.ident()]
        while self.peek() == ',':
            self.take(',')
            result.append(self.ident())
        return result

    def tree(self, depth=0):
        if depth > MAX_TREE_DEPTH:
            self.error(f'tree recursion depth >{MAX_TREE_DEPTH}')
        if self.peek() == '$':
            self.pop()
            return {'capture': self.ident()}
        line = self.tokens[self.pos][1]
        name = self.ident()
        self.take('(')
        args = []
        if self.peek() != ')':
            while True:
                if self.peek().startswith('"'):
                    args.append({'literal': self.string()})
                else:
                    args.append(self.tree(depth + 1))
                if self.peek() != ',':
                    break
                self.take(',')
        self.take(')')
        return validate_tree(self.name, line, name, args)

    def header(self):
        self.take('memgraph')
        line = self.tokens[self.pos][1]
        version = self.number()
        self.take(';')
        self.take('budget'); self.take('steps')
        steps = self.number()
        self.take('nodes'); nodes = self.number()
        self.take('depth'); depth = self.number()
        self.take(';')
        return validate_budget({'steps': steps, 'nodes': nodes, 'depth': depth},
                               version, self.name, line)

    def parse(self):
        budget = self.header()
        if self.peek() == 'pass':
            self.take('pass'); name = self.ident()
            self.take('stage'); stage = self.ident()
            self.take('mode'); mode = self.ident()
            self.take('priority'); priority = self.number()
            self.take(';')
            profile = None
            if self.peek() == 'profile':
                self.take('profile'); profile = self.ident(); self.take(';')
            includes = []
            while self.peek() == 'include':
                self.take('include'); includes.append(self.string()); self.take(';')
            self.take('endpass'); self.take(';')
            kind = {'kind': 'pass', 'name': name, 'stage': stage, 'mode': mode,
                    'priority': priority, 'includes': includes}
            if profile is not None:
                kind['profile'] = profile
        elif self.peek() == 'patterns':
            self.take('patterns'); name = self.ident(); self.take(';')
            declarations = []
            while self.peek() in ('check', 'reject', 'rewrite'):
                line = self.tokens[self.pos][1]
                kind = self.pop()
                rule_name = self.ident()
                self.take('priority'); priority = self.number()
                self.take('match'); match = self.tree()
                d = {'kind': kind, 'name': rule_name, 'priority': priority, 'match': match, 'line': line}
                if kind == 'rewrite':
                    self.take('replace'); d['replace'] = self.tree()
                self.take({'check': 'requires', 'reject': 'when', 'rewrite': 'requires'}[kind])
                d['conditions'] = self.names()
                if kind == 'rewrite':
                    self.take('preserves'); d['preserves'] = self.names()
                else:
                    self.take('diagnostic'); d['diagnostic'] = self.string()
                self.take(';')
                declarations.append(d)
                if len(declarations) > MAX_RULES:
                    self.error(f'pattern file needs 1..{MAX_RULES} declarations')
            self.take('endpatterns'); self.take(';')
            kind = {'kind': 'patterns', 'name': name, 'declarations': declarations}
        else:
            self.error('expected pass or patterns declaration')
        if self.peek() != '<eof>':
            self.error('extra tokens after document')
        return {'budget': budget, **kind}


def capture_order(node):
    if 'capture' in node:
        return [node['capture']]
    result = []
    for ch in node['args']:
        if 'literal' not in ch:
            result.extend(capture_order(ch))
    return result


def check_declaration(declaration, mode, source):
    name = f'{source}:{declaration["line"]}: {declaration["name"]}'
    kind = declaration['kind']
    for key in ('conditions', 'preserves'):
        values = declaration.get(key, [])
        if len(values) != len(set(values)):
            die(f'{name}: duplicate obligation/preservation name')
    if not (0 <= declaration['priority'] <= 10000):
        die(f'{name}: invalid rule priority')
    if 'capture' in declaration['match']:
        die(f'{name}: match root cannot be a bare capture')
    captures = capture_order(declaration['match'])
    if len(set(captures)) != len(captures):
        die(f'{name}: a match capture must occur exactly once')
    if not captures:
        die(f'{name}: match needs at least one capture')
    if kind == 'rewrite':
        if mode != 'rewrite':
            die(f'{name}: rewrite in verify-only pass')
        if capture_order(declaration['replace']) != captures:
            die(f'{name}: replacement must use every capture exactly once, in evaluation order')
        if set(declaration['preserves']) != PRESERVATION:
            die(f'{name}: all nine preservation obligations are mandatory')
        conditions = set(declaration['conditions'])
        if 'typed_hir' not in conditions or 'no_hidden_effect' not in conditions:
            die(f'{name}: rewrite needs typed_hir and no_hidden_effect obligations')
    elif not re.fullmatch(r'MMG-[A-Z]+-[A-Z][A-Z0-9-]*', declaration['diagnostic']):
        die(f'{name}: malformed diagnostic ID')
    recognized = NEGATIVE if kind == 'reject' else REQUIRE
    unknown = set(declaration['conditions']) - recognized
    if unknown:
        die(f'{name}: unsupported {kind} condition(s): {sorted(unknown)}')


def check_profile_rule(rule, profile):
    name = f'{rule["source"]}:{rule["line"]}: {rule["name"]}'
    match = rule['match']
    replace = rule.get('replace')
    if profile != EXECUTABLE_PROFILE:
        def uses_executable_operator(tree):
            return (tree.get('op') in PROFILE_OPERATORS
                    or any(uses_executable_operator(t) for t in tree.get('args', [])))

        if uses_executable_operator(match) or (replace and uses_executable_operator(replace)):
            die(f'{name}: fixed-array operators require executable profile {EXECUTABLE_PROFILE}')
        return

    # Refuse to compile rule declarations outside this profile's schema.
    # These declarations still grant no proof authority to the host.
    if rule['kind'] not in ('check', 'rewrite'):
        die(f'{name}: unsupported executable rule kind {rule["kind"]!r}')
    proved = PROVED_FOR.get(match.get('op'))
    if proved is None or any('capture' not in t for t in match['args']):
        die(f'{name}: executable match must be a checked fixed-array operator over captures')
    if replace is not None and (replace.get('op') != proved or replace['args'] != match['args']):
        die(f'{name}: executable replacement must be the corresponding proved operator over the same captures')
    expected = ARRAY_REWRITE if rule['kind'] == 'rewrite' else ARRAY_CHECK
    if set(rule['conditions']) != expected:
        die(f'{name}: executable {rule["kind"]} requires exactly {", ".join(sorted(expected))}')


def include_path(root: Path, name: str):
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]*\.mmgi', name):
        die(f'unsafe or malformed include filename: {name!r}')
    p = root / name
    if p.is_symlink() or not p.is_file() or p.resolve().parent != root.resolve():
        die(f'include must be an existing regular sibling: {name!r}')
    return p


def compile_package(root: Path, *, parser: Path | None = None):
    """Return the deterministic plan; a supplied parser is authoritative."""
    root = root.resolve()
    spec = root / 'MEMGraph.llsynth'
    grammar = read_regular(spec)
    decode_source(grammar, spec.name)
    inputs = {spec.name: hashlib.sha256(grammar).hexdigest()}
    sources = list(islice(chain(root.glob('*.mmg'), root.glob('*.mmgi')), MAX_PACKAGE_FILES + 1))
    if len(sources) > MAX_PACKAGE_FILES:
        die(f'package exceeds {MAX_PACKAGE_FILES} .mmg/.mmgi files')
    generated = load_parser(parser) if parser is not None else None

    def document(path):
        raw = read_regular(path)
        text = decode_source(raw, path.name)
        inputs[path.name] = hashlib.sha256(raw).hexdigest()
        if generated is None:
            return BootstrapParser(text, path.name).parse()
        try:
            tree = generated.parse(text)
        except generated.LanguageError as exc:
            die(f'{path.name}:{exc}')
        try:
            return ParseTree(path.name).document(tree)
        except (AttributeError, IndexError, ValueError) as exc:
            die(f'{path.name}: incompatible generated MEMGraph parse tree: {exc}')

    passes = []
    all_names = set()
    executable = None
    total_rules = 0
    for source in sorted(x for x in sources if x.name.endswith('.mmg')):
        p = document(source)
        if p['kind'] != 'pass':
            die(f'{source.name}: .mmg must contain a pass')
        if p['stage'] not in STAGES or p['mode'] not in MODES or not (0 <= p['priority'] <= 10000):
            die(f'{source.name}: unknown stage/mode or invalid priority')
        if p['name'] in all_names:
            die(f'duplicate pass name: {p["name"]}')
        all_names.add(p['name'])
        if 'profile' in p:
            if p['profile'] != EXECUTABLE_PROFILE:
                die(f'{source.name}: unsupported executable profile {p["profile"]!r}')
            if p['stage'] != 'pre_codegen' or p['mode'] != 'rewrite':
                die(f'{source.name}: profile {EXECUTABLE_PROFILE} requires pre_codegen rewrite mode')
            if executable is not None:
                die(f'{source.name}: duplicate executable profile {EXECUTABLE_PROFILE} in {executable}')
            executable = source.name
        if not 1 <= len(p['includes']) <= 32 or len(p['includes']) != len(set(p['includes'])):
            die(f'{source.name}: pass must include 1..32 unique pattern files')
        names = set()
        rules = []
        for inc in p['includes']:
            program = document(include_path(root, inc))
            if program['kind'] != 'patterns' or program['name'] != p['name']:
                die(f'{inc}: pattern group does not match pass {p["name"]}')
            if program['budget'] != p['budget']:
                die(f'{inc}: budget differs from pass {source.name}')
            if not 1 <= len(program['declarations']) <= MAX_RULES:
                die(f'{inc}: pattern file needs 1..{MAX_RULES} declarations')
            if len(rules) + len(program['declarations']) > MAX_RULES:
                die(f'{source.name}: total rule count exceeds limit')
            total_rules += len(program['declarations'])
            if total_rules > MAX_PACKAGE_RULES:
                die(f'{inc}: package exceeds {MAX_PACKAGE_RULES} rules')
            for rule in program['declarations']:
                if rule['name'] in names:
                    die(f'{source.name}: duplicate rule {rule["name"]}')
                names.add(rule['name'])
                check_declaration(rule, p['mode'], inc)
                rules.append({'source': inc, **rule})
        rules.sort(key=lambda r: (-r['priority'], r['name']))
        passes.append({'source': source.name, **p, 'rules': rules})
    if not passes:
        die('no .mmg pass files')
    all_patterns = {x.name for x in sources if x.name.endswith('.mmgi')}
    if all_patterns != {inc for p in passes for inc in p['includes']}:
        die('orphan or unreferenced .mmgi file')
    for p in passes:
        for rule in p['rules']:
            check_profile_rule(rule, p.get('profile'))
    passes.sort(key=lambda p: (STAGES[p['stage']], -p['priority'], p['name']))
    # Preserve the v1 manifest marker: the plan itself implements no host admission.
    return {'schema': 'on1x.memgraph-plan/v1', 'source_type': 'untrusted_declarative',
            'host_admission': 'required_not_implemented', 'inputs_sha256': dict(sorted(inputs.items())),
            'passes': passes}


def run_llsynth(root: Path, script: Path):
    """Generate the actual parser, then compile directly from its tree."""
    if not script.is_file() or script.name != 'llsynth.pl':
        die('expected the real LibStone cornerstone/LLSynth/scripts/llsynth.pl')
    read_regular(root / 'MEMGraph.llsynth')
    with tempfile.TemporaryDirectory(prefix='mmg-llsynth-') as temp:
        output = Path(temp) / 'memgraph_parser.py'
        command = ['perl', str(script.resolve()), '--mode=interpret', '-T', 'python3',
                   '-o', str(output), str(root / 'MEMGraph.llsynth')]
        r = subprocess.run(command, capture_output=True, encoding='utf-8', timeout=120)
        if r.returncode:
            die('LLSynth generation failed:\n' + r.stderr)
        return compile_package(root, parser=output)


def emit_header(plan):
    compiled = next((p for p in plan['passes'] if p.get('profile') == EXECUTABLE_PROFILE), None)
    if compiled is None:
        die(f'cannot emit C++ tables without executable profile {EXECUTABLE_PROFILE}')

    terms, conditions, rules = [], [], []

    def flatten(tree):
        if 'capture' in tree:
            terms.append(('$' + tree['capture'], 0))
        else:
            terms.append((tree['op'], len(tree['args'])))
            for arg in tree['args']:
                flatten(arg)

    for rule in compiled['rules']:
        check_profile_rule(rule, EXECUTABLE_PROFILE)
        match_begin = len(terms)
        flatten(rule['match'])
        replace_begin = len(terms)
        if 'replace' in rule:
            flatten(rule['replace'])
        cond_begin = len(conditions)
        conditions.extend(rule['conditions'])
        rules.append((
            rule['kind'], rule['name'], rule['source'], rule.get('diagnostic', ''),
            rule['line'], match_begin, replace_begin - match_begin,
            replace_begin, len(terms) - replace_begin, cond_begin, len(conditions) - cond_begin,
        ))

    def array(name, ctype, rows):
        # All strings here are grammar identifiers or validated diagnostics/filenames.
        return (f'inline constexpr std::array<{ctype}, {len(rows)}> {name}{{{{\n'
                + ''.join(f'  {row},\n' for row in rows) + '}};\n')

    def row(values):
        return '{' + ', '.join(json.dumps(x) for x in values) + '}'

    return """// Generated by mmgc.py from MEMGraph rules. Do not edit.
#pragma once
#include <array>
#include <cstddef>
#include <string_view>

namespace on1x::memgraph::compiled {

struct Term { std::string_view name; std::size_t arity; };
struct Budget { std::size_t steps, nodes, depth; };
struct Rule {
  std::string_view kind, name, source, diagnostic;
  std::size_t line;
  std::size_t match_begin, match_size, replace_begin, replace_size;
  std::size_t conditions_begin, conditions_size;
};

""" + ('inline constexpr Budget budget{%(steps)d, %(nodes)d, %(depth)d};\n\n'
       % compiled['budget']) + '\n'.join((
           array('terms', 'Term', [row(term) for term in terms]),
           array('conditions', 'std::string_view', [json.dumps(c) for c in conditions]),
           array('rules', 'Rule', [row(rule) for rule in rules]),
       )) + '\n} // namespace on1x::memgraph::compiled\n'


def write_output(path, text):
    staged = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=f'.{path.name}.', delete=False,
                                         mode='w', encoding='utf-8', newline='\n') as stream:
            staged = Path(stream.name)
            stream.write(text)
        staged.replace(path)
    finally:
        if staged is not None:
            staged.unlink(missing_ok=True)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    p.add_argument('--manifest', type=Path, help='write deterministic JSON manifest (not an admission)')
    p.add_argument('--header', '--output', type=Path, help='write the immutable executable profile tables')
    parser = p.add_mutually_exclusive_group()
    parser.add_argument('--parser', type=Path, help='use a generated LLSynth Python parser')
    parser.add_argument('--llsynth', type=Path, help='generate and use a parser from a real LibStone llsynth.pl')
    args = p.parse_args(argv)
    if args.header and not (args.parser or args.llsynth):
        p.error('--header requires --parser or --llsynth')
    try:
        result = (run_llsynth(args.root.resolve(), args.llsynth) if args.llsynth
                  else compile_package(args.root, parser=args.parser))
        header = emit_header(result) if args.header else None
        if args.manifest:
            write_output(args.manifest, json.dumps(result, indent=2, sort_keys=True) + '\n')
        if args.header:
            write_output(args.header, header)
        counts = {k: sum(r['kind'] == k for x in result['passes'] for r in x['rules'])
                  for k in ('check', 'reject', 'rewrite')}
        print(f'MEMGraph v1: {len(result["passes"])} passes, '
              f'{sum(counts.values())} rules {counts}; host admission required')
        return 0
    except (SpecError, OSError, subprocess.TimeoutExpired) as exc:
        print(f'MEMGraph error: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
