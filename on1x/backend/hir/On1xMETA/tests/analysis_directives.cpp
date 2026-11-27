#include "../../analysis_directives.hpp"
#include <on1x/backend/mir_directives.hpp>
#include <on1x/clone.hpp>
#include <algorithm>
#include <iostream>
#include <limits>

namespace {
namespace m=on1x::mir;
void require(bool value,const char* message) {
    if (!value) throw std::runtime_error(message);
}
on1x::Unit checked(const std::string& source) {
    auto unit=on1x::parse(on1x::scan(source,"analysis-directives.on1x"));
    on1x::check(unit);
    return unit;
}
void directive(const m::Directive& d,const char* kind,const char* action,m::Id function) {
    require(d.kind==kind && d.action==action && d.function==function &&
            d.backend.empty() && d.target.empty() && !d.speculative,
             "unexpected directive payload or scope");
}
const m::Function& function(const m::Module& module,std::string_view name) {
    const auto it=std::find_if(module.functions.begin(),module.functions.end(),
                               [&](const auto& f){return f.name==name;});
    require(it!=module.functions.end(),"missing lowered function");
    return *it;
}
std::size_t count(const m::Module& module,std::string_view name,m::Op op) {
    std::size_t n=0;
    for (const auto& block : function(module,name).blocks)
        for (const auto& inst : block.instructions)
            n+=inst.op==op;
    return n;
}
void scoped() {
    const auto unit=checked(R"(
fn typed_scratch(x:u8,t:(i64,bool))->(u8,(i64,bool)) {
    var word=x; var pair=t; return (word,pair);
}
fn float_scratch(x:f32,y:f64)->(f32,f64) {
    var a=x; var b=y; return (a,b);
}
fn cross_block(x:u8,b:bool)->u8 { var y=x; if b { y=1u8; } return y; }
fn store_only(x:f32)->f32 { var y=x; return x; }
fn redundant(x:i64) { var y=x; y=x; }
fn canonical(x:i64,b:bool)->(bool,i64) { return (b==!b,-(-x)); }
fn canonical_safe(x:i64)->(bool,i64) { let z=0; return (x>9223372036854775807,-z); }
fn bool_zero(x:bool)->bool { let y=false; return x==y; }
fn proven()->bool { let x=7; return x==0; }
fn unproved(x:i64)->bool { return x==0; }
fn main() {
    let (w,(i,flag))=typed_scratch(7u8,(42,true));
    println(w); println(i); println(flag);
    let (b,d)=canonical(i,true); println(b); println(d); println(unproved(0));
}
)");
    auto module=m::lower(unit);
    constexpr auto triple="x86_64-unknown-linux-gnu";
    const auto requests=[&](const char* action,std::string_view name,std::string_view backend,
                            std::string_view target,bool speculative) {
        const auto id=function(module,name).id;
        return std::count_if(module.directives.begin(),module.directives.end(),[&](const auto& d) {
            return d.backend==backend && d.target==target && d.kind=="rewrite" &&
                   d.action==action && d.function==id && d.speculative==speculative;
        });
    };
    require(module.version==m::directive_format_version,"new requests did not select v4");
    require(requests(m::memgraph_local_action,"typed_scratch","mir-c11",triple,false)==1 &&
            requests(m::memgraph_local_action,"typed_scratch","mir-c11",triple,true)==1 &&
            requests(m::memgraph_local_action,"float_scratch","mir-c11",triple,false)==1 &&
            requests(m::memgraph_local_action,"redundant","mir-c11",triple,true)==1,
            "MEMGraph omitted a typed safe or speculative candidate");
    for (const auto backend : {"mir-c11","mir-archon","mir-vm"}) {
        const auto target=std::string_view(backend)=="mir-vm"?"":triple;
        require(requests(m::meta_canonical_action,"canonical",backend,target,false)==1 &&
                requests(m::meta_canonical_action,"canonical",backend,target,true)==1 &&
                requests(m::meta_canonical_action,"canonical_safe",backend,target,false)==1 &&
                requests(m::meta_canonical_action,"proven",backend,target,true)==1 &&
                requests(m::meta_canonical_action,"unproved",backend,target,true)==1,
                "On1xMETA omitted a mode or selected an unexpected product");
    }
    require(module.directives.size()==19,"unexpected requests for unrelated functions or products");
    const auto wire=m::serialize(module);
    require(m::serialize(m::lower_for_vm(unit))==wire,"VM lowering lost scoped requests");
    on1x::hir::append_analysis_directives(unit,module);
    require(m::serialize(module)==wire,"scoped analysis reprojection was not idempotent");
    const auto restored=m::deserialize(wire);
    require(m::structurally_equal(module,restored),"scoped wire round trip changed MIR");

    const auto unmatched=m::specialize(restored,"mir-c11","x86_64-linux-gnu",true);
    const auto no_target=m::specialize(restored,"mir-c11","",true);
    const auto wrong_case=m::specialize(restored,"MIR-C11",triple,true);
    require(m::structurally_equal(unmatched,no_target) && m::structurally_equal(unmatched,wrong_case) &&
            count(unmatched,"typed_scratch",m::Op::Load)==2 &&
            count(unmatched,"canonical",m::Op::Eq)==1 && count(unmatched,"canonical",m::Op::Neg)==2 &&
            count(unmatched,"canonical_safe",m::Op::Gt)==1 && count(unmatched,"canonical_safe",m::Op::Neg)==1,
            "an unmatched selector activated a scoped action");
    const auto safe=m::specialize(restored,"mir-c11",triple);
    require(count(safe,"typed_scratch",m::Op::Load)==1 && count(safe,"float_scratch",m::Op::Load)==0 &&
            count(safe,"redundant",m::Op::Store)==2 && count(safe,"canonical",m::Op::Eq)==0 &&
            count(safe,"canonical",m::Op::Neg)==2 && count(safe,"canonical_safe",m::Op::Gt)==0 &&
            count(safe,"canonical_safe",m::Op::Neg)==0,
            "safe actions did not run independently of speculation");
    const auto both=m::specialize(restored,"mir-c11",triple,true);
    require(count(both,"typed_scratch",m::Op::Load)==0 && count(both,"canonical",m::Op::Neg)==1 &&
            count(both,"proven",m::Op::Eq)==0 && count(both,"unproved",m::Op::Eq)==1,
            "speculative actions skipped a proof or accepted a hint as proof");
    auto spec_requests=restored;
    std::erase_if(spec_requests.directives,[](const auto& d){return !d.speculative;});
    const auto speculative=m::specialize(spec_requests,"mir-c11",triple,true);
    require(count(speculative,"typed_scratch",m::Op::Load)==1 &&
            count(speculative,"float_scratch",m::Op::Load)==2 &&
            count(speculative,"redundant",m::Op::Store)==1 &&
            count(speculative,"canonical",m::Op::Eq)==1 && count(speculative,"canonical",m::Op::Neg)==1,
            "speculative-only requests activated safe rules");
    const auto archon=m::specialize(restored,"mir-archon",triple,true);
    const auto vm=m::specialize(restored,"mir-vm","",true);
    require(m::structurally_equal(vm,m::specialize(restored,"mir-vm","portable-test-target",true)),
            "VM wildcard target did not match a nonempty product target");
    for (const auto* selected : {&archon,&vm})
        require(count(*selected,"canonical",m::Op::Eq)==0 && count(*selected,"canonical",m::Op::Neg)==1 &&
                count(*selected,"typed_scratch",m::Op::Load)==2,
                "product did not select its META requests independently of MEMGraph");
    for (const auto* selected : {&restored,&unmatched,&safe,&both,&speculative,&archon,&vm}) {
        const auto result=m::evaluate(*selected);
        require(result.status==m::Status::Returned && result.output=="7\n42\ntrue\nfalse\n42\ntrue\n",
                "specialization changed the result of source lowering");
        const auto trap=m::evaluate(*selected,"canonical",{std::numeric_limits<std::int64_t>::min(),1});
        require(trap.status==m::Status::Trapped && trap.diagnostic.find("negation overflow")!=std::string::npos,
                "specialization removed a checked negation trap");
    }
}
}

