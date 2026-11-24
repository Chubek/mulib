#!/usr/bin/env python3
"""Run scalar generator and C/C++ differential tests; requires parser path."""
from pathlib import Path
import argparse, json, os, subprocess, sys, tempfile

DIR=Path(__file__).resolve().parents[1]
ON1X=DIR.parents[1]
SPEC=ON1X/'stdlib/ffi/mulib/clamp.ffi'
GEN=DIR/'tools/ffic.py'

def run(argv, ok=True, expected=None):
    p=subprocess.run([str(x) for x in argv],capture_output=True,text=True,timeout=30)
    if expected is not None:
        assert p.returncode==expected, (argv,p.returncode,p.stdout,p.stderr)
    elif ok:
        assert p.returncode==0,(argv,p.returncode,p.stdout,p.stderr)
    else:
        assert p.returncode!=0,(argv,p.stdout,p.stderr)
    return p

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--parser',required=True,type=Path)
    ap.add_argument('--cc',default='cc'); ap.add_argument('--cxx',default='c++')
    a=ap.parse_args(); parser=a.parser.resolve()
    fixture=(DIR/'tests/fixture_parser.py').resolve()==parser
    print('TEST PARSER:', 'test-only Lark grammar projection (not LLSynth)' if fixture else str(parser))
    with tempfile.TemporaryDirectory() as tmp:
        w=Path(tmp); hdr=w/'on1x_ffi_rules.hpp'; manifest=w/'manifest.json'
        def gen(*src,ok=True):
            return run([sys.executable,'-B',GEN,'--parser',parser,'--header',hdr,'--manifest',manifest,*src],ok)
        gen(SPEC)
        before=(hdr.read_bytes(),manifest.read_bytes())
        gen(SPEC); assert before==(hdr.read_bytes(),manifest.read_bytes())
        data=json.loads(manifest.read_text()); assert not data['grants_safe_admission']; assert len(data['bindings'])==3
        assert [r for r in data['bindings'] if r['member']=='i64'][0]['guard']==[1,2]
        source=SPEC.read_text()
        cases=[source.replace('ffi 1;','ffi 2;'),source.replace('mulib-clamp.h','../mulib-clamp.h'),
               source.replace('requires low <= high','requires bogus <= high'),source.replace('b: i64','a: i64'),
               source.replace('-> i64','-> u64'),source.replace('mulib_clamp_sub_i64','mulib_clamp_add_i64'),
               source.replace('fn sub_i64','fn add_i64'),source.replace('provider mulib','provider shell'),
               source.replace('requires low <= high','requires low < high'),source+'garbage']
        for i,bad in enumerate(cases):
            p=w/f'invalid{i}.ffi'; p.write_text(bad); gen(p,ok=False)
            assert before==(hdr.read_bytes(),manifest.read_bytes()),'invalid source replaced output'
        gen(SPEC,SPEC,ok=False)
        assert before==(hdr.read_bytes(),manifest.read_bytes())
        # A second module demonstrates that C++ implementation contains no symbol switch.
        inline=w/'example.ffi'; inline.write_text('''ffi 1;
foreign mulib.example {
 provider mulib "example"; header "mulib-example.h";
 inline fn increment(x: i64) -> i64 = c::mulib_example_increment;
 fn zero() -> i64 = c::mulib_example_zero;
}''')
        gen(SPEC,inline)
        (w/'mulib-example.h').write_text('''#include <stdint.h>
static inline int64_t mulib_example_increment(int64_t x) {return x==INT64_MAX?x:x+1;}
static inline int64_t mulib_example_zero(void) {return 0;}
''')
        harness=w/'emit.cpp'; harness.write_text(r'''#include <on1x/ffi_scalar.hpp>
#include <cassert>
#include <iostream>
#include <set>
#include <vector>
int main() {
 using namespace on1x::ffi;
 assert(scalar_bindings().size()==5);
 assert(!find_member("mulib.example","add_i64"));
 assert(!find_scalar("nope"));
 auto* b=find_member("mulib.clamp","i64"); assert(b&&b->arity==3);
 auto guard=scalar_guard(*b,std::vector<std::string>{"a","lo","hi"});
 assert(guard=="on1x_assert(lo <= hi);\n");
 try { (void)scalar_guard(*b,std::vector<std::string>{"a"}); return 1; } catch(const std::invalid_argument&) {}
 std::set<std::string_view> headers;
 std::cout<<"#include <stdlib.h>\n#include <assert.h>\n#include <stdint.h>\n";
 for(const auto& binding:scalar_bindings()) if(headers.insert(binding.header).second) std::cout<<scalar_header(binding);
 for(const auto& binding:scalar_bindings()) std::cout<<scalar_wrapper(binding,"wrap_"+std::string(binding.member));
 std::cout<<"static void on1x_assert(int test) { if (!test) exit(70); }\n";
 std::cout<<"static int64_t checked(int64_t a,int64_t lo,int64_t hi) {\n"<<guard<<"return wrap_i64(a,lo,hi);}\n";
 std::cout<<R"C(int main(int argc,char **argv) {
 (void)argv;
 if(argc>1) return (int)checked(0,2,1);
 assert(wrap_increment(41)==42); assert(wrap_zero()==0);
 int64_t vals[]={INT64_MIN,INT64_MIN+1,-42,-1,0,1,42,INT64_MAX-1,INT64_MAX};
 for(unsigned i=0;i<9;i++) for(unsigned j=0;j<9;j++) {
   assert(wrap_add_i64(vals[i],vals[j])==mulib_clamp_add_i64(vals[i],vals[j]));
   assert(wrap_sub_i64(vals[i],vals[j])==mulib_clamp_sub_i64(vals[i],vals[j]));
   for(unsigned k=j;k<9;k++) assert(checked(vals[i],vals[j],vals[k])==mulib_clamp_i64(vals[i],vals[j],vals[k]));
 }
 assert(wrap_add_i64(INT64_MAX,1)==INT64_MAX);
 assert(wrap_sub_i64(INT64_MIN,1)==INT64_MIN);
 return 0;
})C";
}''')
        run([a.cxx,'-std=c++20','-Wall','-Wextra','-Werror','-I'+str(ON1X/'include'),'-I'+str(w),DIR/'ffi.cpp',harness,'-o',w/'emit'])
        c=w/'client.c'; c.write_text(run([w/'emit']).stdout+"\n")
        upstream=ON1X.parent/'include'; inc=upstream if (upstream/'mulib-clamp.h').is_file() else DIR/'tests/fixtures'
        print('C header:',inc/'mulib-clamp.h')
        impl=w/'implementation.c'; impl.write_text('#define MULIB_IMPL_CLAMP\n#include <mulib-clamp.h>\n')
        flags=[a.cc,'-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(inc),'-I'+str(w)]
        run([*flags,c,impl,'-o',w/'client']); run([w/'client']); run([w/'client','bad'],expected=70)
        # Compile the same adapter with a deliberately incompatible declaration.
        wrong=w/'wrong'; wrong.mkdir()
        # Robust control independent of upstream whitespace.
        (wrong/'mulib-clamp.h').write_text('#include <stdint.h>\nint64_t mulib_clamp_i64(int64_t,int64_t,int64_t);\nint32_t mulib_clamp_add_i64(int64_t,int64_t);\nint64_t mulib_clamp_sub_i64(int64_t,int64_t);\n')
        result=run([a.cc,'-std=c11','-I'+str(wrong),'-I'+str(w),'-c',c,'-o',w/'wrong.o'],ok=False)
        assert 'prototype drift' in result.stderr,result.stderr
        print('PASS: deterministic descriptors, 11 invalid input cases, generic lookup, 567 forwarding comparisons, guard exit 70, wrong prototype rejection.')
    return 0
if __name__=='__main__': raise SystemExit(main())
