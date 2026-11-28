"""Bounded byte-preserving On1x S-expression protocol codec (v1).

The wire grammar is not the SML reader and has no executable forms.
Python str maps 1:1 to bytes (Latin-1). Atom denotes an unquoted token.
"""
class Atom(str):
    pass

MAX_BYTES=4*1024*1024
MAX_NODES=262144
MAX_DEPTH=256


def loads(data):
    if isinstance(data, bytes):
        try: source=data.decode('ascii')
        except UnicodeError as e: raise ValueError('wire must be ASCII: '+str(e)) from e
    else: source=data
    if len(source)>MAX_BYTES: raise ValueError('input byte limit exceeded')
    pos=0; count=0
    def space():
        nonlocal pos
        while pos<len(source) and source[pos] in ' \t\n\r': pos+=1
    def read(depth):
        nonlocal pos,count
        count+=1
        if count>MAX_NODES or depth>=MAX_DEPTH: raise ValueError('input node/depth limit exceeded')
        space()
        if pos>=len(source): raise ValueError('unexpected end of wire input')
        char=source[pos];pos+=1
        if char=='(':
            arr=[]
            while True:
                space()
                if pos==len(source):raise ValueError('unclosed list')
                if source[pos]==')': pos+=1;return arr
                arr.append(read(depth+1))
        if char==')': raise ValueError('unexpected closing parenthesis')
        if char=='"':
            result=[]
            while pos<len(source):
                char=source[pos];pos+=1
                if char=='"':return ''.join(result)
                if char=='\\':
                    if pos>=len(source):raise ValueError('unterminated escape')
                    char=source[pos];pos+=1
                    if char=='x':
                        digits=source[pos:pos+2]
                        if len(digits)!=2 or any(c not in '0123456789abcdefABCDEF' for c in digits):raise ValueError('invalid hex escape')
                        char=chr(int(digits,16));pos+=2
                    elif char in 'nrt':char={'n':'\n','r':'\r','t':'\t'}[char]
                    elif char not in ('\\','"'):raise ValueError('unknown escape')
                elif not (32<=ord(char)<=126):raise ValueError('unescaped nonprintable byte')
                result.append(char)
            raise ValueError('unclosed quoted string')
        start=pos-1
        while pos<len(source) and source[pos] not in '() \t\r\n':pos+=1
        token=source[start:pos]
        if not token or any(not 33<=ord(c)<=126 or c=='"' for c in token):raise ValueError('invalid symbol')
        return Atom(token)
    result=read(0);space()
    if pos!=len(source):raise ValueError('trailing wire input')
    return result


def dumps(tree):
    out=[]; count=0; total=0
    def emit(data):
        nonlocal total
        total+=len(data)
        if total>MAX_BYTES:raise ValueError('output byte limit exceeded')
        out.append(data)
    def visit(node,depth):
        nonlocal count
        count+=1
        if count>MAX_NODES or depth>=MAX_DEPTH:raise ValueError('output node/depth limit exceeded')
        if isinstance(node,list):
            emit('(')
            for i,child in enumerate(node):
                if i:emit(' ')
                visit(child,depth+1)
            emit(')')
        elif isinstance(node,Atom):
            if not node or any(not 33<=ord(c)<=126 or c in '()"' for c in node):raise ValueError('invalid output atom')
            emit(node)
        elif type(node)==str:
            emit('"')
            for c in node:
                if ord(c)>255:raise ValueError('wire strings must be Latin-1 bytes')
                if c in '\\"':emit('\\'+c)
                elif 32<=ord(c)<=126:emit(c)
                else:emit(f'\\x{ord(c):02x}')
            emit('"')
        else: raise ValueError('output must contain only lists, symbols, and byte strings')
    visit(tree,0);emit('\n')
    return ''.join(out).encode('ascii')
