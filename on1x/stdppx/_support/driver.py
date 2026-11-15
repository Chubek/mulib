"""Run one explicitly selected transform in On1x's verified S-expression v1 protocol."""
import os
import sys
import traceback
from .sexpr import loads,dumps,MAX_BYTES
from .ast import Schema,name,extensions_left
from .passes import PROCESSORS

def serve(processor):
    try:
        if processor not in PROCESSORS:raise ValueError('unknown processor')
        if os.environ.get('ON1X_PPX_PROTOCOL')!='1':raise ValueError('ON1X_PPX_PROTOCOL must equal 1')
        label=os.environ.get('ON1X_PPX_LABEL','')
        if not name(label) or len(label)>128:raise ValueError('missing or invalid ON1X_PPX_LABEL')
        schema_path=os.environ.get('ON1X_PPX_SCHEMA')
        schema=Schema(schema_path if schema_path else None)
        src=sys.stdin.buffer.read(MAX_BYTES+1)
        if len(src)>MAX_BYTES:raise ValueError('PPX input exceeds 4 MiB')
        tree=loads(src);schema.validate(tree)
        result=PROCESSORS[processor](tree,label)
        schema.validate(result)
        if processor not in ('identity','fold') and extensions_left(result,label):
            raise ValueError('unconsumed '+label+' extension/attribute remains')
        sys.stdout.buffer.write(dumps(result))
        sys.stdout.buffer.flush()
    except (ValueError,IndexError,KeyError,OverflowError,OSError,RecursionError,UnicodeError,TypeError) as error:
        print('On1x stdppx/'+processor+': '+str(error),file=sys.stderr)
        raise SystemExit(1)
