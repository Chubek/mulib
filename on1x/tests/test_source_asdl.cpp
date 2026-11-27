#include <on1x/asdl_module.hpp>
#include <on1x/ast.hpp>
#include <iostream>
#include <string_view>

namespace {
using namespace on1x;

bool rejects_at(const Unit& unit, std::string_view message, const Location& where, bool native) {
    try {
        check_asdl_module(unit);
        return !native;
    } catch (const Error& error) {
        return native && error.diagnostic_id == "ON1X-MODULE-ASDL" && error.what() == message &&
               error.where.file == where.file && error.where.offset == where.offset &&
               error.where.line == where.line && error.where.column == where.column;
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
        unit.module = ModuleHeader{module_at, "Apps.Echo"};

        Import request;
        request.kind = Import::Kind::Program;
        request.module = "tools/echo.on1x";
        request.alias = "echo";
        request.quoted = true;
        request.where = program_at;
        unit.module_imports.push_back(request);
        Import legacy;
        legacy.relative = 1;
        legacy.selective = true;
        legacy.names.push_back({"<+>", "<++>", true});
        unit.module_imports.push_back(std::move(legacy));
        Macro declaration;
        declaration.where = {"request.on1x", 140, 8, 1};
        declaration.name = "helper";
        declaration.visibility = {DeclarationVisibility::Shared, 2};
        unit.macros.push_back(std::move(declaration));
        check_asdl_module(unit);

        unit.module->name = "Apps..Echo";
        if (!rejects_at(unit, "invalid hierarchical module declaration", module_at, native)) return 2;

        unit.module.reset();
        unit.module_imports.front().relative = 1;
        if (!rejects_at(unit, "invalid program import request", program_at, native)) return 3;
        unit.module_imports.front().relative = 0;
        check_asdl_module(unit);
        auto& selective_program = unit.module_imports.front();
        selective_program.module = "Tools.Echo";
        selective_program.quoted = false;
        selective_program.alias.clear();
        selective_program.selective = true;
        selective_program.names.push_back({"entry", "run", false});
        check_asdl_module(unit);
        selective_program.selective = false;
        if (!rejects_at(unit, "invalid program import request", program_at, native)) return 4;
        unit.module_imports.erase(unit.module_imports.begin());
        check_asdl_module(unit);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 5;
    }
}
