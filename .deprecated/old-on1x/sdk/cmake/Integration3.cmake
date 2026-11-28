# The compiler archive is linked by Integration2 and exported as implementation
# support. This component retains the existing semantic/IR consumer target.
if(NOT TARGET on1x_sdk_semantics_ir)
    add_library(on1x_sdk_semantics_ir INTERFACE)
    add_library(On1x::SDKSemanticsIR ALIAS on1x_sdk_semantics_ir)
    target_link_libraries(on1x_sdk_semantics_ir INTERFACE
        On1x::SDK
        on1x_frontend
    )
endif()
