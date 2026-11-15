"""New pure libraries and canonical MuLIB bridge execution; no external network."""
import hashlib
import importlib.util
import json
import os
import socket
import sys
import threading
from support import NativeTests,run
t=NativeTests();helper=t.root/'tools/on1x-build.py'
def build(name,text,options=()):
    source=t.source(name,text);exe=t.work/name
    p=run([sys.executable,helper,source,'--on1xc',t.compiler,'--cc',t.cc,'-o',exe,
           *options,*['--cflag='+f for f in t.flags]])
    assert p.returncode==0,(name,p.stderr)
    return exe
def execute(exe,expected):
    p=run([exe]);assert p.returncode==0 and p.stdout==expected,(exe,p.stdout,p.stderr)

execute(build('pure','''import std.base.result as R;import std.base.option as O;
import std.base.list as L;import std.base.string as S;import std.batteries.map as M;
import std.graph.traversal as G;import std.parsec.parsec as P;
fn main(){
 let fail=fn(x:sexpr)->sexpr{tool_fail("callback must not execute");return x;};
 assert(!R.base_result_ok(R.basis_result_map(R.base_error("no"),fail)));
 assert(!O.base_is_some(O.basis_base_option_map(O.base_none(),fail)));
 let ok=R.basis_result_and_then(R.base_ok(sx_string("yes")),R.base_ok);
 assert(sx_text(R.base_result_payload(ok))=="yes");
 let map=M.batteries_map_put(M.batteries_map_put(M.batteries_map_empty(),"x",sx_string("old")),"x",sx_string("new"));
 let(found,v)=M.basis_map_find(map,"x");assert(found&&sx_text(v)=="new");
 assert(!M.batteries_map_has(M.basis_map_remove(map,"x"),"x"));
 let edges=G.graph_add(G.graph_add(G.graph_add(G.graph_empty(),"a","b"),"a","b"),"b","a");
 assert(L.base_list_length(G.graph_bfs(edges,"a"))==2);
 let(a,n,end)=P.basis_parsec_int("-9223372036854775808!",0);assert(a&&n==(-9223372036854775807-1)&&end==20);
 let(b,unused,rollback)=P.basis_parsec_int("9223372036854775808",0);assert(!b&&rollback==0);
 let(c,x,stop)=P.basis_parsec_int("+42!",0);assert(c&&x==42&&stop==3);
 var i=0;while i<2000{assert(!S.base_string_contains("abcdefghijklmnopqrstuvwxyz","no match"));assert(P.parsec_literal("abcd",1,"bc"));i=i+1;}
 println("pure extensions OK");}''',['--cflag=-DON1X_DATA_MAX_POOL=65536']),'pure extensions OK\n')

report=t.work/'bridge-plan.json'
exe=build('bridge','''import std.json.json as J;import std.yaml.yaml as Y;import std.toml.toml as T;
import std.ini.ini as I;import std.crypto.crypto as C;import std.unix.unix as U;
fn main(){
 let(a,json)=J.basis_json_normalize(" {\\"answer\\":42} ");assert(a==0&&json=="{\\"answer\\":42}");
 let(b,bad)=J.basis_json_normalize("invalid");assert(b==-2&&bad=="");
 let(ok,length)=J.basis_json_array_length("[1,2,3]");assert(ok&&length==3);
 let(no,ignored)=J.basis_json_array_length("{}");assert(!no);
 let(c,yaml)=Y.basis_yaml_as_json("answer: 42\\n");assert(c==0&&J.json_valid(yaml));
 let(d,toml)=T.basis_toml_as_json("answer = 42\\n");assert(d==0&&J.json_valid(toml));
 let(e,empty)=I.basis_ini_get("[s]\\nx=\\n","s","x");assert(e==0&&empty=="");
 let(f,missing)=I.basis_ini_get("[s]\\nx=1\\n","s","absent");assert(f==1&&missing=="");
 let(g,invalid)=I.basis_ini_get("[s]\\nx=first\\nx=last\\ninvalid\\n","s","x");assert(g==-2&&invalid=="");
 let(h,first)=I.basis_ini_get("[s]\\nx=first\\nx=last\\n","s","x");assert(h==0&&first=="first");
 let(k,hash)=C.basis_crypto_blake2s("abc");assert(k==0);println(hash);
 let(l,blake)=C.basis_crypto_blake3("");assert(l==0);println(blake);
 assert(U.unix_pid()>0&&U.unix_uid()>=0);println("bridge OK");}''',['--mulib-plan',report])
