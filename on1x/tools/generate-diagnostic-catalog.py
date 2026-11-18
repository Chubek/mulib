#!/usr/bin/env python3
"""Validate the trusted diagnostic catalog and generate its embedded C++ payload."""
import argparse
import json
import re
from pathlib import Path


class InvalidCatalog(ValueError):
    pass


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise InvalidCatalog()
        result[key] = value
    return result


def object_shape(value, required, optional=()):
    if not isinstance(value, dict):
        raise InvalidCatalog()
    if not set(required) <= set(value) or not set(value) <= set(required) | set(optional):
        raise InvalidCatalog()


def string(value, limit=4096):
    if not isinstance(value, str) or not value or len(value.encode()) > limit or '\0' in value:
        raise InvalidCatalog()
    return value


def natural(value, maximum):
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= maximum:
        raise InvalidCatalog()
    return value


WORD = re.compile(r'[a-z][a-z0-9-]{0,63}\Z')
CODE = re.compile(r'[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)*\Z')
FIELDS = {'message', 'expected', 'found', 'replacement', 'count'}


def word(value):
    value = string(value, 64)
    if not WORD.fullmatch(value):
        raise InvalidCatalog()
    return value


def code(value):
    value = string(value, 128)
    if not CODE.fullmatch(value):
        raise InvalidCatalog()
    return value


def template(value):
    value = string(value)
    position = 0
    while position < len(value):
        begin = value.find('{', position)
        close = value.find('}', position)
        if close != -1 and (begin == -1 or close < begin):
            raise InvalidCatalog()
        if begin == -1:
            break
        end = value.find('}', begin + 1)
        if end == -1 or value[begin + 1:end] not in FIELDS:
            raise InvalidCatalog()
        position = end + 1
    return value


def string_list(value, maximum, names=False):
    if not isinstance(value, list) or len(value) > maximum:
        raise InvalidCatalog()
    result = [word(item) if names else string(item, 128) for item in value]
    if len(set(result)) != len(result):
        raise InvalidCatalog()
    return result


