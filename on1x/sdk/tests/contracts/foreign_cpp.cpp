#include "On1xSDK-BindingGenerator.h"
#include "On1xSDK-CDecl.h"
#include "On1xSDK-ContractCheck.h"
#include "On1xSDK-ContractReview.h"
#include "On1xSDK-ExoLib.h"
#include "On1xSDK-ExoML.h"
#include "On1xSDK-Exotract.h"
#include "On1xSDK-FFI.h"
#include "On1xSDK-ForeignCallback.h"
#include "On1xSDK-ForeignModule.h"
#include "On1xSDK-ForeignResource.h"
#include "On1xSDK-ForeignType.h"
#include "On1xSDK-KernelBind.h"
#include "On1xSDK-KernelDiscovery.h"
#include "On1xSDK-Marshalling.h"

#include <mulib-cdd-accessors.h>

#include <cstdint>
#include <type_traits>

static_assert(sizeof(On1xSDK_FFISeverity) == sizeof(std::uint32_t));
static_assert(sizeof(On1xSDK_ForeignTypeKind) == sizeof(std::uint32_t));
static_assert(sizeof(On1xSDK_ForeignOwnership) == sizeof(std::uint32_t));
static_assert(sizeof(On1xSDK_ForeignProfile) == sizeof(std::uint32_t));
static_assert(std::is_standard_layout_v<On1xSDK_ForeignCandidate>);
static_assert(std::is_standard_layout_v<On1xSDK_ForeignAdmissionResult>);

int main() {
  const mulib_cdd_catalogue_t catalogue{nullptr, 0u};
  std::size_t count = 1u;
  if (mulib_cdd_catalogue_entry_count_checked(&catalogue, &count) !=
          MULIB_CDD_OK ||
      count != 0u)
    return 1;
  if (on1x_sdk_foreign_callback_supported() != ON1X_SDK_UNSUPPORTED ||
      on1x_sdk_foreign_marshalling_supported() != ON1X_SDK_UNSUPPORTED ||
      on1x_sdk_foreign_module_loading_supported() != ON1X_SDK_UNSUPPORTED ||
      on1x_sdk_binding_generator_api_supported() != ON1X_SDK_UNSUPPORTED)
    return 2;
  return 0;
}
