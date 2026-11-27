// LLSynth MIR policy database; grammar, policy banks, compiler SHA256: 4ed2655a9c5dfe5d372caffa2304b598e0d9a032047afbfdf82ea98a7de39664 73de2a0de7c831ebb8eea42388acbc60a396ed370d67edfc924bd54afbab0293 a4ce98b52ef585a67025d954cc269adc3340157c86b38aca40c116b25261fc7c
#pragma once

enum class Bank : unsigned {
    AdditiveIdentities = 1u << 0,
    MultiplicativeIdentities = 1u << 1,
    DivisionUnits = 1u << 2,
    GuardedDivision = 1u << 3,
    ScalarEquality = 1u << 4,
    OrderedReflexivity = 1u << 5,
    BooleanNegation = 1u << 6,
    BooleanComparisons = 1u << 7,
    StoreForwarding = 1u << 8,
    LoadReuse = 1u << 9,
    MemgraphLocal = 1u << 10,
    MetaCanonical = 1u << 11,
};

inline constexpr Activation activations[] = {
    {"rewrite", "fold-identity", {"", ""}, Bank::AdditiveIdentities},
    {"rewrite", "fold-identity", {"", ""}, Bank::MultiplicativeIdentities},
    {"rewrite", "fold-identity", {"", ""}, Bank::DivisionUnits},
    {"rewrite", "fold-identity", {"", ""}, Bank::GuardedDivision},
    {"rewrite", "fold-identity", {"", ""}, Bank::ScalarEquality},
    {"rewrite", "fold-identity", {"", ""}, Bank::OrderedReflexivity},
    {"rewrite", "fold-identity", {"", ""}, Bank::BooleanNegation},
    {"rewrite", "fold-identity", {"", ""}, Bank::BooleanComparisons},
    {"hint", "kernel-hot", {"", ""}, Bank::AdditiveIdentities},
    {"hint", "kernel-hot", {"", ""}, Bank::MultiplicativeIdentities},
    {"hint", "kernel-hot", {"", ""}, Bank::DivisionUnits},
    {"hint", "kernel-hot", {"", ""}, Bank::GuardedDivision},
    {"hint", "canonicalized", {"", ""}, Bank::ScalarEquality},
    {"hint", "canonicalized", {"", ""}, Bank::OrderedReflexivity},
    {"hint", "canonicalized", {"", ""}, Bank::BooleanNegation},
    {"hint", "canonicalized", {"", ""}, Bank::BooleanComparisons},
    {"hint", "local-memory", {"", ""}, Bank::StoreForwarding},
    {"hint", "local-memory", {"", ""}, Bank::LoadReuse},
};

