// LLSynth MIR policy database; grammar, policy banks, compiler SHA256: 4ed2655a9c5dfe5d372caffa2304b598e0d9a032047afbfdf82ea98a7de39664 25a673eaf1466a9008c6607b307a35512b99eea8e59d113356745e570768aa05 a4ce98b52ef585a67025d954cc269adc3340157c86b38aca40c116b25261fc7c
#pragma once

inline constexpr Pattern MemgraphLocalPatterns[] = {
    // memgraph-local: forward_word_store
    {{"mir-c11", "x86_64-unknown-linux-gnu"}, Bank::MemgraphLocal, false, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalStoreWord},
    // memgraph-local: reuse_word_load
    {{"mir-c11", "x86_64-unknown-linux-gnu"}, Bank::MemgraphLocal, false, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalLoadWord},
    // memgraph-local: forward_f32_store
    {{"mir-c11", "x86_64-unknown-linux-gnu"}, Bank::MemgraphLocal, false, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalStoreF32},
    // memgraph-local: reuse_f32_load
    {{"mir-c11", "x86_64-unknown-linux-gnu"}, Bank::MemgraphLocal, false, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalLoadF32},
    // memgraph-local: forward_f64_store
    {{"mir-c11", "x86_64-unknown-linux-gnu"}, Bank::MemgraphLocal, false, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalStoreF64},
    // memgraph-local: reuse_f64_load
    {{"mir-c11", "x86_64-unknown-linux-gnu"}, Bank::MemgraphLocal, false, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalLoadF64},
    // memgraph-local: forward_tuple_store
    {{"mir-c11", "x86_64-unknown-linux-gnu"}, Bank::MemgraphLocal, true, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalStoreTuple},
    // memgraph-local: reuse_tuple_load
    {{"mir-c11", "x86_64-unknown-linux-gnu"}, Bank::MemgraphLocal, true, Op::Load, 0, {{}, {}}, {AtomKind::Capture, 0}, Proof::LocalLoadTuple},
    // memgraph-local: discard_redundant_store
    {{"mir-c11", "x86_64-unknown-linux-gnu"}, Bank::MemgraphLocal, true, Op::Store, 1, {{AtomKind::Capture, 0}, {}}, {AtomKind::Capture, 0}, Proof::RedundantStore},
};
