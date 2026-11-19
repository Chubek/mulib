# Build-tree integration for canonical facilities owned by the parent project.
# The principal integration pass can export an equivalent installed component
# once the canonical frontend target itself has an installation contract.
target_link_libraries(on1x_sdk PRIVATE
    "$<BUILD_INTERFACE:on1x_frontend>"
)

if(NOT TARGET on1x_sdk_semantics_ir)
    add_library(on1x_sdk_semantics_ir INTERFACE)
    add_library(On1x::SDKSemanticsIR ALIAS on1x_sdk_semantics_ir)
    target_link_libraries(on1x_sdk_semantics_ir INTERFACE
        On1x::SDK
        on1x_frontend
    )
endif()
