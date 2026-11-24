// LLSynth MIR policy database; grammar, policy, compiler SHA256: 169de30f2460a7fd629722b2448052933de4aecc19fb34e5779bbcfc3f4bfb54 c3d6739d7feac8e5d0841dc3c88ef7ebc902b8b24be8277f8ac9e2df52797668 e6c434a59cc8be1a64f65cedb69183a7d242eb96916efcdbdcd184504634ef62
#pragma once

inline constexpr Activation activations[] = {
    {"rewrite", "fold-identity", {"", ""}, Group::Arithmetic},
    {"rewrite", "fold-identity", {"", ""}, Group::Boolean},
    {"hint", "kernel-hot", {"", ""}, Group::Arithmetic},
    {"hint", "canonicalized", {"", ""}, Group::Boolean},
    {"hint", "local-memory", {"", ""}, Group::Slots},
};

inline constexpr Pattern patterns[] = {
    // add_zero
    {{"", ""}, Group::Arithmetic, false, Op::Add, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 0}}, {AtomKind::Capture, 0}, Proof::Identity},
    // zero_add
    {{"", ""}, Group::Arithmetic, false, Op::Add, 2, {{AtomKind::Literal, 0}, {AtomKind::Capture, 0}}, {AtomKind::Capture, 0}, Proof::Identity},
    // sub_zero
    {{"", ""}, Group::Arithmetic, false, Op::Sub, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 0}}, {AtomKind::Capture, 0}, Proof::Identity},
    // sub_self
    {{"", ""}, Group::Arithmetic, false, Op::Sub, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Identity},
    // mul_one
    {{"", ""}, Group::Arithmetic, false, Op::Mul, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 1}}, {AtomKind::Capture, 0}, Proof::Identity},
    // one_mul
    {{"", ""}, Group::Arithmetic, false, Op::Mul, 2, {{AtomKind::Literal, 1}, {AtomKind::Capture, 0}}, {AtomKind::Capture, 0}, Proof::Identity},
    // div_one
    {{"", ""}, Group::Arithmetic, false, Op::Div, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 1}}, {AtomKind::Capture, 0}, Proof::Identity},
    // div_self
    {{"", ""}, Group::Arithmetic, true, Op::Div, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 1}, Proof::Nonzero},
    // archon_rem_one
    {{"mir-archon", "x86_64-unknown-linux-gnu"}, Group::Arithmetic, false, Op::Mod, 2, {{AtomKind::Capture, 0}, {AtomKind::Literal, 1}}, {AtomKind::Literal, 0}, Proof::Identity},
    // eq_self
    {{"", ""}, Group::Boolean, false, Op::Eq, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 1}, Proof::Identity},
    // ne_self
    {{"", ""}, Group::Boolean, false, Op::Ne, 2, {{AtomKind::Capture, 0}, {AtomKind::Capture, 0}}, {AtomKind::Literal, 0}, Proof::Identity},
    // forward_slot
    {{"", ""}, Group::Slots, false, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalSlot},
};
