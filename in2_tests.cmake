return()
add_executable(maud_in2)
set(dir "${MAUD_DIR}/in2_tests")

function(add_in2_test name src)
  if(src MATCHES "^(.*)COMPILED:\n(.*)RENDERED:\n(.*)$")
    file(WRITE "${dir}/${name}.in2" "${CMAKE_MATCH_1}")
    file(WRITE "${dir}/${name}.in2.cmake" "${CMAKE_MATCH_2}")
    file(WRITE "${dir}/${name}.expected" "${CMAKE_MATCH_3}")
    add_test(
      NAME in2_compile_test.${name}
      COMMAND
      $<TARGET_FILE:maud_in2>
      <"${dir}/${name}.in2"
      #| diff -u - "${dir}/${name}.in2.cmake"
      >"${dir}/${name}.in2.cmake.actual"
    )
    add_test(
      NAME in2_test.${name}
      COMMAND "${CMAKE_COMMAND}" -P "${dir}/${name}.test.cmake"
    )
    file(
      WRITE "${dir}/${name}.test.cmake"
      "set(RENDER_FILE \"${dir}/${name}\")\n"
      [[
      file(READ "${RENDER_FILE}.in2" in2)
      execute_process(
        COMMAND $<TARGET_FILE:maud_in2>
        INPUT_FILE "${RENDER_FILE}.in2"
        OUTPUT_FILE "${RENDER_FILE}.in2.cmake"
        COMMAND_ERROR_IS_FATAL ANY
      )
      include("${RENDER_FILE}.in2.cmake")
      ]]
    )
  else()
    message(FATAL_ERROR "in2 test wasn't formatted usably")
  endif()
endfunction()

add_in2_test(empty [[
COMPILED:
RENDERED:
]])
