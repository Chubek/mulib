if(NOT DEFINED PERL OR NOT DEFINED SCRIPTS OR NOT DEFINED SOURCE OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "PERL SCRIPTS SOURCE OUTPUT required")
endif()
file(MAKE_DIRECTORY "${OUTPUT}")
execute_process(COMMAND "${PERL}" "${SCRIPTS}/asdl.pl"
  --header "${OUTPUT}/on1x_ast.h" --source "${OUTPUT}/on1x_ast.c"
  "${SOURCE}/grammar/On1x.asdl"
  RESULT_VARIABLE status ERROR_VARIABLE errors)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "On1x ASDL generation failed: ${errors}")
endif()
execute_process(COMMAND "${PERL}" "${SCRIPTS}/asdl.pl"
  --header "${OUTPUT}/ppx_ast.h" --source "${OUTPUT}/ppx_ast.c"
  "${SOURCE}/grammar/PPX.asdl"
  RESULT_VARIABLE status ERROR_VARIABLE errors)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "On1x PPX ASDL generation failed: ${errors}")
endif()
execute_process(COMMAND "${PERL}" "${SCRIPTS}/aurocks.pl" --no-strpool
  "${SOURCE}/grammar/On1x.g" OUTPUT_FILE "${OUTPUT}/on1x_parser.c"
  RESULT_VARIABLE status ERROR_VARIABLE errors)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "On1x Aurocks generation failed: ${errors}")
endif()