execute(exe,hashlib.blake2s(b'abc').hexdigest()+'\naf1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262\nbridge OK\n')
example=t.work/'example'
p=run([sys.executable,helper,t.root/'examples/stdlib_extensions.on1x','--on1xc',t.compiler,'--cc',t.cc,
       '-o',example,*['--cflag='+f for f in t.flags]])
assert p.returncode==0,p.stderr
execute(example,'42\nrecoverable parse failure\n')
plan=json.loads(report.read_text());assert plan['plan']['implementation_translation_units']==1
assert {'json','yaml','toml','ini','cryptohash'}<={x['name'] for x in plan['plan']['modules']}
assert str(t.root/'stdlib/_bridge/mulib_bridge.h') in plan['inputs']

spec=importlib.util.spec_from_file_location('house',t.root/'std/mulib/housekeeping.py')
house=importlib.util.module_from_spec(spec);spec.loader.exec_module(house)
implementation=t.work/'ini_impl.c';implementation.write_text(house.implementation(t.root.parent,house.plan(t.root.parent,['ini'])))
allocation=t.work/'bridge_allocation'
p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root.parent/'include',
       t.root/'tests/test_stdlib_bridge_allocation.c',implementation,*t.flags,'-o',allocation])
assert p.returncode==0,p.stderr
execute(allocation,'')
for group in ['JSON','YAML','TOML','INI','CRYPTO','SOCKET','UNIX','']:
    flags=['-DON1X_BRIDGE_'+group] if group else []
    p=run([t.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I',t.root.parent/'include',
           *flags,*t.flags,'-c',t.root/'stdlib/_bridge/mulib_bridge.c','-o',t.work/('group_'+group+'.o')])
    assert p.returncode==0,(group,p.stderr)

# Invalid shapes are rejected instead of silently truncating or accepting tags.
for name,statement,message in [
 ('bad_list','L.base_list_length(sx_symbol("atom"));','list is not proper'),
 ('bad_result','R.base_result_ok(sx_cons(sx_symbol("wrong"),sx_cons(sx_nil(),sx_nil())));','invalid result tag'),
 ('bad_option','O.base_is_some(sx_symbol("some"));','two-element list'),
 ('bad_graph','G.graph_bfs(sx_cons(sx_nil(),sx_nil()),"a");','two-element list'),
 ('bad_position','P.parsec_space("x",2);','position outside input'),
]:
    program='import std.base.list as L;import std.base.result as R;import std.base.option as O;import std.graph.traversal as G;import std.parsec.parsec as P;fn main(){'+statement+'}'
    p=run([build(name,program)],env={**os.environ,'LSAN_OPTIONS':'detect_leaks=0'})
    assert p.returncode==73 and message in p.stderr,(name,p.returncode,p.stderr)

# Local TCP server: binary receive, EOF, invalid/stale handles and socket cleanup.
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(20)
port=listener.getsockname()[1];errors=[]
def serve():
    try:
        conn,_=listener.accept()
        with conn:
            conn.settimeout(10);data=b''
            while len(data)<4:data+=conn.recv(4-len(data))
            assert data==b'ping';conn.sendall(b'A\0\xff')
        conn,_=listener.accept()
        with conn:
            conn.sendall(b'\0')
    except Exception as e:errors.append(e)
    finally:listener.close()
network=build('network','''import std.network.network as N;
fn main(){
 let handle=N.socket_connect("127.0.0.1",PORT);assert(handle>0);
 assert(N.network_send_all(handle,"ping"));
 var hex="";while text_len(hex)<6{let(status,chunk)=N.basis_socket_receive_hex(handle,16);assert(status==0);hex=text_concat(hex,chunk);}
 assert(hex=="4100ff");let(eof,empty)=N.basis_socket_receive(handle,16);assert(eof==2&&empty=="");
 assert(N.socket_close(handle));assert(!N.socket_close(handle));
 let(bad,value)=N.basis_socket_receive_hex(handle,16);assert(bad==-1&&value=="");
 let second=N.socket_connect("127.0.0.1",PORT);assert(second>0&&second!=handle);
 assert(!N.socket_close(handle));
 assert(N.on1x_mulib_socket_send_from(second,"x",2)==-1);
 let(binary,not_text)=N.basis_socket_receive(second,16);assert(binary==-6&&not_text=="");
 assert(N.socket_close(second));
 println("socket OK");}'''.replace('PORT',str(port)))
thread=threading.Thread(target=serve);thread.start()
try:execute(network,'socket OK\n')
finally:thread.join(25);listener.close()
assert not thread.is_alive() and not errors,errors
print('Pure extensions, bounded MuLIB adapters, recoverable errors and local binary sockets: PASS')
