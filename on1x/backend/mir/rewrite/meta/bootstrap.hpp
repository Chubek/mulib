// LLSynth MIR policy database; grammar, policy banks, compiler SHA256: 4ed2655a9c5dfe5d372caffa2304b598e0d9a032047afbfdf82ea98a7de39664 473cf52c9dd9a1843928a62a60f047962e53f5ea3578162e27f03ffadfa6f921 a4ce98b52ef585a67025d954cc269adc3340157c86b38aca40c116b25261fc7c
#pragma once

inline constexpr Pattern MetaCanonicalPatterns[] = {
    // meta-canonical: negate_zero
    {{"", ""}, Bank::MetaCanonical, false, Op::Neg, 1, {{AtomKind::Literal, 0}, {}}, {AtomKind::Literal, 0}, Proof::Identity},
    // meta-canonical: equal_bool_complement
    {{"", ""}, Bank::MetaCanonical, false, Op::Eq, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 1}}, {AtomKind::Literal, 0}, Proof::BoolComplement},
    // meta-canonical: unequal_bool_complement
    {{"", ""}, Bank::MetaCanonical, false, Op::Ne, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 1}}, {AtomKind::Literal, 1}, Proof::BoolComplement},
    // meta-canonical: less_than_i64_min
    {{"", ""}, Bank::MetaCanonical, false, Op::Lt, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, (-INT64_C(9223372036854775807) - 1)}}, {AtomKind::Literal, 0}, Proof::Identity},
    // meta-canonical: greater_than_i64_max
    {{"", ""}, Bank::MetaCanonical, false, Op::Gt, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 9223372036854775807}}, {AtomKind::Literal, 0}, Proof::Identity},
    // meta-canonical: at_least_i64_min
    {{"", ""}, Bank::MetaCanonical, false, Op::Ge, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, (-INT64_C(9223372036854775807) - 1)}}, {AtomKind::Literal, 1}, Proof::Identity},
    // meta-canonical: cancel_double_checked_neg
    {{"", ""}, Bank::MetaCanonical, true, Op::Neg, 1, {{AtomKind::Capture, 0}, {}}, {AtomKind::Capture, 1}, Proof::DoubleCheckedNeg},
    // meta-canonical: equal_zero_when_nonzero
    {{"", ""}, Bank::MetaCanonical, true, Op::Eq, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 0}}, {AtomKind::Literal, 0}, Proof::NonzeroComparison},
    // meta-canonical: unequal_zero_when_nonzero
    {{"", ""}, Bank::MetaCanonical, true, Op::Ne, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 0}}, {AtomKind::Literal, 1}, Proof::NonzeroComparison},
};
