# When building Muad itself, we need the maud_inject_regenerate
# program to be compiled and runnable *before* configuration ends.
# This can be accomplished with try_compile:

set(
  _MAUD_INJECT_REGENERATE
  "${MAUD_DIR}/maud_inject_regenerate"
  CACHE INTERNAL
  "try_compile'd maud_inject_regenerate for bootstrapping Maud"
)

file(
  WRITE "${MAUD_DIR}/maud_.cxx"
  "
  export module maud_;
  export import :filesystem;
  "
)

try_compile(
  success
  SOURCES "${dir}/maud_inject_regenerate.cxx"
  SOURCES_TYPE CXX_MODULE
  SOURCES
    "${MAUD_DIR}/maud_.cxx"
    "${dir}/filesystem.cxx"
    "${dir}/cmake_modules/executable.cxx"
  COPY_FILE "${_MAUD_INJECT_REGENERATE}"
  OUTPUT_VARIABLE errors
  CXX_STANDARD 20
  NO_CACHE
)

if(NOT success)
  message(FATAL_ERROR "try_compile failed: ${errors}")
endif()


# When documenting Maud itself, we need to render the sphinx
# extension template with maud_in2. However we can only get
# the name of the target file with a generator expression,
# but that's enough to copy it to a known location.
set(
  _MAUD_IN2
  "${MAUD_DIR}/maud_in2"
  CACHE INTERNAL
  "copied maud_in2 for bootstrapping Maud"
)
add_custom_command(
  OUTPUT "${_MAUD_IN2}"
  DEPENDS maud_in2
  COMMAND
    "${CMAKE_COMMAND}" -E copy
    $<TARGET_FILE:maud_in2>
    "${_MAUD_IN2}"
)
