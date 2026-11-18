const assert = require("node:assert/strict");
const manifest = require("./package.json");
const { languages: [language], grammars: [contribution] } = manifest.contributes;
const config = require(language.configuration);
const grammar = require(contribution.path);
assert.equal(language.id, "on1x");
assert.deepEqual(language.extensions, [".on1x", ".on1xi"]);
assert.equal(contribution.language, language.id);
assert.equal(contribution.scopeName, grammar.scopeName);
assert.equal(grammar.scopeName, "source.on1x");
for (const field of ["main", "browser", "activationEvents", "dependencies"]) {
  assert.equal(manifest[field], undefined, `${field} would change the syntax-only package`);
}
assert.equal(manifest.capabilities.untrustedWorkspaces.supported, true);
assert.deepEqual(config.comments, { lineComment: "#" });
assert.deepEqual(config.brackets, [["{", "}"], ["[", "]"], ["(", ")"]]);

// Compile the authored regex subset with Node, without needing a VS Code host.
// This checks grammar wiring and individual rules, not TextMate tokenization.
function validate(rule) {
  if (rule.include) {
    assert.ok(grammar.repository[rule.include.slice(1)], rule.include);
  }
  for (const field of ["begin", "end", "match"]) {
    if (rule[field]) new RegExp(rule[field]);
  }
  for (const child of rule.patterns || []) validate(child);
}
validate(grammar);
for (const rule of Object.values(grammar.repository)) validate(rule);

function matches(group, text) {
  // For flat match groups, consume the earliest match, in rule order on ties.
  const alternatives = grammar.repository[group].patterns.map(rule => `(?:${rule.match})`);
  return text.match(new RegExp(alternatives.join("|"), "g")) || [];
}
assert.deepEqual(matches("literals", "1..=4 -22 0x2au8 2e-2 1.25f32 5f64 value32"), [
  "1", "4", "22", "0x2au8", "2e-2", "1.25f32", "5f64"
]);
assert.deepEqual(matches("punctuation", "1..=4 ... <- => ?"), ["..=", "...", "<-", "=>", "?"]);
assert.deepEqual(matches("declarations", "fn fizz(x: i64)"), ["fn fizz"]);
assert.deepEqual(matches("declarations", "enum Option<T>"), ["enum Option"]);
assert.deepEqual(matches("declarations", "on1xi 1;"), ["on1xi"]);
assert.deepEqual(matches("metaprogramming", "unless!(false) $value @ppx checks"), ["unless!", "$value", "@ppx"]);
assert.deepEqual(matches("metaprogramming", "return !(flag);"), []);
assert.deepEqual(matches("keywords", "let match = for(loop); close(value); await select"), ["let"]);
assert.deepEqual(matches("keywords", "if close { return 1; }"), ["if", "return"]);
assert.deepEqual(matches("keywords", "match sent with { Ok(n) => n }"), ["match", "with"]);

const [quoted, triple, single] = grammar.repository.strings.patterns;
assert.equal(new RegExp(quoted.patterns[0].match).exec(String.raw`x\"y`)[0], String.raw`\"`);
assert.equal(new RegExp(quoted.patterns[0].match).test(String.raw`\q`), false);
assert.equal(new RegExp(quoted.patterns[1].match).test(String.raw`\q`), true);
assert.equal(new RegExp(triple.end).test("line without a backtick"), false);
assert.equal(new RegExp(single.end).test("line without a backtick"), true);
assert.equal(new RegExp(`^(?:${single.begin})`).test("```"), false); // Triple rule wins at the opening tick.
assert.equal(new RegExp(grammar.repository.comments.begin).test("// /*"), false);
console.log("On1x VS Code manifest, configuration and regex checks passed (no editor host).");
