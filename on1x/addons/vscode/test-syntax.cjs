const assert = require("node:assert/strict");
const manifest = require("./package.json");
const { languages: [language, manifestLanguage], grammars: [contribution, manifestContribution] } =
  manifest.contributes;
const manifestConfig = require(manifestLanguage.configuration);
const manifestGrammar = require(manifestContribution.path);
const config = require(language.configuration);
const grammar = require(contribution.path);
assert.equal(language.id, "on1x");
assert.deepEqual(language.extensions, [".on1x", ".on1xi"]);
assert.equal(contribution.language, language.id);
assert.equal(contribution.scopeName, grammar.scopeName);
assert.equal(grammar.scopeName, "source.on1x");
assert.deepEqual(manifestLanguage.filenames, ["On1xfile", "On1xfile.m", "On1xfile.i"]);
assert.equal(manifestContribution.language, manifestLanguage.id);
assert.equal(manifestContribution.scopeName, manifestGrammar.scopeName);
assert.equal(manifestGrammar.scopeName, "source.on1xfile");
assert.deepEqual(manifestConfig.comments, { lineComment: "#" });
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
    assert.ok(rule.include === "$self" ||
      (rule.include.startsWith("#") && grammar.repository[rule.include.slice(1)]), rule.include);
  }
  for (const field of ["begin", "end", "match"]) {
    if (rule[field]) new RegExp(rule[field]);
  }
  for (const child of rule.patterns || []) validate(child);
}
validate(grammar);
validate(manifestGrammar);
for (const rule of Object.values(grammar.repository)) validate(rule);

function matches(group, text) {
  // For flat match groups, consume the earliest match, in rule order on ties.
  const alternatives = grammar.repository[group].patterns.map(rule => `(?:${rule.match})`);
  return text.match(new RegExp(alternatives.join("|"), "g")) || [];
}
assert.deepEqual(matches("literals", "1..=4 -22 0x2au8 0o17u32 0B10 2e-2 1.25f32 5f64 value32"), [
  "1", "4", "22", "0x2au8", "0o17u32", "0B10", "2e-2", "1.25f32", "5f64"
]);
assert.deepEqual(matches("punctuation", "1..=4 ... <- => := ~ ?"), ["..=", "...", "<-", "=>", ":=", "~", "?"]);
assert.deepEqual(matches("declarations", "fn fizz(x: i64)"), ["fn fizz"]);
assert.deepEqual(matches("declarations", "proc fizz(x: i64) { let action = proc() {}; }"), ["proc fizz"]);
assert.deepEqual(matches("declarations", "proc visit<T>(x: T)"), ["proc visit"]);
assert.deepEqual(matches("declarations", "enum Option<T>"), ["enum Option"]);
assert.deepEqual(matches("declarations", "on1xi 1;"), ["on1xi"]);
assert.deepEqual(matches("metaprogramming", "unless!(false) $value @ppx checks"), ["unless!", "$value", "@ppx"]);
assert.deepEqual(matches("metaprogramming", "return !(flag);"), []);
assert.deepEqual(matches("metaprogramming", "proc!(flag);"), []);
assert.deepEqual(matches("keywords", "let match = for(loop); close(value); await select"), ["let"]);
assert.deepEqual(matches("keywords", "if close { return 1; }"), ["if", "return"]);
assert.deepEqual(matches("keywords", "match sent with { Ok(n) => n }"), ["match", "with"]);

const [removedImport, foreignImport, dynamicImport] = grammar.repository.keywords.patterns;
assert.equal(removedImport.name, "invalid.illegal.import.on1x");
assert.deepEqual(matches("keywords", "unsafe import foreign stdffi.clamp;"), ["unsafe import foreign"]);
const stdffi = new RegExp(foreignImport.match);
assert.deepEqual(stdffi.exec("import foreign stdffi . clamp as Clamp;").slice(1),
  ["import", "foreign", "stdffi", ".", "clamp"]);
assert.equal(foreignImport.captures["3"].name, "entity.name.namespace.on1x");
for (const text of ["import foreign as F;", "import foreign.clamp;", "import foreign;", "import foreign mulib.clamp;",
                    "import foreign stdffi.clamp.extra;", "let foreign = stdffi.clamp;"]) {
  assert.equal(stdffi.test(text), false, text);
}
const dynlib = new RegExp(dynamicImport.match);
assert.equal(dynlib.exec("unsafe import kernel [ dynlib ] mulib.clamp;")[4], "dynlib");
assert.equal(dynamicImport.captures["4"].name, "storage.modifier.on1x");
assert.equal(dynlib.test("let kernel = values[dynlib];"), false);
assert.equal(dynlib.test("import kernel as K;"), false);
assert.deepEqual(matches("metaprogramming", '@InlineC11("unsafe_i64", "a0", 4)'), ["@InlineC11"]);

