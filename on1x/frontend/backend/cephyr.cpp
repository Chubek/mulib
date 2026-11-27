#include "cephyr.hpp"
#include <cephyr/Cephyr.h>

#include <memory>
#ifdef ON1X_CEPHYR_SHARED_PATH
#include <dlfcn.h>
#include <stdexcept>
#endif

namespace on1x::cephyr {
namespace {
namespace fs = std::filesystem;

// A restricted compiler process can lower portable MIR without mapping any
// native provider. Keep the selected library live while its results exist.
struct Api {
#ifdef ON1X_CEPHYR_SHARED_PATH
    struct Close { void operator()(void* handle) const { dlclose(handle); } };
    std::unique_ptr<void, Close> library{dlopen(ON1X_CEPHYR_SHARED_PATH, RTLD_NOW | RTLD_LOCAL)};
    template<class F> F symbol(const char* name) {
        if (!library) throw std::runtime_error("could not load the configured Cephyr embedding library");
        auto address = dlsym(library.get(), name);
        if (!address) throw std::runtime_error(std::string("Cephyr embedding library lacks ") + name);
        return reinterpret_cast<F>(address); // POSIX dlsym function address
    }
#define ON1X_CEPHYR_ENTRY(name) decltype(&cephyr_##name) name = symbol<decltype(name)>("cephyr_" #name)
#else
#define ON1X_CEPHYR_ENTRY(name) decltype(&cephyr_##name) name = cephyr_##name
#endif
    ON1X_CEPHYR_ENTRY(abi_version);
    ON1X_CEPHYR_ENTRY(compile);
    ON1X_CEPHYR_ENTRY(result_destroy);
    ON1X_CEPHYR_ENTRY(result_diagnostics);
    ON1X_CEPHYR_ENTRY(result_output);
#undef ON1X_CEPHYR_ENTRY
};

bool exists_as(const fs::path& path, fs::file_type type) {
    if (path.empty() || path.native().find('\0') != std::string::npos) return false;
    std::error_code error;
    return fs::status(path, error).type() == type && !error;
}

Result invalid(std::string message) {
    return {Status::invalid_argument, {}, std::move(message)};
}
}

Result compile_object(const Request& request) {
    if (request.target != "x86_64-linux-gnu")
        return {Status::unsupported_target, {}, "unsupported On1x Cephyr object target: " + request.target};
    static const Api api;
    if (api.abi_version() != 1)
        return invalid("On1x requires Cephyr embedding ABI 1");
    if (request.logical_filename.empty()) return invalid("missing generated C filename");
    std::string input = "#line 1 \"";
    for (unsigned char c : request.logical_filename) {
        if (c < ' ' || c == 127) return invalid("control byte in generated C filename");
        if (c == '\\' || c == '"' || c == '?') input += '\\';
        input += static_cast<char>(c);
    }
    input += "\"\n";
    input += request.source;

    if (request.resource_root.empty() ||
        !exists_as(request.resource_root / "triples/x86_64-linux-gnu.lua", fs::file_type::regular) ||
        !exists_as(request.resource_root / "stdlib/include", fs::file_type::directory))
        return invalid("Cephyr resource root must contain the AMD64 target script and stdlib/include");
    if (request.archon_root.empty() ||
        !exists_as(request.archon_root / "stdspec/amd64/root.arch", fs::file_type::regular))
        return invalid("Archon resource root must contain stdspec/amd64/root.arch");
    if (!exists_as(request.config, fs::file_type::regular))
        return invalid("missing On1x Cephyr configuration: " + request.config.string());

    std::vector<std::string> args{"--config", request.config.string(),
        "--target=" + request.target, request.options.factir ? "--factir" : "--no-factir",
        request.options.sandslab ? "--sandslab" : "--no-sandslab",
        "-x", "c", "-c", "-o", "-"};
    const auto& options = request.options;
    if (options.sarif) args.emplace_back("--diagnostics-format=sarif");
    if (!options.profile.empty()) args.emplace_back("--profile=" + options.profile);
    for (const auto& pass : options.passes) args.emplace_back("--qml-pass=" + pass);
    for (const auto& file : options.trusted_files) {
        std::error_code ec;
        auto path = fs::canonical(file, ec);
        if (ec || !exists_as(path, fs::file_type::regular))
            return invalid("trusted QML fragment must be an existing regular file: " + file.string());
        args.emplace_back("--qml-file=" + path.string());
    }
    if (!options.fuel.empty()) args.emplace_back("--qml-fuel=" + options.fuel);
    for (const auto& param : options.parameters) args.emplace_back("--qml-param=" + param);
    if (!options.facts.empty()) args.emplace_back("--factir-facts=" + options.facts.string());
    if (!options.smtlib.empty()) args.emplace_back("--factir-smtlib=" + options.smtlib.string());
    for (const auto& directory : request.include_dirs) {
        if (!exists_as(directory, fs::file_type::directory))
            return invalid("invalid Cephyr include directory: " + directory.string());
        args.push_back("-I");
        args.push_back(directory.string());
    }
    args.push_back("-");
    std::vector<const char*> argv;
    for (const auto& arg : args) argv.push_back(arg.c_str());
    cephyr_request native{};
    native.struct_size = sizeof(native);
    native.argc = argv.size();
    native.argv = argv.data();
    native.stdin_data = input.data();
    native.stdin_size = input.size();
    native.resource_root = request.resource_root.c_str();
    native.archon_root = request.archon_root.c_str();

    cephyr_result* raw = nullptr;
    const int code = api.compile(&native, &raw);
    std::unique_ptr<cephyr_result, decltype(&cephyr_result_destroy)> owner(raw, api.result_destroy);
    size_t size = 0;
    std::string diagnostics;
    if (const char* data = api.result_diagnostics(raw, &size))
        diagnostics.assign(data, size);
    if (code != CEPHYR_OK) {
        Status status = Status::internal_error;
        if (code == CEPHYR_INVALID_ARGUMENT) status = Status::invalid_argument;
        if (code == CEPHYR_COMPILE_ERROR) status = Status::compile_error;
        if (diagnostics.empty()) diagnostics = "cephyr_compile returned status " + std::to_string(code);
        return {status, {}, std::move(diagnostics)};
    }
    const char* object = api.result_output(raw, &size);
    // Guard the publication boundary: ELF64 little-endian ET_REL / EM_X86_64.
    if (!object || size < 64 || std::string_view(object, 6) != "\177ELF\2\1" ||
        object[16] != 1 || object[17] != 0 || object[18] != 62 || object[19] != 0)
        return {Status::internal_error, {}, "Cephyr returned no AMD64 ELF relocatable object"};
    return {Status::ok, std::string(object, size), std::move(diagnostics)};
}
}
