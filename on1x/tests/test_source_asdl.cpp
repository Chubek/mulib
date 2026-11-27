#include <on1x/asdl_module.hpp>
#include <on1x/ast.hpp>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace on1x;

bool rejects_at(const Unit& unit, std::string_view message, const Location& where, bool native,
                std::string_view code = "ON1X-MODULE-ASDL") {
    try {
        check_asdl_module(unit);
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
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 5;
    }
}
