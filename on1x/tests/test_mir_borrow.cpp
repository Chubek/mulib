#include <on1x/backend/mir.hpp>
#include <iostream>
#include <stdexcept>

namespace {
using namespace on1x;

Unit checked(const std::string& text) {
    auto unit = parse(scan(text, "mir-borrow-hir.on1x"));
    check(unit);
    return unit;
}

void require(bool ok, const char* text) {
    if (!ok) throw std::runtime_error(text);
}

template<class Lower>
void reject(Lower lower, const Unit& unit) {
    try {
        (void)lower(unit, {});
    } catch (const Error& error) {
        if (error.diagnostic_id != "ON1X-REF") throw;
        require(error.where.file == "mir-borrow-hir.on1x" && error.where.line &&
                    error.where.column,
                "rejection lost its source location");
        return;
    }
    throw std::runtime_error("corrupted loan facts reached MIR");
}
}

int main() try {
    auto unit = checked("fn main(){let x=1;{let r=borrow mut x;"
                        "let s=borrow mut r;s:=s+1;println(s);}println(x);}");
    const auto ir = mir::lower(unit);
    require(mir::structurally_equal(ir, mir::lower_for_vm(unit)), "common scalar route drift");
    const auto roundtrip = mir::deserialize(mir::serialize(ir));
    require(mir::structurally_equal(ir, roundtrip), "slot lowering wire drift");
    require(ir.version == mir::legacy_format_version, "lexical loans bumped MIR wire version");
    const auto& fun = ir.functions.at(0);
    require(fun.slots.size() == 1 && fun.slots.front().type == Type::I64,
            "expected one typed root slot");
    int reads = 0, writes = 0;
    for (const auto& block : fun.blocks)
        for (const auto& op : block.instructions)
            if (op.op == mir::Op::Load || op.op == mir::Op::Store) {
                require(op.target == fun.slots.front().id, "alias did not use the root slot");
                require(op.effect_in && op.effect_out, "lost memory effect on a root access");
                (op.op == mir::Op::Load ? reads : writes)++;
            }
    require(reads == 3 && writes == 2, "borrow acquisition performed runtime memory effects");
    const auto evaluated = mir::evaluate(roundtrip);
    require(evaluated.status == mir::Status::Returned && evaluated.output == "2\n2\n",
            "nested alias store did not update its lexical root");

    auto& body = unit.functions.front().body.at(1).body;
    auto& reborrow = *body.at(1).value;
    // A conflicting acquisition from the owner while its exclusive loan lives.
    reborrow.children.at(0)->text = "x";
    reject(mir::lower, unit);
    reject(mir::lower_for_vm, unit);

    reborrow.children.at(0)->text = "r";
    // A shared reborrow cannot be used as the destination of :=.
    reborrow.borrow = BorrowMode::Shared;
    reject(mir::lower, unit);
    reject(mir::lower_for_vm, unit);

    auto shadow = checked("fn main(){let x=1;{let r=borrow mut x;"
                          "{let x=3;println(x);}r:=2;}println(x);}");
    const auto shadow_mir = mir::lower(shadow);
    require(shadow_mir.functions.front().slots.size() == 1,
            "unborrowed shadow spilled into a root slot");
    require(mir::evaluate(shadow_mir).output == "3\n2\n",
            "lexical shadow changed the resolved loan root");

    auto exchange = checked("fn next()->i64{return 2;} fn main(){let x=1;"
                            "let r=borrow mut x;println(ref_replace(r,next()));println(r);}");
    const auto exchange_mir = mir::lower(exchange);
    const auto& ops = exchange_mir.functions.at(1).blocks.front().instructions;
    constexpr mir::Op expected[] = {mir::Op::Constant, mir::Op::Store, mir::Op::Call,
        mir::Op::Load, mir::Op::Store, mir::Op::Print, mir::Op::Load, mir::Op::Print};
    require(ops.size() == sizeof(expected) / sizeof(expected[0]), "exchange lowered unexpected ops");
    for (std::size_t i = 0; i != ops.size(); ++i)
        require(ops[i].op == expected[i], "exchange did not evaluate RHS before old load and store");
    require(ops[3].target == ops[4].target && ops[3].target == ops[1].target &&
                ops[4].operands == std::vector<mir::Id>{ops[2].id} &&
                ops[5].operands == std::vector<mir::Id>{ops[3].id},
            "exchange did not replace its root and yield the old value");
    auto& rhs = *exchange.functions.at(1).body.at(2).value->children.at(0)->children.at(1);
    rhs.kind = Expr::Kind::Variable;
    rhs.reference = Expr::Reference::Local;
    rhs.text = "r";
    rhs.callee.reset();
    reject(mir::lower, exchange);
    reject(mir::lower_for_vm, exchange);

    auto tuple = checked(R"(
fn main() {
    let root=(1,(false,2));
    {
        let edit=borrow mut root;
        let next=borrow mut edit;
        let (a,(flag,b))=ref_replace(next,(3,(true,4)));
        next:=(5,(false,6));
        println(a+b);
        println(flag);
    }
    let (x,(flag,y))=root;
    println(x+y);
    println(flag);
}
)");
    const auto tuple_ir = mir::lower(tuple);
    require(mir::structurally_equal(tuple_ir, mir::lower_for_vm(tuple)), "tuple-loan route drift");
    const auto tuple_roundtrip = mir::deserialize(mir::serialize(tuple_ir));
    require(mir::structurally_equal(tuple_ir, tuple_roundtrip), "tuple slot lost on the wire");
    // Tuple slots can trigger an existing optional analysis directive. The
    // executable operations themselves still fit the original tuple profile.
    auto tuple_profile = tuple_roundtrip;
    tuple_profile.directives.clear();
    tuple_profile.version = mir::format_version;
    mir::verify(tuple_profile);
    const auto& tuple_fn = tuple_ir.functions.front();
    const auto& tuple_body = tuple.functions.front().body;
    require(tuple_fn.slots.size() == 1 &&
            tuple_fn.slots.front().type == tuple_body.at(0).value->type &&
            tuple_fn.slots.front().where.offset == tuple_body.at(0).where.offset,
            "expected one tuple slot at the original declaration");
    const auto& tuple_scope = tuple_body.at(1).body;
    const auto& tuple_exchange = *tuple_scope.at(2).value;
    const auto& tuple_assign = tuple_scope.at(3);
    std::vector<mir::Op> accesses;
    std::vector<std::size_t> sites;
    for (const auto& block : tuple_fn.blocks)
        for (const auto& op : block.instructions)
            if (op.op == mir::Op::Load || op.op == mir::Op::Store) {
                require(op.target == tuple_fn.slots.front().id && op.effect_in && op.effect_out,
                        "tuple alias did not use the effectful root slot");
                accesses.push_back(op.op);
                sites.push_back(op.where.offset);
            }
    require(accesses == std::vector{mir::Op::Store, mir::Op::Load, mir::Op::Store,
                                   mir::Op::Store, mir::Op::Load} &&
            sites == std::vector{tuple_body.at(0).where.offset,
                                 tuple_exchange.children.at(0)->where.offset,
                                 tuple_exchange.where.offset, tuple_assign.where.offset,
                                 tuple_body.at(2).value->where.offset},
            "tuple alias lost source access identity or evaluated borrow acquisition");
    const auto tuple_result = mir::evaluate(tuple_roundtrip);
    require(tuple_result.status == mir::Status::Returned &&
            tuple_result.output == "3\nfalse\n11\nfalse\n",
            "tuple load was not a snapshot or replacement did not update the root");
    tuple.functions.front().body.at(1).body.at(1).value->borrow = BorrowMode::Shared;
    reject(mir::lower, tuple);
    reject(mir::lower_for_vm, tuple);

    std::cout << "MIR scalar and tuple root slots, wire round-trip and malformed HIR gates PASS\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
