cmake_minimum_required(VERSION 3.28)
project(glob-fix)

set(MAUD_DIR "${CMAKE_BINARY_DIR}/_maud")
file(WRITE "${MAUD_DIR}/empty/.mkdir-p" "")
file(WRITE "${MAUD_DIR}/junk/.mkdir-p" "")
file(
  # run this once to ensure there's a VerifyGlobs
  GLOB_RECURSE _
  CONFIGURE_DEPENDS
  "${MAUD_DIR}/empty/*"
)

set(FIX_SRC "${CMAKE_SOURCE_DIR}/fix_verify_globs.cxx")
set(FIX "${MAUD_DIR}/fix_verify_globs")

if(NOT EXISTS "${FIX}" OR "${FIX_SRC}" IS_NEWER_THAN "${FIX}")
  try_compile(
    success
    SOURCES "${FIX_SRC}"
    SOURCES_TYPE CXX_MODULE SOURCE_FROM_FILE _executable.cxx
    "${CMAKE_SOURCE_DIR}/../tools/cmake_modules/_executable.cxx"
    COPY_FILE "${FIX}"
    OUTPUT_VARIABLE errors
    NO_CACHE
    CXX_STANDARD 20
  )
  if(NOT success)
    message(FATAL_ERROR "try_compile failed: ${errors}")
  endif()
endif()


file(REMOVE "${CMAKE_BINARY_DIR}/CMakeFiles/VerifyGlobs.cmake")


execute_process(
  COMMAND
  setsid --fork
  "${FIX}"
  "${CMAKE_BINARY_DIR}/CMakeFiles/VerifyGlobs.cmake"
  "${CMAKE_SOURCE_DIR}/../tools/cmake_modules/Maud.cmake"
  WORKING_DIRECTORY "${MAUD_DIR}/junk"
  OUTPUT_FILE "${MAUD_DIR}/junk/fix.out"
  ERROR_FILE "${MAUD_DIR}/junk/fix.out"
  COMMAND_ERROR_IS_FATAL ANY
)
