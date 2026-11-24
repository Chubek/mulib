#include "../../analysis_directives.hpp"
#include <on1x/clone.hpp>
#include <iostream>

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
    return 0;
} catch (const std::exception& e) {
    std::cerr<<e.what()<<'\n';
    return 1;
}
