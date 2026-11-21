"""Transport optimization policy to the linked compiler; it owns the pass inventory."""
import argparse
import re

PROFILES = ('default', 'debug', 'release')


def selector(value):
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_-]{0,127}', value):
        raise ValueError(f'invalid optimization selector: {value!r}')
    return value


class Select(argparse.Action):
    def __call__(self, parser, namespace, value, option_string=None):
        try:
            key = 'all' if self.nargs == 0 else selector(value)
        except ValueError as error:
            parser.error(str(error))
        choices = dict(getattr(namespace, self.dest) or {})
        enabled = option_string in ('--enable-opt', '--all-optimizations')
        if key in choices and choices[key] != enabled:
            parser.error(f'conflicting optimization selections: {key}')
        choices[key] = enabled
        setattr(namespace, self.dest, choices)


class Profile(argparse.Action):
    def __call__(self, parser, namespace, value, option_string=None):
        if getattr(namespace, self.dest) is not None:
            parser.error('specify only one optimization profile')
        setattr(namespace, self.dest, value)


def arguments(parser, *, alias=True):
    options = ['--opt-profile', '--profile'] if alias else ['--opt-profile']
    parser.add_argument(*options, choices=PROFILES, dest='opt_profile', action=Profile,
                        help='select On1x compiler optimizations independently of host C flags')
    parser.add_argument('--enable-opt', '--disable-opt', metavar='NAME', action=Select,
                        dest='opt_select', help='select a compiler optimization, or all (repeatable)')
    parser.add_argument('--all-optimizations', '--no-optimizations', action=Select, nargs=0,
                        dest='opt_select', help='shorthand for --enable-opt all or --disable-opt all')


def flags(profile, selections):
    result = ['--opt-profile', profile] if profile else []
    for name, enabled in sorted((selections or {}).items()):
        result.extend(('--enable-opt' if enabled else '--disable-opt', name))
    return result
