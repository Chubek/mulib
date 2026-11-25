"""Resolve the On1x HTTP(S) transport policy without inheriting unrelated environment."""
import os
import re


ROUTES = {'proxy-http': 'http_proxy', 'proxy-https': 'https_proxy', 'proxy-any': 'any_proxy'}
DIRECTIVES = frozenset((*ROUTES, 'proxy-none'))
ROOT_ONLY = re.compile(rb'^[ \t\r]*proxy-(?:http|https|any|none)(?=[ \t\r#]|\\[ \t\r]*$|$)')
_NAMES = (*ROUTES.values(), 'no_proxy')
_MAX_BYTES = 4096


def _check(value, name, *, bypass=False):
    # Never interpolate a value: a proxy URL may contain credentials.
    if len(value) > _MAX_BYTES or any(
            not (0x21 <= ord(c) <= 0x7e or bypass and c in ' \t') for c in value):
        raise ValueError(f'{name} contains an invalid or overlong proxy setting')
    return value


def manifest_url(verb, value):
    """Validate the already decoded quoted field of a root proxy route."""
    if not value:
        raise ValueError(f'{verb} requires a nonempty quoted proxy URL')
    return _check(value, verb)


def validate_manifest(options):
    if 'proxy-none' in options and ROUTES.keys() & options.keys():
        raise ValueError('proxy-none cannot be combined with other proxy directives')


def _ambient(environ):
    selected = {}
    for key, value in environ.items():
        name = key.lower()
        if name not in _NAMES or not key.isascii():
            continue
        rank = (0 if key == name else 1 if key == name.upper() else 2, key)
        if name not in selected or rank < selected[name][0]:
            selected[name] = rank, value
    return {name: value for name, (_, value) in selected.items()}


def git_environment(options, environ=None):
    """Select only the lowercase libcurl variables for Git's HTTPS target.

    Layer these into the cleaned Git environment only for network operations.
    Do not pass proxy values on the command line or emit Git transport output
    if a proxy is selected, since that output can include proxy credentials.
    """
    if 'proxy-none' in options:
        return {}
    ambient = _ambient(os.environ if environ is None else environ)
    for name, value in (('proxy-https', options.get('proxy-https')),
                        ('proxy-any', options.get('proxy-any')),
                        ('HTTPS_PROXY', ambient.get('https_proxy')),
                        ('ANY_PROXY', ambient.get('any_proxy'))):
        if value:
            env = {'https_proxy': _check(value, name)}
            if bypass := ambient.get('no_proxy'):
                env['no_proxy'] = _check(bypass, 'NO_PROXY', bypass=True)
            return env
    return {}


def compiler_environment(options, environ):
    """Export canonical proxy variables to unsandboxed On1x project children."""
    if 'proxy-none' in options:
        return {'no_proxy': '*'}
    ambient = _ambient(environ)
    common = options.get('proxy-any') or ambient.get('any_proxy')
    result = {}
    for route in ('http', 'https'):
        key = f'{route}_proxy'
        value = (options.get(f'proxy-{route}') or options.get('proxy-any')
                 or ambient.get(key) or common)
        if value:
            result[key] = _check(value, key)
    if common:
        result['any_proxy'] = result['all_proxy'] = _check(common, 'ANY_PROXY')
    if bypass := ambient.get('no_proxy'):
        result['no_proxy'] = _check(bypass, 'NO_PROXY', bypass=True)
    return result
