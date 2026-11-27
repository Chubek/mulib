#include <on1x/asdl_module.hpp>
#include <on1x/loader.hpp>
#include <on1x/module_signature.hpp>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace on1x;

template<class Check>
bool rejects_at(Check check, std::string_view message, const Location& where, bool native,
                std::string_view code = "ON1X-MODULE-ASDL") {
    try {
        check();
        if (native) std::cerr << "expected rejection: " << message << '\n';
        return !native;
    } catch (const Error& error) {
        const bool matches = native && error.diagnostic_id == code && error.what() == message &&
                             error.where.file == where.file && error.where.offset == where.offset &&
                             error.where.line == where.line && error.where.column == where.column;
        if (!matches) std::cerr << "unexpected rejection: " << error.diagnostic_id << ": " << error.what() << '\n';
        return matches;
    }
}

bool rejects_at(const Unit& unit, std::string_view message, const Location& where, bool native,
                std::string_view code = "ON1X-MODULE-ASDL") {
    return rejects_at([&] { check_asdl_module(unit); }, message, where, native, code);
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    const bool native = std::string_view(argv[1]) == "1";
    try {
        Unit unit;
        const Location module_at{"request.on1x", 8, 2, 3};
        const Location program_at{"request.on1x", 80, 5, 7};
        const Location selector_at{"request.on1x", 120, 7, 3};
        const Location declaration_at{"request.on1x", 140, 8, 1};
        const auto warnings = collect_asdl_unused({
            {UnusedKind::Import, program_at, "action", "procedure"},
            {UnusedKind::Variable, selector_at, "local", "variable"},
            {UnusedKind::Type, declaration_at, "Variant", "enum type"},
        }, {.name="warning",.sarif="warning",.kind=diagnostic::Severity::Warning});
        if (warnings.size() != 3 || warnings[0].name != "unused-import" ||
            warnings[0].origin != (native ? "On1x.asdl" : "on1x-resolver") ||
            warnings[0].group != "unused" || warnings[0].message != "unused import of procedure 'action'" ||
            warnings[0].where.file != program_at.file || warnings[0].where.offset != program_at.offset ||
            warnings[1].message != "unused variable 'local'" ||
            warnings[2].message != "unused private enum type 'Variant'") return 5;
        const auto rejects_elevation = [&] {
            return rejects_at(unit, "shared elevation must be within the declaring module depth",
                              declaration_at, native, "ON1X-MODULE-ELEVATION");
        };
        unit.module = ModuleHeader{module_at, "Apps.Echo"};

        Import request;
        request.kind = Import::Kind::Program;
        request.module = "tools/echo.on1x";
        request.alias = "echo";
        request.quoted = true;
        request.where = program_at;
        unit.module_imports.push_back(request);
        Import legacy;
        legacy.where = selector_at;
        legacy.relative = 1;
        legacy.selective = true;
        legacy.names.push_back({"<+>", {}, true});
        unit.module_imports.push_back(std::move(legacy));
        Macro declaration;
        declaration.where = declaration_at;
        declaration.name = "helper";
        declaration.visibility = {DeclarationVisibility::Shared, 2};
        unit.macros.push_back(std::move(declaration));
        check_asdl_module(unit);
        unit.module_imports.front().module = "tools/echo'on1x\n-- error('not code')";
        check_asdl_module(unit);
        unit.module_imports.front().module = request.module;
        auto& elevation = unit.macros.front().visibility.elevation;
        elevation = 1;
        check_asdl_module(unit);
        elevation = 0;
        if (!rejects_elevation()) return 2;
        elevation = 3;
        if (!rejects_elevation()) return 2;
        elevation = std::numeric_limits<std::size_t>::max();
        if (!rejects_elevation()) return 2;
        elevation = 2;
        unit.module_imports.back().names[0].alias = "invalid";
        if (!rejects_at(unit, "invalid import selector", selector_at, native)) return 2;
        unit.module_imports.back().names[0].alias.clear();

        unit.module->name = "Apps..Echo";
        if (!rejects_at(unit, "invalid hierarchical module declaration", module_at, native)) return 2;

        unit.module.reset();
        if (!rejects_elevation()) return 3;
        unit.macros.clear();
        const auto rejects_program = [&] {
            return rejects_at(unit, "invalid program import request", program_at, native);
        };
        const auto rejects_list = [&] {
            return rejects_at(unit, "inconsistent import selector list or target alias", program_at, native);
        };
        const auto rejects_selector = [&] {
            return rejects_at(unit, "invalid import selector", program_at, native);
        };
        auto& selective_program = unit.module_imports.front();
        selective_program.relative = 1;
        if (!rejects_program()) return 3;
        selective_program.relative = 0;
        selective_program.module = "bad\\path";
        if (!rejects_program()) return 3;
        selective_program.module = "tools/echo.on1x";
        check_asdl_module(unit);
        selective_program.quoted = false;
        selective_program.module = "Tools..Echo";
        if (!rejects_program()) return 3;
        selective_program.module = "Tools.Echo";
        selective_program.selective = true;
        if (!rejects_list()) return 4;
        selective_program.names.push_back({"entry", "run", false});
        if (!rejects_list()) return 4;
        selective_program.alias.clear();
        check_asdl_module(unit);
        selective_program.names[0] = {"<+>", {}, true};
        if (!rejects_selector()) return 4;
        selective_program.names[0] = {"", "run", false};
        if (!rejects_selector()) return 4;
        selective_program.names[0] = {"entry", "bad.alias", false};
        if (!rejects_selector()) return 4;
        selective_program.names[0] = {"entry", "run", false};
        selective_program.selective = false;
        if (!rejects_list()) return 4;
        unit.module_imports.erase(unit.module_imports.begin());
        check_asdl_module(unit);

        Macro headerless;
        headerless.where = declaration_at;
        headerless.name = "helper";
        headerless.visibility = {DeclarationVisibility::Shared, 1};
        unit.macros.push_back(std::move(headerless));
        if (!rejects_elevation()) return 6;

        // Signature members and physical graph inputs arrive after the host has
        // verified the source and resolved imports. These negative cases
        // simulate damaged projections, not new language-policy diagnostics.
        ModuleSignature signature;
        signature.where = module_at;
        check_asdl_signature(signature); // Legacy signatures do not invoke this projection.
        signature.module = "Apps.Echo";
        const auto signature_check = [&] { check_asdl_signature(signature); };
        if (!rejects_at(signature_check, "empty verified module signature projection", module_at, native)) return 7;
        signature.types.push_back({module_at, "Item", 1});
        // Type and value namespaces are separate (the callable type remains C++-owned).
        signature.members.push_back({program_at, "Item", Type::function({}, Type::Void), true});
        signature_check();
        signature.types.push_back({declaration_at, "Item", 0});
        if (!rejects_at(signature_check, "invalid verified signature member projection", declaration_at, native))
            return 7;
        signature.types.pop_back();
        signature_check(); // A failed run cannot leave diagnostics or nodes in the next run.

        LoadedUnit graph;
        graph.dependencies = {"/tmp/provider.on1x", "/tmp/importer'\n.on1xm"};
        graph.source_edges.emplace_back(graph.dependencies[1], graph.dependencies[0]);
        const auto graph_check = [&] { check_asdl_graph(graph, module_at); };
        graph_check();
        graph.source_edges[0].second = "/tmp/missing.on1x";
        if (!rejects_at(graph_check, "missing physical dependency endpoint", module_at, native)) return 8;
        graph.source_edges[0].second = graph.dependencies[0];
        graph.dependencies.push_back(graph.dependencies[0]);
        if (!rejects_at(graph_check, "invalid physical dependency path projection", module_at, native)) return 8;
        graph.dependencies.pop_back();
        graph_check();
        if (native) {
            // The compiler's edge budget must fit in one native pass.
            graph.dependencies.clear();
            graph.source_edges.clear();
            for (unsigned i = 0; i < 1024; ++i)
                graph.dependencies.emplace_back("/tmp/source-" + std::to_string(i) + ".on1xm");
            for (unsigned i = 32; i < graph.dependencies.size() && graph.source_edges.size() < 16384; ++i)
                for (unsigned j = 0; j < 32; ++j)
                    graph.source_edges.emplace_back(graph.dependencies[i], graph.dependencies[j]);
            graph_check();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 5;
    }
}