int main() try {
    const auto unit=checked(R"(
fn scratch(x:i64)->i64 { var y=x; y=y+2; return y; }
fn noisy(x:i64)->i64 { var y=x; println(0); return y; }
fn branch(x:i64,b:bool)->i64 { var y=x; if b {y=y+2;} return y; }
fn unused(x:i64)->i64 { var y=x; return x; }
fn mark(x:i64)->i64 { println(x); return x; }
fn identity(x:i64)->i64 { return mark(x)/1; }
fn comparison(x:bool,y:bool)->bool { return !x==!y; }
fn main() { println(scratch(identity(40))); }
)");
    auto module=m::lower(unit);
    require(module.version==m::directive_format_version,"evidence did not select directive format");
    require(module.directives.size()==4,"wrong number of directive facts");
    directive(module.directives[0],"hint","local-memory",module.functions[0].id);
    directive(module.directives[1],"hint","canonicalized",module.functions[5].id);
    directive(module.directives[2],"rewrite","fold-identity",module.functions[5].id);
    directive(module.directives[3],"hint","canonicalized",module.functions[6].id);

    const auto wire=m::serialize(module);
    on1x::hir::append_analysis_directives(unit,module);
    require(m::serialize(module)==wire,"reprojection changed directive order or duplicated facts");
    const auto restored=m::deserialize(wire);
    require(m::structurally_equal(module,restored),"analysis directive round-trip mismatch");
    require(m::evaluate(restored).output=="40\n42\n","directive emission changed observable effects");

    std::size_t budget=65536;
    const auto cloned=on1x::clone_unit(unit,budget);
    require(m::serialize(m::lower(cloned))==wire,"clone lost On1xMETA pass history");
    require(m::serialize(m::lower_for_vm(unit))==wire,"VM lowering lost analysis evidence");

    const auto plain=m::lower(checked(R"(
fn local_memory(x:i64)->i64 { return x; }
fn canonicalized(x:i64)->i64 { return x*0; }
fn fold_identity(x:i64)->i64 { return x/2; }
fn ordinary_desugar(x:i64,y:i64)->bool { return x!=y; }
fn main() {}
)"));
    require(plain.version==m::legacy_format_version && plain.directives.empty(),
            "plain scalar or name-based false positive");
    const auto floating=m::lower(checked("fn main(){for _ in 0 .. 4 { println(1.0+0.0); }}"));
    require(floating.version==m::format_version && floating.directives.empty(),
            "integer identity rule applied to IEEE operands");
    const auto numeric=m::lower(checked("fn main(){println(1u8+0u8);}"));
    require(numeric.version==m::numeric_format_version && numeric.directives.empty(),
            "integer identity rule applied to unsigned operands");
    scoped();
    return 0;
} catch (const std::exception& e) {
    std::cerr<<e.what()<<'\n';
    return 1;
}
