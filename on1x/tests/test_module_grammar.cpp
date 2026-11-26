#include <on1x/ast.hpp>
#include <on1x/clone.hpp>
#include <on1x/operator_scope.hpp>
#include <iostream>
#include <string_view>

namespace {
using namespace on1x;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void rejects(const std::string& source, const std::string& file, std::string_view id) {
    auto tokens = scan(source, file);
    try {
        (void)parse(tokens);
    } catch (const Error& error) {
        require(error.where.file == file, "lost diagnostic source file");
        require(id.empty() || error.diagnostic_id == id, error.what());
        return;
    }
    throw std::runtime_error("invalid module syntax accepted: " + source);
}

void module_and_imports() {
    const auto tokens = scan(
        "# comment\n"
        "module example.tools;\n"
        "import one.two as two;\n"
        "from one.more import (source as renamed, another, operator(\"%%\"));\n"
        "import program tool.helper as helper;\n"
        "import program \"scripts/start.on1x\" as start;\n"
        "import program; import program.foo; import program as ordinary;\n",
        "tools.on1xm");
    const auto discovery = discover_operators(tokens);
    const auto unit = parse(tokens);
    std::size_t budget = 1000;
    const auto cloned = clone_unit(unit, budget);
    require(unit.module && discovery.module && unit.module->name == "example.tools" &&
            discovery.module->name == unit.module->name &&
            unit.module->where.line == 2 && unit.module->where.column == 1,
            "missing header or wrong module location");
    const auto imports = [](const std::vector<Import>& list) {
        require(list.size() == 7, "wrong import count");
        require(list[0].module == "one.two" && list[0].alias == "two" &&
                list[0].kind == Import::Kind::Module, "wrong module import");
        require(list[1].selective && list[1].module == "one.more" &&
                list[1].names.size() == 3 && list[1].names[0].name == "source" &&
                list[1].names[0].alias == "renamed" && list[1].names[1].name == "another" &&
                list[1].names[2].is_operator && list[1].names[2].name == "%%",
                "selective imports lost their spelling");
        require(list[2].kind == Import::Kind::Program && !list[2].quoted &&
                list[2].module == "tool.helper" && list[2].alias == "helper",
                "wrong dotted program import");
        require(list[3].kind == Import::Kind::Program && list[3].quoted &&
                list[3].module == "scripts/start.on1x" && list[3].alias == "start",
                "wrong quoted program import");
        for (std::size_t i = 4; i < list.size(); ++i)
            require(list[i].kind == Import::Kind::Module, "'program' is no longer contextual");
    };
    imports(unit.module_imports);
    imports(discovery.imports);
    imports(cloned.module_imports);
    require(cloned.module && cloned.module->name == unit.module->name, "clone lost module header");
    require(unit.imports.empty(), "explicit program imports leaked into legacy quoted paths");
}

void legacy() {
    const auto unit = parse(scan(
        "import \"legacy.on1x\"; from ..one import two;\n"
        "fn public(module: i64) -> i64 {"
        "  let private = module; let shared = private; let elevate = shared;"
        "  let program = elevate; let abstract = program; return abstract;"
        "}", "legacy.on1x"));
    require(!unit.module && unit.imports.size() == 1 && unit.imports[0] == "legacy.on1x",
            "legacy quoted import changed");
    require(unit.module_imports.size() == 2 && unit.module_imports[1].relative == 2 &&
            unit.module_imports[1].module == "one", "legacy relative import changed");
    require(unit.functions.size() == 1 && unit.functions[0].name == "public",
            "module words became globally reserved");
    const auto signature = parse_interface(scan("on1xi 1; fn f(x: i64) -> bool;", "legacy.on1xi"));
    require(signature.size() == 1 && signature[0].name == "f", "legacy interface changed");
}

void visibility() {
    const auto tokens = scan(
        "module alpha.beta.gamma;\n"
        "public fn public(private: i64) -> i64 { return private; }\n"
        "private record shared{elevate: i64}\n"
        "shared elevate(2) fn shared() -> i64 { return 1; }\n"
        "shared fn elevate() -> i64 { return 2; }\n"
        "fn module() -> i64 { return shared(); }\n"
        "private operator(lexeme: \"%%\", arity: \"binary\", formals: x: i64, y: i64)"
        " -> i64 { return x + y; }\n",
        "visibility.on1xm");
    const auto discovery = discover_operators(tokens);
    const auto unit = parse(tokens);
    const auto& funcs = unit.functions;
    require(funcs.size() == 4 && funcs[0].visibility.kind == DeclarationVisibility::Public &&
            funcs[1].visibility.kind == DeclarationVisibility::Shared &&
            funcs[1].visibility.elevation == 2 &&
            funcs[2].visibility.kind == DeclarationVisibility::Shared &&
            funcs[2].visibility.elevation == 1 &&
            funcs[3].visibility.kind == DeclarationVisibility::Unspecified,
            "function visibility or elevation was lost");
    require(unit.nominals.size() == 1 && unit.nominals[0].name == "shared" &&
            unit.nominals[0].visibility.kind == DeclarationVisibility::Private,
            "nominal visibility was lost or became reserved");
    require(discovery.operators.size() == 1 && unit.operators.size() == 1 &&
            discovery.operators[0].visibility.kind == DeclarationVisibility::Private &&
            unit.operators[0].header.visibility.kind == DeclarationVisibility::Private,
            "parse and operator discovery disagree on visibility");
    std::size_t budget = 1000;
    const auto cloned = clone_unit(unit, budget);
    require(cloned.functions[1].visibility.kind == DeclarationVisibility::Shared &&
            cloned.functions[1].visibility.elevation == 2 &&
            cloned.operators[0].header.visibility.kind == DeclarationVisibility::Private,
            "clone lost annotated declaration metadata");
}
}