const [comment, jsonString, directive, value, number, continuation] = manifestGrammar.patterns;
for (const name of ["on1xfile", "kind", "name", "directory", "vm", "scrutiny", "scrutiny-work",
                    "optimization-profile", "optimization", "backend", "target", "dependency", "profile",
                    "profile-arg", "command", "define", "dotenv", "kernel", "kernel-path", "source", "library",
                    "resource", "interface", "compiler", "assembler", "linker", "assembler-arg", "linker-arg",
                    "proxy-http", "proxy-https", "proxy-any", "proxy-none"]) {
  assert.equal(new RegExp(directive.match).exec(`  ${name} field`)[1], name);
}
assert.equal(new RegExp(directive.match).test("optimization-profile-extra default"), false);
assert.equal(new RegExp(directive.match).test("proxy-none-extra"), false);
assert.equal(new RegExp(directive.match).exec("proxy-none#comment")[1], "proxy-none");
for (const word of ["mir-vm", "default", "debug", "release", "on", "off", "override", "preserve",
                    "opt-in", "cephyr", "sandas", "yellowstone", "host", "integer", "true"]) {
  assert.equal(new RegExp(value.match).exec(word)[0], word);
}
assert.equal(new RegExp(value.match).test("some-mir-vm"), false);
assert.equal(new RegExp(number.match).exec("scrutiny-work 8192")[0], "8192");
assert.equal(new RegExp(number.match).exec('profile-arg "dev" level integer -2')[0], "-2");
assert.equal(new RegExp(continuation.match).test("kind \\ "), true);
assert.equal(new RegExp(continuation.match).test("kind \\ # trailing"), false);
assert.equal(new RegExp(jsonString.begin).exec('name "foo#bar" # trailing').index, 5);
assert.equal(new RegExp(comment.match).exec('name "foo#bar" # trailing').index, 9);
assert.equal(new RegExp(comment.match).exec("# trailing")[0], "# trailing");
assert.equal(new RegExp(jsonString.patterns[0].match).exec(String.raw`\u03bb`)[0], String.raw`\u03bb`);
assert.equal(new RegExp(jsonString.patterns[0].match).test(String.raw`\0`), false);

const repository = grammar.repository;
const [quoted, triple, single] = repository.strings.patterns.slice(-3);
const [textEscape, , interpolation, interpolationName] = repository["text-body"].patterns;
const [byteEscape] = repository["buffer-escapes"].patterns;
assert.equal(new RegExp(textEscape.match).exec(String.raw`x\"y`)[0], String.raw`\"`);
assert.equal(new RegExp(textEscape.match).test(String.raw`\q`), false);
assert.equal(new RegExp(textEscape.match).test(String.raw`\0`), false);
assert.equal(new RegExp(textEscape.match).test(String.raw`\$`), true);
assert.equal(new RegExp(byteEscape.match).test(String.raw`\0\xff`), true);
assert.equal(new RegExp(byteEscape.match).test(String.raw`\$`), false);
assert.equal(new RegExp(repository["invalid-escape"].match).test(String.raw`\q`), true);
assert.equal(new RegExp(interpolation.begin).test("${score + 1}"), true);
assert.equal(new RegExp(interpolationName.match).test("$name"), true);
assert.equal(quoted.patterns[0].include, "#text-body");
for (const [text, scope, child] of [
  ['r!"""', "string.quoted.double.raw.on1x", undefined],
  ["r!'''", "string.quoted.single.raw.on1x", undefined],
  ['r!"', "string.quoted.double.raw.on1x", undefined],
  ['b!"""', "string.quoted.double.buffer.on1x", "#buffer-escapes"],
  ["b!'''", "string.quoted.single.buffer.on1x", "#buffer-escapes"],
  ['b!"', "string.quoted.double.buffer.on1x", "#buffer-escapes"],
  ['"""', "string.quoted.double.block.on1x", "#text-body"],
  ["'''", "string.quoted.single.block.on1x", "#text-body"],
]) {
  const rule = repository.strings.patterns.find(item => new RegExp(`^(?:${item.begin})`).test(text));
  assert.equal(rule.name, scope);
  assert.equal(rule.patterns?.at(-1)?.include, child);
}
assert.equal(repository.strings.patterns.some(rule => new RegExp(rule.begin).test("'word'")), false);
assert.equal(new RegExp(triple.end).test("line without a backtick"), false);
assert.equal(new RegExp(single.end).test("line without a backtick"), true);
assert.equal(new RegExp(`^(?:${single.begin})`).test("```"), false); // Triple rule wins at the opening tick.
const [hashComment, , atComment, docstring] = repository.comments.patterns;
assert.equal(new RegExp(hashComment.begin).test("// /*"), false);
for (const text of [String.raw`#\pound`, String.raw`b!#\o112`, String.raw`#\"`, String.raw`#\Ｂ`]) {
  assert.equal(new RegExp(hashComment.begin).test(text), false);
  assert.equal(new RegExp(repository.characters.match).exec(text)[0], text);
}
assert.equal(new RegExp(atComment.begin).test("{@inline(x)}"), false);
assert.equal(new RegExp(atComment.begin).test("{@ inline(x) @}"), true);
assert.equal(new RegExp(repository["dash-comment"].begin).test("{- block -}"), true);
assert.equal(new RegExp(docstring.begin).test("{> metadata }"), true);
console.log("On1x VS Code manifest, configuration and regex checks passed (no editor host).");
