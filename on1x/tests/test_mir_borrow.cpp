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

    std::cout << "MIR scalar root slot, wire round-trip and malformed HIR gates PASS\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
