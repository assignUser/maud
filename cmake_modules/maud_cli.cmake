# maud_cli.cmake must always be in the same directory as Maud.cmake
set(maud_path "${CMAKE_CURRENT_LIST_DIR}/Maud.cmake")

set(cmake_args)

foreach(i RANGE 4 ${CMAKE_ARGC})
  if(i EQUAL CMAKE_ARGC)
    break()
  endif()
  set(arg "${CMAKE_ARGV${i}}")

  if(arg MATCHES "^-[DWCTA].*$")
    list(APPEND cmake_args "${CMAKE_ARGV${i}}")
  elseif(arg MATCHES "^-+([^-][^= ]*)=(.*)$")
    # handle kebab case
    string(REPLACE - _ arg_name ARG_${CMAKE_MATCH_1})
    set(${arg_name} "${CMAKE_MATCH_2}")
  elseif(arg MATCHES "^-+([^-][^= ]*)$")
    string(REPLACE - _ arg_name ARG_${CMAKE_MATCH_1})
    set(${arg_name} ON)
  else()
    message(FATAL_ERROR "Unrecognized argument ${CMAKE_ARGV${i}}")
  endif()
endforeach()

function(argument name default help)
  if(NOT "${ARG_${name}}" STREQUAL "")
    set(value "${ARG_${name}}")
  else()
    set(value "${default}")
  endif()

  set(${name} "${value}" PARENT_SCOPE)

  if(default STREQUAL "OFF")
    set(default "\t")
  elseif(help STREQUAL "")
    set(default "\t[=${default}]")
  else()
    set(default "\t[=${default}]\n\t\t\t")
  endif()

  string(REPLACE _ - name ${name})
  set(help_str "${help_str}  --${name}${default}${help}\n" PARENT_SCOPE)
endfunction()

set(
  help_str
  "
Maud CLI - generate with cmake then build

"
)
argument(help OFF "-h\tShow help text")
if(ARG_h)
  set(help ON)
endif()
string(APPEND help_str "\n")

argument(log_level STATUS "Log level for cmake")
argument(generator "Ninja Multi-Config" "Build tool for generated build")
argument(
  source_dir "${CMAKE_SOURCE_DIR}"
  "Directory in which to generate CMakeLists.txt"
)
argument(
  build_dir "${CMAKE_SOURCE_DIR}/.build"
  "Path where build directory should be generated"
)

string(APPEND help_str "\n")
cmake_path(GET source_dir FILENAME project_name)
argument(
  cmake_minimum "cmake_minimum_required(VERSION 3.28)"
  ""
)
argument(
  project_command "project(\"${project_name}\" LANGUAGES CXX)"
  ""
)

string(APPEND help_str "\n")
argument(source_readonly OFF "Use symlinks to avoid writing in $source_dir")
argument(generate_only OFF "Only generate a build directory")
argument(CMakeLists_only OFF "Only generate CMakeLists.txt")

if(log_level STREQUAL "VERBOSE")
  message(STATUS "This is Larry's spirit guide, Maud. I am looking into the box...")
endif()

if(help)
  message("${help_str}")
  return()
endif()

cmake_path(ABSOLUTE_PATH source_dir)
cmake_path(NORMAL_PATH source_dir)

cmake_path(ABSOLUTE_PATH build_dir)
cmake_path(NORMAL_PATH build_dir)

if(source_readonly)
  cmake_path(IS_PREFIX source_dir "${build_dir}" build_in_source_dir)
  if(build_in_source_dir)
    message(
      FATAL_ERROR
      "Using --source-readonly requires $build_dir outside $source_dir"
    )
  endif()
  file(WRITE "${build_dir}/CMakeLists.txt" "")
  file(CREATE_LINK "${source_dir}" "${build_dir}/source" SYMBOLIC)
  set(source_dir "${build_dir}")
  set(build_dir "${build_dir}/.build")
endif()

file(
  WRITE "${source_dir}/CMakeLists.txt"
  "
  ${cmake_minimum}
  ${project_command}

  include(\"${maud_path}\")

  include(CTest)

  _maud_setup()
  _maud_cmake_modules()
  foreach(module \${_MAUD_CMAKE_MODULES})
    cmake_path(GET module PARENT_PATH dir)
    include(\"\${module}\")
  endforeach()
  if(NOT COMMAND \"maud_add_test\")
    # TODO fallback to FetchContent
    find_package(GTest)
  endif()
  _maud_in2()
  _maud_finalize_generated()
  _maud_include_directories()
  _maud_cxx_sources()
  _maud_setup_clang_format()
  _maud_finalize_targets()
  _maud_setup_doc()
  _maud_setup_regenerate()
  "
)

if(CMakeLists_only)
  return()
endif()

execute_process(
  COMMAND
  "${CMAKE_COMMAND}"
  -B "${build_dir}"
  -S "${source_dir}"
  -G "${generator}"
  ${cmake_args}
  --log-level=${log_level}
  --fresh
  RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "Generation failed.")
endif()

if(generate_only)
  return()
endif()

execute_process(
  COMMAND
  "${CMAKE_COMMAND}"
  --build "${build_dir}"
  RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "Build failed.")
endif()

