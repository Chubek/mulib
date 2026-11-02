// Run with: node tests/test_machinebuild.js /path/to/mulib_test_fst_machinebuild
"use strict";
const assert = require("node:assert/strict");
const {execFileSync} = require("node:child_process");
const vm = require("node:vm");
const generated = execFileSync(process.argv[2], ["--emit-js"], {encoding: "utf8"});
// Evaluate only in the test: generated code itself contains no eval or imports.
const factory = vm.runInNewContext(generated);
const a = factory(), b = factory();
const plain = value => JSON.parse(JSON.stringify(value));
assert.equal(a.state, "idle");
assert.equal(a.accepting, true);
assert.deepEqual(plain(a.run([])), {accepted: true, output: [], state: "idle"});
assert.deepEqual(plain(a.step(96)), {matched: false, output: [], state: "idle", accepting: true});
assert.deepEqual(plain(a.step(97)), {matched: true, output: [65], state: "active", accepting: false});
assert.equal(b.state, "idle");
assert.deepEqual(plain(a.step(9)), {matched: true, output: [], state: "active", accepting: false});
assert.deepEqual(plain(a.step(4294967295)), {matched: true, output: [0], state: "done", accepting: true});
assert.deepEqual(plain(a.step(42)), {matched: true, output: [4294967295], state: "idle", accepting: true});
a.step(122);
assert.deepEqual(plain(a.run([97, 0, 9, 4294967295, 42])),
                 {accepted: true, output: [65, 0, 4294967295], state: "idle"});
assert.deepEqual(plain(a.run(new Uint32Array([122, 4294967295]))),
                 {accepted: true, output: [65, 0], state: "done"});
assert.equal(a.state, "active", "run is independent of incremental state");
assert.deepEqual(plain(a.run([97])), {accepted: false, output: [], state: "active"});
assert.deepEqual(plain(a.run([97, 10])), {accepted: false, output: [], state: "active"});
for (const token of [-1, 4294967296, 1.5, NaN, Infinity, "97", null, undefined, 97n]) {
  assert.throws(() => a.step(token), {name: "RangeError"});
  assert.equal(a.state, "active");
  assert.throws(() => a.run([token]), {name: "RangeError"});
}
for (const tokens of [null, "abc", {}, new DataView(new ArrayBuffer(4))]) {
  assert.throws(() => a.run(tokens), {name: "TypeError"});
}
assert.throws(() => a.run([96, -1]), {name: "RangeError"});
a.reset();
assert.equal(a.state, "idle");
const reference = JSON.parse(execFileSync(process.argv[2], ["--reference"], {
  encoding: "utf8", maxBuffer: 8 * 1024 * 1024
}));
for (const sample of reference) {
  const actual = plain(a.run(sample.tokens));
  assert.equal(actual.accepted, sample.accepted, JSON.stringify(sample.tokens));
  assert.deepEqual(actual.output, sample.output, JSON.stringify(sample.tokens));
}
assert.equal(reference.length, 2801);
console.log("Machinebuild generated JavaScript: passed");
