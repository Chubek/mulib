#include "On1xSDK.h"
#include "On1xSDK.hpp"

#include <string>
#include <type_traits>

static_assert(std::is_same_v<On1xSDK_ProjectString, On1xSDK_StringView>);
static_assert(std::is_same_v<On1xSDK_ProjectBytes, On1xSDK_ByteView>);
static_assert(std::is_same_v<On1xSDK_ProjectSpan, On1xSDK_SourceSpan>);
static_assert(std::is_same_v<
              On1xSDK_ProjectDiagnostic, On1xSDK_DiagnosticV2>);

int main()
{
    const std::string source =
        "on1xfile 1\nkind library\nname \"aggregate\"\n";
    const auto manifest = on1x::sdk::Manifest::parse(source);
    if (manifest.name() != "aggregate")
        return 1;

    On1xSDK_Version version{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_Version),
        0,
        0,
        0,
        {nullptr, 0},
    };
    return on1x_sdk_version(&version) == ON1X_SDK_OK &&
                   version.major == 2u
               ? 0
               : 2;
}