def validate(root):
    pending = [(root, 0)]
    nodes = 0
    while pending:
        value, depth = pending.pop()
        nodes += 1
        if nodes > 65536 or depth >= 64:
            raise InvalidCatalog()
        if isinstance(value, dict):
            pending.extend((child, depth + 1) for child in value.values())
        elif isinstance(value, list):
            pending.extend((child, depth + 1) for child in value)
    object_shape(root, ('schema', 'levels', 'providers', 'repair_policies', 'presentation',
                        'default_scrutiny', 'scrutiny', 'diagnostics'))
    if root['schema'] != 'on1x.diagnostic-catalog/v2':
        raise InvalidCatalog()

    if not isinstance(root['levels'], list) or len(root['levels']) != 3:
        raise InvalidCatalog()
    levels, level_kinds = {}, set()
    for item in root['levels']:
        object_shape(item, ('name', 'kind', 'failure', 'sarif', 'color'))
        name, kind = word(item['name']), natural(item['kind'], 2)
        if name in levels or kind in level_kinds or not isinstance(item['failure'], bool):
            raise InvalidCatalog()
        word(item['sarif'])
        natural(item['color'], 107)
        levels[name], level_kinds = item, level_kinds | {kind}

    if not isinstance(root['providers'], list) or len(root['providers']) != 3:
        raise InvalidCatalog()
    providers, provider_kinds = {}, set()
    for item in root['providers']:
        object_shape(item, ('name', 'kind', 'origin'))
        name, kind = word(item['name']), natural(item['kind'], 2)
        if name in providers or kind in provider_kinds:
            raise InvalidCatalog()
        word(item['origin'])
        providers[name], provider_kinds = item, provider_kinds | {kind}

    if not isinstance(root['repair_policies'], list) or len(root['repair_policies']) != 4:
        raise InvalidCatalog()
    repairs, repair_kinds = {}, set()
    optional = ('insert_id', 'replace_id', 'end_marker', 'candidate_id',
                'maximum_distance', 'replacements', 'rationale', 'proposal')
    for item in root['repair_policies']:
        object_shape(item, ('name', 'kind', 'candidate_description', 'notice'), optional)
        name, kind = word(item['name']), natural(item['kind'], 3)
        if name in repairs or kind in repair_kinds:
            raise InvalidCatalog()
        template(item['candidate_description'])
        template(item['notice'])
        present = {key for key in optional if key in item}
        expected = {1: {'insert_id', 'replace_id', 'end_marker'},
                    2: {'candidate_id', 'maximum_distance', 'replacements'},
                    3: {'rationale', 'proposal'}}.get(kind, set())
        if present != expected:
            raise InvalidCatalog()
        if kind == 1:
            word(item['insert_id'])
            word(item['replace_id'])
            string(item['end_marker'], 128)
        elif kind == 2:
            template(item['candidate_id'])
            if natural(item['maximum_distance'], 8) == 0:
                raise InvalidCatalog()
            if not string_list(item['replacements'], 32):
                raise InvalidCatalog()
        elif kind == 3:
            template(item['rationale'])
            template(item['proposal'])
        repairs[name], repair_kinds = item, repair_kinds | {kind}

    presentation_fields = ('repair_status', 'diagnostic_label', 'category_label', 'note_label',
                           'help_label', 'repair_label', 'bytes_label', 'arrow_label', 'dropped',
                           'invalid_edits', 'source_limit', 'missing_validator', 'validator_failure',
                           'compiler_validated', 'source_ppx_unavailable', 'ppx_client_failure',
                           'already_valid', 'ambiguous_repairs', 'sarif_tool')
    presentation = root['presentation']
    object_shape(presentation, (*presentation_fields, 'command_errors'))
    if not isinstance(presentation['command_errors'], list) or len(presentation['command_errors']) != 17:
        raise InvalidCatalog()
    for message in presentation['command_errors']:
        template(message)
    if len(string_list(presentation['repair_status'], 4, names=True)) != 4:
        raise InvalidCatalog()
    for key in presentation_fields[1:]:
        template(presentation[key])

    policies, ranks = {}, set()
    if not isinstance(root['scrutiny'], list) or not 1 <= len(root['scrutiny']) <= 32:
        raise InvalidCatalog()
    for item in root['scrutiny']:
        object_shape(item, ('name', 'rank', 'enabled', 'checks', 'severity_overrides'))
        name, rank = word(item['name']), natural(item['rank'], 31)
        if name in policies or rank in ranks or not isinstance(item['enabled'], bool):
            raise InvalidCatalog()
        checks = set(string_list(item['checks'], 32, names=True))
        overrides = item['severity_overrides']
        if not isinstance(overrides, list) or len(overrides) > 8:
            raise InvalidCatalog()
        seen = set()
        for change in overrides:
            object_shape(change, ('from', 'to'))
            source, target = word(change['from']), word(change['to'])
            if source not in levels or target not in levels or source in seen:
                raise InvalidCatalog()
            seen.add(source)
        policies[name] = (item, checks)
        ranks.add(rank)
    default = word(root['default_scrutiny'])
    if default not in policies:
        raise InvalidCatalog()
    for left, left_checks in policies.values():
        for right, right_checks in policies.values():
            if left['rank'] < right['rank'] and not left_checks <= right_checks:
                raise InvalidCatalog()

    entries = root['diagnostics']
    if not isinstance(entries, list) or not 1 <= len(entries) <= 512:
        raise InvalidCatalog()
    required = ('id', 'role', 'provider', 'level', 'message', 'category', 'phase',
                'guidance', 'remediation', 'repair')
    optional = ('match', 'repair_tokens', 'upstream', 'check', 'minimum_scrutiny', 'processor')
    ids, matches, roles, upstream_keys, processors = set(), set(), set(), set(), set()
    for item in entries:
        object_shape(item, required, optional)
        identity, role = code(item['id']), natural(item['role'], 5)
        provider, level, repair = word(item['provider']), word(item['level']), word(item['repair'])
        if identity in ids or provider not in providers or level not in levels or repair not in repairs:
            raise InvalidCatalog()
        ids.add(identity)
        template(item['message'])
        word(item['category'])
        word(item['phase'])
        template(item['guidance'])
        template(item['remediation'])
        match = code(item['match']) if 'match' in item else None
        if match and match in matches:
            raise InvalidCatalog()
        if match:
            matches.add(match)
        if ('repair_tokens' in item) != (repairs[repair]['kind'] == 1):
            raise InvalidCatalog()
        if 'repair_tokens' in item:
            if not string_list(item['repair_tokens'], 32):
                raise InvalidCatalog()
        provider_kind = providers[provider]['kind']
        if role:
            if role in roles or provider_kind != 0 or any(key in item for key in
                                                          ('match', 'upstream', 'check',
                                                           'minimum_scrutiny', 'processor')):
                raise InvalidCatalog()
            if role != 1 and not re.sub(r'\{[^{}]+\}', '', item['message']):
                raise InvalidCatalog()
            roles.add(role)
        if provider_kind == 0:
            if any(key in item for key in ('upstream', 'check', 'minimum_scrutiny', 'processor')):
                raise InvalidCatalog()
        elif provider_kind == 1:
            if role or match or 'processor' in item or not all(key in item for key in
                                                               ('upstream', 'check',
                                                                'minimum_scrutiny')):
                raise InvalidCatalog()
            upstream = item['upstream']
            object_shape(upstream, ('rule', 'level', 'adapter'))
            rule = string(upstream['rule'], 128)
            source_level = natural(upstream['level'], 1)
            natural(upstream['adapter'], 2)
            check = word(item['check'])
            minimum = word(item['minimum_scrutiny'])
            if minimum not in policies or (rule, source_level) in upstream_keys or not check:
                raise InvalidCatalog()
            upstream_keys.add((rule, source_level))
        else:
            if role or match or any(key in item for key in ('upstream', 'check',
                                                            'minimum_scrutiny')) or 'processor' not in item:
                raise InvalidCatalog()
            processor = word(item['processor'])
            if processor in processors:
                raise InvalidCatalog()
            processors.add(processor)
    if roles != {1, 2, 3, 4, 5}:
        raise InvalidCatalog()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    raw = args.input.read_text(encoding='utf-8')
    if len(raw.encode('utf-8')) > 1024 * 1024:
        raise InvalidCatalog()
    root = json.loads(raw, object_pairs_hook=unique_object,
                      parse_float=lambda _: (_ for _ in ()).throw(InvalidCatalog()))
    validate(root)
    # C++ universal-character escapes cannot represent JSON surrogate pairs.
    # Fixed-width octal bytes preserve UTF-8 and literal JSON escape spelling.
    def cpp_string(value):
        return '"' + ''.join(f'\\{byte:03o}' for byte in value.encode('utf-8')) + '"'

    payload = cpp_string(raw)
    origin = cpp_string(args.input.as_posix())
    args.output.write_text(
        '#pragma once\n'
        'namespace on1x::diagnostic {\n'
        f'inline constexpr char embedded_diagnostic_catalog[] = {payload};\n'
        f'inline constexpr char embedded_diagnostic_catalog_origin[] = {origin};\n'
        '}\n',
        encoding='utf-8')


if __name__ == '__main__':
    try:
        main()
    except (InvalidCatalog, json.JSONDecodeError, OSError):
        # With no validated trusted catalog, fail configuration by status only.
        # Do not create a second implementation-literal bootstrap diagnostic.
        raise SystemExit(1) from None