inline constexpr Pattern patterns[] = {
    // additive-identities: add_zero
    {{"", ""}, Bank::AdditiveIdentities, false, Op::Add, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 0}}, {AtomKind::Capture, 0}, Proof::Identity},
    // additive-identities: zero_add
    {{"", ""}, Bank::AdditiveIdentities, false, Op::Add, 2, {{AtomKind::Literal, 0}, {AtomKind::Capture, 0}}, {AtomKind::Capture, 0}, Proof::Identity},
    // additive-identities: sub_zero
    {{"", ""}, Bank::AdditiveIdentities, false, Op::Sub, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 0}}, {AtomKind::Capture, 0}, Proof::Identity},
    // additive-identities: sub_self
    {{"", ""}, Bank::AdditiveIdentities, false, Op::Sub, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Identity},
    // multiplicative-identities: mul_zero
    {{"", ""}, Bank::MultiplicativeIdentities, false, Op::Mul, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 0}}, {AtomKind::Literal, 0}, Proof::Identity},
    // multiplicative-identities: zero_mul
    {{"", ""}, Bank::MultiplicativeIdentities, false, Op::Mul, 2, {{AtomKind::Literal, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Identity},
    // multiplicative-identities: mul_one
    {{"", ""}, Bank::MultiplicativeIdentities, false, Op::Mul, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 1}}, {AtomKind::Capture, 0}, Proof::Identity},
    // multiplicative-identities: one_mul
    {{"", ""}, Bank::MultiplicativeIdentities, false, Op::Mul, 2, {{AtomKind::Literal, 1}, {AtomKind::Capture, 0}}, {AtomKind::Capture, 0}, Proof::Identity},
    // division-units: div_one
    {{"", ""}, Bank::DivisionUnits, false, Op::Div, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 1}}, {AtomKind::Capture, 0}, Proof::Identity},
    // division-units: archon_rem_one
    {{"mir-archon", "x86_64-unknown-linux-gnu"}, Bank::DivisionUnits, false, Op::Mod, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 1}}, {AtomKind::Literal, 0}, Proof::Identity},
    // guarded-division: div_self
    {{"", ""}, Bank::GuardedDivision, true, Op::Div, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 1}, Proof::Nonzero},
    // guarded-division: zero_div
    {{"", ""}, Bank::GuardedDivision, true, Op::Div, 2, {{AtomKind::Literal, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Nonzero},
    // guarded-division: rem_self
    {{"", ""}, Bank::GuardedDivision, true, Op::Mod, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Nonzero},
    // guarded-division: zero_rem
    {{"", ""}, Bank::GuardedDivision, true, Op::Mod, 2, {{AtomKind::Literal, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Nonzero},
    // scalar-equality: eq_self
    {{"", ""}, Bank::ScalarEquality, false, Op::Eq, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 1}, Proof::Identity},
    // scalar-equality: ne_self
    {{"", ""}, Bank::ScalarEquality, false, Op::Ne, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Identity},
    // ordered-reflexivity: lt_self
    {{"", ""}, Bank::OrderedReflexivity, false, Op::Lt, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Identity},
    // ordered-reflexivity: le_self
    {{"", ""}, Bank::OrderedReflexivity, false, Op::Le, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 1}, Proof::Identity},
    // ordered-reflexivity: gt_self
    {{"", ""}, Bank::OrderedReflexivity, false, Op::Gt, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Identity},
    // ordered-reflexivity: ge_self
    {{"", ""}, Bank::OrderedReflexivity, false, Op::Ge, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 1}, Proof::Identity},
    // boolean-negation: not_false
    {{"", ""}, Bank::BooleanNegation, false, Op::Not, 1, {{AtomKind::Literal, 0}, {}}, {AtomKind::Literal, 1}, Proof::Identity},
    // boolean-negation: not_true
    {{"", ""}, Bank::BooleanNegation, false, Op::Not, 1, {{AtomKind::Literal, 1}, {}}, {AtomKind::Literal, 0}, Proof::Identity},
    // boolean-negation: not_not
    {{"", ""}, Bank::BooleanNegation, false, Op::Not, 1, {{AtomKind::Capture, 0}, {}}, {AtomKind::Capture, 1}, Proof::DoubleNot},
    // boolean-comparisons: eq_true
    {{"", ""}, Bank::BooleanComparisons, false, Op::Eq, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 1}}, {AtomKind::Capture, 0}, Proof::BoolOperands},
    // boolean-comparisons: true_eq
    {{"", ""}, Bank::BooleanComparisons, false, Op::Eq, 2, {{AtomKind::Literal, 1}, {AtomKind::Capture, 0}}, {AtomKind::Capture, 0}, Proof::BoolOperands},
    // boolean-comparisons: ne_false
    {{"", ""}, Bank::BooleanComparisons, false, Op::Ne, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 0}}, {AtomKind::Capture, 0}, Proof::BoolOperands},
    // boolean-comparisons: false_ne
    {{"", ""}, Bank::BooleanComparisons, false, Op::Ne, 2, {{AtomKind::Literal, 0}, {AtomKind::Capture, 0}}, {AtomKind::Capture, 0}, Proof::BoolOperands},
    // store-forwarding: forward_slot
    {{"", ""}, Bank::StoreForwarding, false, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalStore},
    // load-reuse: reuse_slot
    {{"", ""}, Bank::LoadReuse, false, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalLoad},
};
