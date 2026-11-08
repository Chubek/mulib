"""Small, documented host API; native MuLIB names come from Inventory.json."""

KEYWORDS = {
    "val": "Declare an immutable binding: val name <- expression;",
    "var": "Declare a mutable binding: var name <- expression;",
    "ref": "Alias an existing mutable binding: ref alias <- name;",
    "mulib": "Declare a host-backed MuLIB namespace. Its semicolon is optional at a declaration boundary.",
    "fn": "Lexically scoped function: fn(parameters) { statements }.",
    "if": "Conditional: if condition { statements } else { statements }.",
    "else": "Alternative branch; else if is supported.",
    "while": "Loop while the condition is truthy.",
    "return": "Return a value (or nil with return;). At top level, ends the run.",
    "break": "Exit the nearest loop; cannot cross a function boundary.",
    "and": "Short-circuit conjunction; returns an operand.",
    "or": "Short-circuit alternative; returns an operand.",
    "not": "Boolean negation. Only nil and false are falsey.",
    "true": "Boolean true.", "false": "Boolean false.", "nil": "Absence of a value.",
}

# signature, documentation. Keep names and argument order aligned with mull/README.md.
MEMBERS = {
    "G": {
        "LoadLib": ("LoadLib(name)", "Load a Package.mull; returns nil and sets $E? on failure."),
        "Native": ("Native(name)", "Retrieve a native package registered by the host."),
        "GetStdout": ("GetStdout()", "Return a callable accepting one string."),
        "DieWith": ("DieWith(value)", "Raise a runtime error with the given value."),
        "Assert": ("Assert(value)", "Raise an error unless value is truthy."),
        "Str": ("Str(value)", "Convert a value to display text."),
        "Type": ("Type(value)", "Return the value's type name."),
        "Len": ("Len(value)", "Count string bytes, list elements, or record entries."),
        "MulibWrap": ("MulibWrap(name)", "Wrap a MuLIB module using the installed native provider."),
    },
    "printf": {
        "SetMainSink": ("SetMainSink(fn)", "Set the output callback, which accepts a string."),
        "Format": ("Format(template, ...)", "Format {} placeholders; {{ and }} escape braces."),
        "Printf": ("Printf(template, ...)", "Format and write to the configured sink."),
        "Print": ("Print(...)", "Concatenate values and write them to the sink."),
        "Println": ("Println(...)", "Concatenate values and append a newline."),
    },
    "terminal": {
        "Write": ("Write(string)", "Write to the runtime stdout callback."),
        "Clear": ("Clear()", "Write ANSI terminal-clear bytes."),
        "Color": ("Color(code, string)", "Wrap text in ANSI SGR escapes; code must be integral, 0–107."),
    },
    "math": {name: (signature, "Finite binary64 arithmetic.") for name, signature in [
        ("Abs", "Abs(x)"), ("Floor", "Floor(x)"), ("Ceil", "Ceil(x)"),
        ("Sqrt", "Sqrt(x)"), ("Pow", "Pow(x, y)"), ("Min", "Min(x, y)"),
        ("Max", "Max(x, y)"), ("Pi", "Pi"),
    ]},
    "text": {name: (signature, "Byte-string operation; case conversion is ASCII-only.") for name, signature in [
        ("Split", "Split(text, sep)"), ("Join", "Join(strings, sep)"),
        ("Replace", "Replace(text, from, to)"), ("Upper", "Upper(text)"), ("Lower", "Lower(text)"),
    ]},
    "list": {name: (signature, "Collections are immutable; updates return a new list.") for name, signature in [
        ("Range", "Range(start, end)"), ("Map", "Map(xs, fn)"), ("Filter", "Filter(xs, fn)"),
        ("Fold", "Fold(xs, initial, fn)"), ("Push", "Push(xs, value)"), ("Set", "Set(xs, index, value)"),
    ]},
    "Mulib": {name: (signature, "MuLIB native bridge helper; requires an installed provider.") for name, signature in [
        ("Wrap", "Wrap(name)"), ("Libraries", "Libraries()"), ("New", "New(type, initial?)"),
        ("Array", "Array(type, countOrList)"), ("Buffer", "Buffer(sizeOrString)"),
        ("Callback", "Callback(type, fn)"), ("SizeOf", "SizeOf(type)"),
    ]},
}

TOOLKIT = {
    "DSL": ("DSL<Derived, Features...>", "CRTP feature composition", "ch03-crtp-foundation.md"),
    "FixedString": ("FixedString", "Compile-time string", "ch04-fixed-string.md"),
    "Pipeline": ("Pipeline", "Pipeline feature tag", "ch06-pipeline.md"),
    "Operators": ("Operators", "Predicate-composition feature tag", "ch07-operators.md"),
    "ASTNode": ("ASTNode", "Value-based abstract syntax tree", "ch10-ast-node.md"),
    "leaf": ("leaf", "Construct an AST leaf", "ch11-leaf-and-node.md"),
    "node": ("node", "Construct an AST node", "ch11-leaf-and-node.md"),
    "Parser": ("Parser", "Parser combinator", "ch23-parser-core.md"),
    "ch": ("ch", "Match one character", "ch24-primitive-parsers.md"),
    "run_parser": ("run_parser", "Run a parser with diagnostics", "ch25-parser-diagnostics.md"),
    "MuLL": ("MuLL.Runtime", "D MuLL embedding runtime", "../mull/README.md"),
    "Runtime": ("mull::Runtime / MuLL.Runtime", "MuLL embedding runtime", "../mull/README.md"),
}
