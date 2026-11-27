#include "../lexical_slots.hpp"
#include <on1x/canonical/control.hpp>
#include <iostream>
#include <string_view>

namespace {
using namespace on1x;
using memgraph::plan_lexical_slots;

void require(bool condition, const char* failure) {
    if (!condition) throw std::runtime_error(failure);
}

Unit checked(const char* text) {
    auto unit = parse(scan(text, "lexical-slots.on1x"));
    check(unit);
    canonical::verify_control(unit);
    return unit;
}

void rejected(const Unit& unit, std::string_view id, const Location& where) {
    try {
        (void)plan_lexical_slots(unit);
    } catch (const Error& error) {
        if (error.diagnostic_id != id) throw;
        require(error.where.file == where.file && error.where.offset == where.offset,
                "plan rejection lost the canonical location");
        return;
    }
    throw std::runtime_error("invalid HIR produced a lexical plan");
}

void identity_and_order() {
    auto unit = checked(R"(
fn test(x:i64)->i64 {
    let slot = borrow mut x;
    {
        let x = 21;
        let slot = borrow x;
        println(slot);
    }
    {
        let child = borrow mut slot;
        child := child + 1;
    }
    return slot;
}
fn main() { println(test(10)); }
)");
    const auto original = emit_c(unit, "lexical-slots.on1x");
    const auto& function = unit.functions.at(0);
    const auto& outer = function.body.at(0);
    const auto& shadow = function.body.at(1).body;
    const auto& reborrow = function.body.at(2).body;
    const auto& ret = *function.body.at(3).value;
    const auto& parameter = function.params.at(0);
    const auto plan = plan_lexical_slots(unit);
    const auto* root_alias = plan.binding(outer);
    const auto* shadow_alias = plan.binding(shadow.at(1));
    const auto* child = plan.binding(reborrow.at(0));
    require(root_alias && shadow_alias && child, "missing scalar aliases");
    require(root_alias->binding == &outer && root_alias->source == outer.value->children.at(0).get() &&
                root_alias->type == Type::I64 && root_alias->mode == BorrowMode::Exclusive &&
                root_alias->root_parameter == &parameter && !root_alias->root_statement &&
                !root_alias->parent_binding && plan.is_root(parameter) && !plan.is_root(outer),
            "lost parameter origin, mode, or original operand");
    require(shadow_alias->root_statement == &shadow.at(0) && !shadow_alias->root_parameter &&
                shadow_alias->mode == BorrowMode::Shared && plan.is_root(shadow.at(0)) &&
                plan.read(*shadow.at(2).value->children.at(0)) == shadow_alias,
            "confused a shadowed origin or use");
    require(child->parent_binding == &outer && child->root_parameter == &parameter &&
                plan.read(*reborrow.at(1).value->children.at(0)) == child &&
                plan.store(reborrow.at(1)) == child && plan.read(ret) == root_alias &&
                !plan.read(*child->source),
            "reborrow lost its parent/root or included address-only use");
    require(emit_c(unit, "lexical-slots.on1x") == original, "planning mutated reference HIR");
    auto other = checked("fn main(){let x=1;let r=borrow x;println(r);}");
    require(!plan.binding(other.functions.at(0).body.at(1)), "plan recognized a different Unit");

    // Forge an out-of-scope read of the child after its lexical block.
    auto& body = unit.functions.at(0).body;
    body.at(3).value->text = body.at(2).body.at(0).name;
    rejected(unit, "ON1X-VALUES-VERIFY", body.at(3).value->where);
}

void storage_initialization() {
    auto unit = checked("fn main(){let x=4;let r=borrow x;println(r);}");
    auto& body = unit.functions.at(0).body;
    auto& root = body.at(0);
    Stmt initialize;
    initialize.kind = Stmt::Kind::Initialize;
    initialize.where = root.where;
    initialize.name = root.name;
    initialize.value = std::move(root.value);
    root.kind = Stmt::Kind::Storage;
    root.declared = Type::I64;
    body.insert(body.begin() + 1, std::move(initialize));
    const auto plan = plan_lexical_slots(unit);
    const auto* slot = plan.binding(body.at(2));
    require(slot && slot->root_statement == &body.at(0) && plan.is_root(body.at(0)) &&
                !plan.is_root(body.at(1)),
            "lost initialized storage origin");
    std::swap(body.at(1), body.at(2));
    rejected(unit, "ON1X-VALUES-VERIFY", body.at(1).value->children.at(0)->where);
}

void unsupported_shapes() {
    auto unit = checked(R"(
record Cell { value:i64 }
fn peek(x:borrow i64)->i64 { let view=borrow x; return view; }
fn main() {
    let x=1;
    let alias=borrow mut x;
    println(peek(alias));
    println(ref_replace(alias, 3));
    println(alias);
    let record_value=record<Cell>{value:2};
    let record_view=borrow record_value;
    let owned=box(record<Cell>{value:4});
    let owned_view=borrow owned;
    let text="hello";
    let text_view=borrow text;
    let (left,right)=(1,2);
    let component=borrow left;
}
fn managed(x:sexpr,next:sexpr)->sexpr {
    let view=borrow mut x;
    return ref_replace(view,next);
}
fn incoming(x:ref i64)->i64 {
    let view=borrow mut x;
    return ref_replace(view,42);
}
)");
    const auto& peeker = unit.functions.at(0);
    const auto& body = unit.functions.at(1).body;
    const auto plan = plan_lexical_slots(unit);
    const auto* alias = plan.binding(body.at(1));
    require(alias && plan.read(*body.at(4).value->children.at(0)) == alias,
            "lost a direct read beside address-only uses");
    require(!plan.read(*body.at(2).value->children.at(0)->children.at(0)) &&
                !plan.read(*body.at(3).value->children.at(0)->children.at(0)),
            "mistook pointer-call or exchange argument for direct read");
    require(plan.exchange(*body.at(3).value->children.at(0)) == alias &&
                !plan.exchange(*body.at(2).value->children.at(0)),
            "did not distinguish the exchange from a borrowed call");
    require(!plan.is_root(peeker.params.at(0)) && !plan.binding(peeker.body.at(0)) &&
                !plan.read(*peeker.body.at(1).value),
            "incoming borrowed parameter gained a pointer-free plan");
    for (auto index : {6u, 8u, 10u, 12u})
        require(!plan.binding(body.at(index)), "unsupported origin gained a plan");
    for (auto index : {2u, 3u})
        require(!plan.exchange(*unit.functions.at(index).body.at(1).value),
                "exchange gained a plan for a managed or incoming borrowed origin");
}

void exchange_identity_and_rejection() {
    auto unit = checked(R"(
fn update(x:i64,next:i64)->i64 {
    let edit=borrow mut x;
    {
        let x=100;
        let child=borrow mut edit;
        return ref_replace(child, next+x);
    }
}
fn main() { println(update(5,6)); }
)");
    auto& function = unit.functions.at(0);
    auto& nested = function.body.at(1).body;
    auto& call = *nested.at(2).value;
    auto& target = *call.children.at(0);
    auto& replacement = *call.children.at(1);
    const auto plan = plan_lexical_slots(unit);
    const auto* exchange = plan.exchange(call);
    require(exchange && exchange == plan.binding(nested.at(1)) &&
                exchange->root_parameter == &function.params.at(0) &&
                !exchange->root_statement && exchange->parent_binding == &function.body.at(0),
            "exchange did not preserve the exact root under shadowing");
    require(!plan.exchange(target) && !plan.exchange(replacement) && !plan.read(target),
            "exchange indexed an operand instead of the checked call");

    nested.at(1).value->borrow = BorrowMode::Shared;
    rejected(unit, "ON1X-REF", target.where);
    nested.at(1).value->borrow = BorrowMode::Exclusive;
    auto& rhs_read = *replacement.children.at(0);
    rhs_read.text = nested.at(1).name;
    rejected(unit, "ON1X-REF", rhs_read.where);

    auto shadow = checked(R"(
fn ref_replace(a:i64,b:i64)->i64 { return a+b; }
fn main() {
    let value=1;
    let view=borrow mut value;
    println(ref_replace(view,2));
}
)");
    const auto shadow_plan = plan_lexical_slots(shadow);
    const auto& same_name = *shadow.functions.at(1).body.at(2).value->children.at(0);
    require(!shadow_plan.exchange(same_name) && shadow_plan.read(*same_name.children.at(0)),
            "matched a user call instead of the intrinsic identity");
}

void invalid_store_and_overlap() {
    auto unit = checked(R"(
fn main() {
    let x=1;
    let r=borrow mut x;
    r := r + 1;
    println(r);
}
)");
    auto& body = unit.functions.at(0).body;
    body.at(2).ref_store = false;
    rejected(unit, "ON1X-REF", body.at(2).where);
    body.at(2).ref_store = true;
    auto& operand = *body.at(2).value->children.at(0);
    operand.text = body.at(0).name;
    rejected(unit, "ON1X-REF", operand.where);
}
}

int main() try {
    identity_and_order();
    storage_initialization();
    unsupported_shapes();
    exchange_identity_and_rejection();
    invalid_store_and_overlap();
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
