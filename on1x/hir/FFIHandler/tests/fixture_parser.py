"""Test-only parse-tree fixture from the .llsynth grammar using Lark.
NOT LLSynth, NOT a production fallback. Used only for offline unit tests.
Real integration must use the generated parser and --parser.
"""
from pathlib import Path
import re
from types import SimpleNamespace
from lark import Lark, Tree, Token
from lark.exceptions import LarkError

class LanguageError(ValueError): pass

spec = (Path(__file__).resolve().parents[1]/'FFIHandler.llsynth').read_text()
rules = dict(re.findall(r'^\.rule (\w+)\n\x27syntax: (.*)$', spec, re.M))
assert rules
names = {k: 'r_'+k.lower() for k in rules}
reverse = {v:k for k,v in names.items()}
lines=[]
for name, rhs in rules.items():
    # Only unquoted grammar references, not literal tokens.
    pieces=re.split(r'("[^"\n]*")',rhs)
    for i in range(0,len(pieces),2):
        pieces[i]=re.sub(r'\b\w+\b',lambda m:names.get(m[0],'' if m[0]=='EOF' else m[0]),pieces[i])
    lines.append(names[name]+': '+''.join(pieces))
lines += [r'NAME: /[A-Za-z_][A-Za-z_0-9]*/',r'NUMBER: /[0-9]+/',r'STRING: /"([^"\\\r\n]|\\["\\nrt])*"/',r'%ignore /[ \t\r\n]+/',r'%ignore /#[^\n]*/']
parser=Lark('\n'.join(lines),start='r_start',parser='lalr',keep_all_tokens=True,maybe_placeholders=False)

def convert(value):
    if isinstance(value,Token):
        return SimpleNamespace(symbol=value.type,children=[],token=SimpleNamespace(lexeme=str(value)))
    return SimpleNamespace(symbol=reverse[str(value.data)],children=[convert(x) for x in value.children],token=None)

def parse(text):
    try: return convert(parser.parse(text))
    except LarkError as exc: raise LanguageError(str(exc)) from exc