int main() {
    try {
        module_and_imports();
        legacy();
        visibility();
        rejects("", "empty.on1xm", "ON1X-MODULE-HEADER");
        rejects("fn f() {}", "missing.on1xm", "ON1X-MODULE-HEADER");
        rejects("module a; fn f() {}", "wrong.on1x", "ON1X-MODULE-HEADER");
        rejects("module a; module b;", "duplicate.on1xm", "ON1X-MODULE-HEADER");
        rejects("module a..b;", "dots.on1xm", "ON1X-PARSE-EXPECTED");
        rejects("module a.;", "trailing.on1xm", "");
        rejects("module a.b", "semicolon.on1xm", "ON1X-PARSE-EXPECTED");
        rejects("module 42;", "numeric.on1xm", "");
        rejects("module " + std::string(4097, 'x') + ";", "long.on1xm", "");
        rejects("module a; import program \"\";", "path.on1xm", "ON1X-PROGRAM-IMPORT");
        rejects("module a; import program \"foo\\n.on1x\";", "escape.on1xm", "ON1X-PROGRAM-IMPORT");
        rejects("module a; import program foo as ;", "alias.on1xm", "");
        rejects("public fn f() {}", "program.on1x", "ON1X-MODULE-VISIBILITY");
        rejects("module a; private public fn f() {}", "double.on1xm", "ON1X-MODULE-VISIBILITY");
        rejects("module a; public import b;", "import.on1xm", "ON1X-MODULE-VISIBILITY");
        rejects("module a; public @scrutiny off;", "directive.on1xm", "ON1X-MODULE-VISIBILITY");
        rejects("module a; private elevate(1) fn f() {}", "private.on1xm", "ON1X-MODULE-VISIBILITY");
        for (std::string_view n : {"", "0", "00", "-1", "+1", "0x1", "1u8", "1.0", "1e3", "oops",
                                   "9999999999999999999999999999999999999999"})
            rejects("module a; shared elevate(" + std::string(n) + ") fn f() {}",
                    "elevation.on1xm", "ON1X-MODULE-ELEVATION");
        rejects("module a; shared elevate(1 + 1) fn f() {}", "expression.on1xm", "ON1X-PARSE-EXPECTED");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
