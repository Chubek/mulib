"""Bounded tty adapter, canonical kernel plan, and actual On1x facade."""
import importlib.util
import json
import os
import struct
import sys

from support import NativeTests, run


t = NativeTests()
helper = t.root / 'tools/on1x-build.py'


def build(name, text=None, source=None, options=()):
    if source is None:
        source = t.source(name, text)
    executable = t.work / name
    process = run([
        sys.executable, helper, source, '--on1xc', t.compiler, '--cc', t.cc,
        '-o', executable, *options, *['--cflag=' + flag for flag in t.flags],
    ])
    assert process.returncode == 0, (name, process.stderr)
    return executable


def execute(executable, expected, env=None):
    process = run([executable], env=env)
    assert process.returncode == 0, (
        executable, process.returncode, process.stdout, process.stderr)
    assert process.stdout == expected, (executable, process.stdout, expected)


plan_path = t.work / 'tty-plan.json'
program = build('tty_on1x', r'''
import std.tty.tty as T;
fn main(){
 let (cap_status,caps)=T.basis_tty_capabilities(1);
 assert(cap_status==0&&caps==64);
 assert(T.basis_tty_has_capability(caps,64));
 assert(T.basis_tty_rgb(171,205,239)==28036591);
 assert(T.basis_tty_rgb(256,0,0)==-2);
 assert(T.basis_tty_attributes(true,false,false,true,false,false,false,false)==9);
 let (render_status,text)=T.basis_tty_render(1,"safe",1,2,-1,true);
 assert(render_status==1&&text=="safe");
 let (open_status,decoder)=T.basis_tty_decoder_open();
 assert(open_status==0&&decoder>0);
 assert(T.basis_tty_decoder_feed_hex(decoder,"1b5b313b3541")==0);
 let (event_status,available,event_type,key,codepoint,modifiers,
      button,action,x,y,raw)=T.basis_tty_decoder_next(decoder,false);
 assert(event_status==0&&available&&event_type==0&&key==6&&codepoint==0);
 assert(modifiers==4&&button==0&&action==0&&x==0&&y==0);
 assert(T.basis_tty_event_type_name(event_type)=="key"&&
        T.basis_tty_key_name(key)=="up"&&T.basis_tty_has_modifier(modifiers,4));
 assert(raw=="1b5b313b3541");
 let (again_status,again,a,b,c,d,e,f,g,h,empty)=
   T.basis_tty_decoder_next(decoder,false);
 assert(again_status==3&&!again&&a==0&&b==0&&c==0&&d==0&&e==0&&
        f==0&&g==0&&h==0&&empty=="");
 assert(T.basis_tty_decoder_close(decoder)==0);
 assert(T.basis_tty_decoder_close(decoder)==-1);
 let (screen_status,screen)=T.basis_tty_vterm_open(5,2);
 assert(screen_status==0&&screen>0&&screen!=decoder);
 assert(T.basis_tty_vterm_feed_hex(screen,"1b5b33316d58")==0);
 let (cell_status,cell,foreground,background,attributes)=
   T.basis_tty_vterm_cell(screen,0,0);
 assert(cell_status==0&&cell==88&&foreground==1&&
        background==4294967295&&attributes==0);
 let (info_status,width,height,cursor_x,cursor_y,invalid,ignored,visible)=
   T.basis_tty_vterm_info(screen);
 assert(info_status==0&&width==5&&height==2&&cursor_x==1&&cursor_y==0&&
        invalid==0&&ignored==0&&visible==1);
 assert(T.basis_tty_vterm_feed_hex(screen,"not hex")==-1);
 assert(T.basis_tty_vterm_close(screen)==0);
 assert(T.basis_tty_vterm_close(screen)==-1);
 let (raw_status,raw_handle)=T.basis_tty_raw_begin(-1);
 assert(raw_status==-1&&raw_handle==-1);
 println("tty On1x OK");
}''', options=['--mulib-plan', plan_path])
execute(program, 'tty On1x OK\n',
        {**os.environ, 'TERM': 'xterm-256color', 'NO_COLOR': '1'})

if os.name == 'posix' and hasattr(os, 'openpty'):
    import fcntl
    import termios

    raw_program = build('tty_raw_on1x', r'''
import std.tty.tty as T;
fn main(){
 let (begin_status,handle)=T.basis_tty_raw_begin(0);
 assert(begin_status==0&&handle>0);
 let (size_status,columns,rows)=T.basis_tty_size(0);
 assert(size_status==0&&columns==80&&rows==24);
 assert(T.basis_tty_raw_end(handle)==0);
 println("tty raw On1x OK");
}''')
    master, slave = os.openpty()
    try:
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 80, 0, 0))
        before = termios.tcgetattr(slave)
        process = run([raw_program], stdin=slave)
        after = termios.tcgetattr(slave)
        assert process.returncode == 0 and process.stdout == 'tty raw On1x OK\n', (
            process.returncode, process.stdout, process.stderr)
        assert before == after
    finally:
        os.close(slave)
        os.close(master)

plan = json.loads(plan_path.read_text())
recipe = plan['plan']
assert recipe['implementation_translation_units'] == 1
modules = {module['name'] for module in recipe['modules']}
assert {'termutils', 'termkey', 'termstyle', 'terminfo', 'vterm',
        'grid', 'utf8', 'strbuf'} <= modules
assert str((t.root / 'stdlib/tty/runtime.c').resolve()) in plan['inputs']
assert str((t.root / 'stdlib/tty/runtime.h').resolve()) in plan['inputs']

spec = importlib.util.spec_from_file_location(
    'house', t.root / 'stdlib/mulib/housekeeping.py')
house = importlib.util.module_from_spec(spec)
spec.loader.exec_module(house)
native_plan = house.plan(
    t.root.parent, ['termutils', 'termkey', 'termstyle', 'vterm'])
implementation = t.work / 'tty_impl.c'
implementation.write_text(
    house.implementation(t.root.parent, native_plan), encoding='utf-8')
native_sources = [t.root.parent / path for path in native_plan['native_sources']]
native_links = ['-l' + name for name in native_plan['link_libraries']]

for name, sources in [
    ('tty_runtime', [
        t.root / 'tests/test_tty_runtime.c',
        t.root / 'stdlib/tty/runtime.c',
        implementation,
    ]),
    ('tty_allocation', [
        t.root / 'tests/test_tty_allocation.c',
        implementation,
    ]),
]:
    executable = t.work / name
    process = run([
        t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-I', t.root.parent / 'kernels', '-I', t.root / 'stdlib/tty',
        *sources, *native_sources, *t.flags, *native_links, '-o', executable,
    ])
    assert process.returncode == 0, (name, process.stderr)
    execute(executable, '')

runtime_source = (t.root / 'stdlib/tty/runtime.c').read_text(encoding='utf-8')
assert 'MULIB_IMPL_' not in runtime_source
object_file = t.work / 'tty_runtime_declarations.o'
process = run([
    t.cc, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
    '-I', t.root.parent / 'kernels', '-I', t.root / 'stdlib/tty',
    '-c', t.root / 'stdlib/tty/runtime.c', '-o', object_file, *t.flags,
])
assert process.returncode == 0, process.stderr

example = build('tty_example', source=t.root / 'examples/tty.on1x')
execute(example, 'terminal-safe text\n6\n88\n',
        {**os.environ, 'TERM': 'xterm-256color', 'NO_COLOR': '1'})

print('TTY policy, raw restoration, event decoding, vterm and On1x linkage: PASS')
