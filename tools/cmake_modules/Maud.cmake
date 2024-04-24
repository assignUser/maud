include_guard()

set(MAUD_DIR "${CMAKE_BINARY_DIR}/_maud")
set(_MAUD_SELF_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_language(GET_MESSAGE_LOG_LEVEL CMAKE_MESSAGE_LOG_LEVEL)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
elseif(CMAKE_CXX_STANDARD LESS 20)
  message(FATAL_ERROR "Building with modules requires at least C++20")
endif()


if(NOT MAUD_CXX_SOURCE_EXTENSIONS)
  set(MAUD_CXX_SOURCE_EXTENSIONS "$ENV{MAUD_CXX_SOURCE_EXTENSIONS}")
endif()
if(NOT MAUD_CXX_SOURCE_EXTENSIONS)
  set(
    MAUD_CXX_SOURCE_EXTENSIONS
    .cxx .cxxm .ixx .mxx .cpp .cppm .cc .ccm .c++ .c++m
  )
endif()


if(NOT MAUD_IGNORED_SOURCE_REGEX)
  set(MAUD_IGNORED_SOURCE_REGEX "$ENV{MAUD_IGNORED_SOURCE_REGEX}")
endif()
if(NOT MAUD_IGNORED_SOURCE_REGEX)
  set(MAUD_IGNORED_SOURCE_REGEX [[(/|^)(.*-|)build|(/|^)[._].*]])
endif()


function(_maud_glob out_var root_dir)
  set(matches)
  foreach(pattern ${ARGN})
    if(pattern MATCHES "^!(.*)$")
      list(FILTER matches EXCLUDE REGEX "${CMAKE_MATCH_1}")
      continue()
    endif()

    file(
      GLOB_RECURSE all_files
      FOLLOW_SYMLINKS
      LIST_DIRECTORIES true
      # Filters are applied to *relative* paths; otherwise directory
      # names above root_dir might spuriously include/exclude.
      RELATIVE "${root_dir}"
      "${root_dir}/*"
    )
    list(FILTER all_files INCLUDE REGEX "${pattern}")
    list(APPEND matches ${all_files})
    list(REMOVE_DUPLICATES matches)
  endforeach()
  list(TRANSFORM matches PREPEND "${root_dir}/")
  set(${out_var} "${matches}" PARENT_SCOPE)
endfunction()


function(glob out_var)
  cmake_parse_arguments(
    "" # prefix
    "CONFIGURE_DEPENDS" # options
    "" # single value arguments
    "" # multi value arguments
    ${ARGN}
  )

  set(patterns ${_UNPARSED_ARGUMENTS})
  _maud_glob(
    matches
    "${CMAKE_SOURCE_DIR}"
    ${patterns}
    "!${MAUD_IGNORED_SOURCE_REGEX}"
  )
  _maud_glob(
    gen_matches
    "${MAUD_DIR}/rendered"
    ${patterns}
  )
  list(APPEND matches ${gen_matches})
  set(${out_var} "${matches}" PARENT_SCOPE)
  if(_CONFIGURE_DEPENDS)
    file(APPEND "${MAUD_DIR}/globs" "${patterns} :MATCHED: ${matches}\n")
  endif()
endfunction()


# FIXME we need to ensure that deleting CMakeCache.txt will always
# result in a clean rebuild.


block(PROPAGATE _MAUD_BASE_DIRS)
  # Assert that globbing will exclude the build directory
  _maud_glob(all_files "${CMAKE_SOURCE_DIR}" "!${MAUD_IGNORED_SOURCE_REGEX}")
  foreach(file ${all_files})
    cmake_path(IS_PREFIX CMAKE_BINARY_DIR "${file}" NORMALIZE is_prefix)
    if(is_prefix)
      message(
        FATAL_ERROR
        "Build directory ${CMAKE_BINARY_DIR} is not excluded from globs"
      )
    endif()
  endforeach()

  # Assemble the minimal list of FILE_SET BASE_DIRS
  set(base_dirs "${CMAKE_SOURCE_DIR};${MAUD_DIR}/rendered;${_MAUD_SELF_DIR}")
  foreach(base_dir ${base_dirs})
    foreach(other_dir ${base_dirs})
      if(base_dir STREQUAL other_dir)
        continue()
      endif()

      cmake_path(IS_PREFIX base_dir "${other_dir}" is_prefix)
      if(is_prefix)
        list(REMOVE_ITEM base_dirs "${other_dir}")
      endif()
    endforeach()
  endforeach()
  set(_MAUD_BASE_DIRS BASE_DIRS ${base_dirs})
endblock()


function(_maud_get_ddi_path source_file out_var)
  set(rendered_base "${MAUD_DIR}/rendered")
  cmake_path(IS_PREFIX rendered_base "${source_file}" NORMALIZE is_gen)

  if(is_gen)
    set(source_base_directory "${MAUD_DIR}/rendered")
    set(ddi_base_directory "${MAUD_DIR}/ddi/rendered")
  else()
    set(source_base_directory "${CMAKE_SOURCE_DIR}")
    set(ddi_base_directory "${MAUD_DIR}/ddi/source")
  endif()

  cmake_path(
    RELATIVE_PATH source_file
    BASE_DIRECTORY "${source_base_directory}"
    OUTPUT_VARIABLE ddi_path
  )
  cmake_path(
    ABSOLUTE_PATH ddi_path
    BASE_DIRECTORY "${ddi_base_directory}"
    OUTPUT_VARIABLE ddi_path
  )

  if(MSVC)
    set(ddi_path "${ddi_path}.obj.ddi")
  else()
    set(ddi_path "${ddi_path}.o.ddi")
  endif()

  cmake_path(NATIVE_PATH ddi_path NORMALIZE ddi_path)
  set(${out_var} "${ddi_path}" PARENT_SCOPE)
endfunction()


function(_maud_write_scan_script source_file)
  _maud_preprocessing_scan_options("${source_file}" flags)

  # The scan script accepts one argument:
  # a suffix to apply to the ddi file (so we can write .ddi.new)
  _maud_get_ddi_path("${source_file}" ddi_path)
  cmake_path(REMOVE_EXTENSION ddi_path LAST_ONLY OUTPUT_VARIABLE obj_path)

  cmake_path(NATIVE_PATH source_file NORMALIZE source_file)

  if(MSVC)
    set(arg "%1")
  else()
    set(arg "$1")
  endif()
  set(scan "${CMAKE_CXX_SCANDEP_SOURCE}\n")
  string(REPLACE <CMAKE_CXX_COMPILER> "\"${CMAKE_CXX_COMPILER}\"" scan "${scan}")
  string(REPLACE <FLAGS> "${flags}" scan "${scan}")
  string(REPLACE <DEFINES> "" scan "${scan}")
  string(REPLACE <INCLUDES> "" scan "${scan}")
  string(REPLACE <SOURCE> "\"${source_file}\"" scan "${scan}")
  string(REPLACE <OBJECT> "\"${obj_path}\"" scan "${scan}")
  string(REPLACE <DEP_FILE> "\"${ddi_path}.d\"" scan "${scan}")
  string(REPLACE <DYNDEP_FILE> "\"${ddi_path}${arg}\"" scan "${scan}")
  string(REPLACE <PREPROCESSED_SOURCE> "\"${ddi_path}.preprocessed\"" scan "${scan}")
  if(MSVC)
    file(WRITE "${ddi_path}.scan.bat" "${scan}\n")
  else()
    file(WRITE "${ddi_path}.scan.sh" "${scan}\n")
  endif()
endfunction()


function(_maud_preprocessing_scan_options source_file out_var)
  get_source_file_property(
    flags
    "${source_file}"
    MAUD_PREPROCESSING_SCAN_OPTIONS
  )
  if(NOT flags)
    set(flags "")
  endif()
  # TODO verify maud works while using c++23
  string(APPEND flags " ${CMAKE_CXX${CMAKE_CXX_STANDARD}_STANDARD_COMPILE_OPTION}")
  get_directory_property(dirs INCLUDE_DIRECTORIES)
  foreach(dir ${dirs})
    string(APPEND flags " ${CMAKE_INCLUDE_FLAG_CXX} \"${dir}\"")
  endforeach()
  set(${out_var} "${flags}" PARENT_SCOPE)
endfunction()


function(_maud_include_directories)
  glob(include_dirs CONFIGURE_DEPENDS "(/|^)include$")
  foreach(include_dir ${include_dirs})
    message(VERBOSE "Detected include directory: ${include_dir}")
    include_directories("${include_dir}")
  endforeach()
endfunction()


function(_maud_cxx_sources)
  set(source_regex ${MAUD_CXX_SOURCE_EXTENSIONS})
  list(TRANSFORM source_regex REPLACE [=[[+][+]]=] [=[[+][+]]=])
  list(TRANSFORM source_regex REPLACE [[\.(.+)]] [[\\.\1$]])
  string(JOIN "|" source_regex ${source_regex})

  glob(source_files CONFIGURE_DEPENDS ${source_regex})
  foreach(source_file ${source_files})
    _maud_scan("${source_file}")
  endforeach()
  file(WRITE "${MAUD_DIR}/source_files.list" "${source_files}")
endfunction()


function(_maud_scan source_file)
  # TODO I'd like the scanning to be multithreaded for speed.
  # This is pretty easy since execute_process with multiple
  # COMMANDS runs them all as a pipeline (with single shared
  # stderr). Therefore all we really need is a cross platform
  # `nproc` to find out how many commands we want and a
  # `-P DoScan.cmake` which receives a subset of the files to
  # scan.
  message(VERBOSE "scanning ${source_file}")

  _maud_write_scan_script("${source_file}")
  _maud_get_ddi_path("${source_file}" ddi)

  if(MSVC)
    set(command "${ddi}.scan.bat")
  else()
    set(command sh "${ddi}.scan.sh")
  endif()
  execute_process(COMMAND ${command} COMMAND_ERROR_IS_FATAL ANY)

  # ... and read back the ddi
  file(READ "${ddi}" ddi)

  # collect all imports
  string(JSON requires ERROR_VARIABLE error GET "${ddi}" rules 0 requires)
  set(imports)
  if(NOT error)
    string(JSON i LENGTH ${requires})
    while(i GREATER 0)
      math(EXPR i "${i} - 1")
      string(JSON import GET "${requires}" "${i}" logical-name)
      list(APPEND imports "${import}")
    endwhile()
  endif()

  string(JSON module ERROR_VARIABLE error GET "${ddi}" rules 0 _maud_module-name)
  if(NOT error)
    message(FATAL_ERROR "FIXME not yet supported")
    list(REMOVE_ITEM imports "${module}")
    set(type IMPLEMENTATION)
  else()
    set(module "")
  endif()

  string(JSON provides ERROR_VARIABLE error GET "${ddi}" rules 0 provides 0)
  if(NOT error)
    string(JSON logical-name GET ${provides} logical-name)
    string(JSON is-interface GET ${provides} is-interface)
    if(logical-name MATCHES "(.+):(.+)")
      set(module ${CMAKE_MATCH_1})
      set(partition ${CMAKE_MATCH_2})
    else()
      set(module ${logical-name})
      set(partition "")
    endif()

    if(is-interface)
      set(type INTERFACE)
    else()
      set(type PROVIDER)
    endif()
  else()
    set(is-interface OFF)
  endif()

  if(module STREQUAL "")
    # No associated module was detected, but this is sometimes due to using a
    # scanner which doesn't report implementation units with _maud_module-name.
    # If it happens to be one of the special modules, then it'll be reported in
    # imports and we can avoid orphaning this source.
    if("executable" IN_LIST imports)
      list(REMOVE_ITEM imports "executable")
      set(module "executable")
      set(type IMPLEMENTATION)
    elseif("test_" IN_LIST imports)
      list(REMOVE_ITEM imports "test_")
      set(module "test_")
      set(type IMPLEMENTATION)
    else()
      # If we have no module name then we can't associate this source with a target
      message(VERBOSE "  ORPHANED, imports ${imports}")
      return()
    endif()
  endif()

  message(VERBOSE "  module ${type} ${module}:${partition}")
  message(VERBOSE "  imports ${imports}")

  if(module STREQUAL "executable")
    cmake_path(GET source_file STEM target_name)
    set(source_access PRIVATE)
  elseif(module STREQUAL "test_")
    if(COMMAND "maud_add_test")
      maud_add_test("${source_file}" "${partition}" target_name)
    else()
      _maud_add_test("${source_file}" "${partition}" target_name)
    endif()

    if(NOT TARGET "${target_name}")
      message(FATAL_ERROR "A test target named '${target_name}' should have been created")
    endif()
    set(source_access PRIVATE)
  else()
    set(target_name ${module})
    set(source_access PUBLIC)
  endif()

  if(NOT TARGET ${target_name})
    if(module STREQUAL "executable")
      add_executable(${target_name})
      message(VERBOSE "  creating executable ${target_name}")
    elseif(module MATCHES "^.*_$")
      add_library(${target_name} OBJECT)
      message(VERBOSE "  creating internal library ${target_name}")
    else()
      add_library(${target_name})
      message(VERBOSE "  creating library ${target_name}")
    endif()
  else()
    message(VERBOSE "  attaching to ${target_name}")
  endif()
  set_property(TARGET ${target_name} APPEND PROPERTY MAUD_IMPORTS "${imports}")

  set_target_properties(${target_name} PROPERTIES MAUD_SCANNED ON)
  target_compile_features(
    ${target_name}
    PUBLIC
    cxx_std_${CMAKE_CXX_STANDARD}
  )

  # attach sources
  if(type STREQUAL "IMPLEMENTATION")
    target_sources(${target_name} PRIVATE "${source_file}")
  else()
    target_sources(
      ${target_name}
      ${source_access}
      FILE_SET module_providers
      TYPE CXX_MODULES
      ${_MAUD_BASE_DIRS}
      FILES "${source_file}"
    )
  endif()

  if(type STREQUAL "INTERFACE")
    set(internal_imports "${imports}")
    list(FILTER internal_imports INCLUDE REGEX "_$")
    # Every module imported by an interface must also be installed, so
    # ensure that no internal modules were imported here.
    if(internal_imports STREQUAL "")
    else()
      message(
        FATAL_ERROR
        "${source_file} is an interface for an installed target but imports ${internal_imports}"
      )
    endif()

    if(partition STREQUAL "")
      set_target_properties(
        ${target_name}
        PROPERTIES
        MAUD_INTERFACE "${source_file}"
      )
    else()
      set_property(
        TARGET ${target_name}
        APPEND PROPERTY
        MAUD_INTERFACE_PARTITIONS "${partition}"
      )
    endif()
  endif()

  # set properties for later introspection
  set_source_files_properties(
    ${source_file}
    PROPERTIES
    MAUD_TYPE ${type}
    MAUD_MODULE ${module}
    MAUD_PARTITION "${partition}"
    MAUD_IS_INTERFACE ${is-interface}
    MAUD_IMPORTS "${imports}"
  )
endfunction()


function(_maud_add_test source_file partition out_target_name)
  cmake_path(GET source_file PARENT_PATH parent_dir)
  cmake_path(GET parent_dir FILENAME target_name)

  if(NOT TARGET "${target_name}")
    add_executable(${target_name})
    add_test(
      NAME ${target_name}
      COMMAND $<TARGET_FILE:${target_name}>
    )
    if(EXISTS "${parent_dir}/labels")
      file(STRINGS "${parent_dir}/labels" labels)
      set_property(TEST ${target_name} APPEND PROPERTY LABELS ${labels})
    endif()

    target_link_libraries(${target_name} PRIVATE GTest::gtest GTest::gtest_main)
    target_sources(
      ${target_name}
      PRIVATE
      FILE_SET module_providers
      TYPE CXX_MODULES
      ${_MAUD_BASE_DIRS}
      FILES "${_MAUD_SELF_DIR}/_test_.cxx"
    )
    target_compile_definitions(
      ${target_name}
      PRIVATE
      "-DSUITE_NAME=\"${target_name}\""
    )
    set_target_properties(
      ${target_name}
      PROPERTIES
      MAUD_INTERFACE "${_MAUD_SELF_DIR}/_test_.cxx"
    )
    target_compile_options(
      ${target_name}
      PRIVATE
      # FIXME need /Fi on MSVC I think
      -include "${_MAUD_SELF_DIR}/_test_.hxx"
    )
  endif()

  set(${out_target_name} "${target_name}" PARENT_SCOPE)

  if(partition STREQUAL "main")
    message(FATAL_ERROR "FIXME not yet supported")
  endif()
endfunction()


function(_maud_rescan source_file out_var)
  set(${out_var} "" PARENT_SCOPE)
  _maud_get_ddi_path("${source_file}" ddi)
  file(TIMESTAMP "${source_file}" src_ts "%s")
  file(TIMESTAMP "${ddi}" ddi_ts "%s")
  # Should be able to use:
  #if(ddi IS_NEWER_THAN source_file)
  # but it falls down sometimes...
  if(ddi_ts GREATER src_ts)
    message(VERBOSE "skipping rescan of ${source_file}")
    return()
  endif()
  message(VERBOSE "rescanning ${source_file}")

  if(MSVC)
    set(command "${ddi}.scan.bat" .new)
  else()
    set(command sh "${ddi}.scan.sh" .new)
  endif()
  execute_process(COMMAND ${command} COMMAND_ERROR_IS_FATAL ANY)

  file(READ "${ddi}" old_ddi)
  file(READ "${ddi}.new" new_ddi)
  string(COMPARE EQUAL "${old_ddi}" "${new_ddi}" equal)
  if(NOT equal)
    set(${out_var} "BEFORE=${old_ddi}\nAFTER=${new_ddi}" PARENT_SCOPE)
  else()
    file(REMOVE "${ddi}.new")
    file(TOUCH "${ddi}")
  endif()
endfunction()


function(_maud_finalize_targets)
  include(GNUInstallDirs)
  message(STATUS "TARGETS:")
  get_property(
    targets
    DIRECTORY .
    PROPERTY BUILDSYSTEM_TARGETS
  )
  foreach(target ${targets})
    if(target MATCHES "^_maud")
      continue()
    endif()

    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "UTILITY")
      continue()
    endif()

    message(STATUS "${target}: ${target_type}")
    get_target_property(scanned ${target} MAUD_SCANNED)
    if(NOT scanned)
      message(VERBOSE "  NOT A MAUD TARGET")
      continue()
    endif()

    get_target_property(imports ${target} MAUD_IMPORTS)
    if(NOT imports)
      set(imports "")
    endif()
    message(VERBOSE "  IMPORTS: ${imports}")

    # Link targets to imported modules
    list(FILTER imports EXCLUDE REGEX ":")
    foreach(import ${imports})
      if(NOT TARGET ${import})
        find_package("${import}.maud" REQUIRED CONFIG)
      endif()
      if(import STREQUAL target) # for example a partition might import the primary
        continue()
      endif()
      target_link_libraries(${target} PRIVATE ${import})
    endforeach()

    get_target_property(interface ${target} MAUD_INTERFACE)
    if(NOT interface)
      if(target_type STREQUAL "EXECUTABLE")
        set(interface "${_MAUD_SELF_DIR}/_executable.cxx")
        set(source_access PRIVATE)
      else()
        get_target_property(src ${target} MAUD_INTERFACE_PARTITIONS)
        set(interface "${MAUD_DIR}/injected/${target}.cxx")
        list(TRANSFORM src PREPEND "export import :")
        list(PREPEND src "export module ${target}")
        file(WRITE "${MAUD_DIR}/injected/${target}.cxx" "${src};\n")
        set(source_access PUBLIC)
        message(VERBOSE "  No primary interface supplied, injecting ${interface}")
      endif()
      target_sources(
        ${target}
        ${source_access}
        FILE_SET module_providers
        TYPE CXX_MODULES
        ${_MAUD_BASE_DIRS}
        FILES "${interface}"
      )
    endif()
    print_target_sources(${target})

    # Ensure that all targets depend on the _maud_maybe_regenerate target
    add_dependencies(${target} _maud_maybe_regenerate)

    if(target MATCHES "_$")
      install(
        TARGETS ${target} 
        EXPORT ${target}
        DESTINATION "${MAUD_DIR}/fake_install"
        CXX_MODULES_BMI
        DESTINATION "${MAUD_DIR}/fake_install"
        FILE_SET module_providers
        DESTINATION "${MAUD_DIR}/fake_install"
      )
      install(
        EXPORT ${target}
        DESTINATION "${MAUD_DIR}/fake_install"
        FILE ${target}.maud-config.cmake
      )
      continue()
    endif()

    if(target_type STREQUAL "EXECUTABLE")
      if(TEST ${target})
        continue()
      endif()
      install(
        TARGETS ${target}
        EXPORT ${target}
        DESTINATION ${CMAKE_INSTALL_BINDIR}
      )
    else()
      install(
        TARGETS ${target} 
        EXPORT ${target}
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        CXX_MODULES_BMI
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/bmi/${CMAKE_CXX_COMPILER_ID}
        # install the module interface sources
        FILE_SET module_providers
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/module_interface/${target}
      )
    endif()

    install(
      EXPORT ${target}
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake
      FILE ${target}.maud-config.cmake
    )
  endforeach()
endfunction()


function(_maud_regenerate_during_build)
  execute_process(
    COMMAND
    "${CMAKE_COMMAND}"
    -S "${CMAKE_SOURCE_DIR}"
    -B "${CMAKE_BINARY_DIR}"
    --regenerate-during-build
  )
  # --regenerated-during-build only regenerates; it doesn't resume the build.
  # FIXME what if ninja was only building one target?
  # Then we just started rebuilding *everything*.
  execute_process(
    COMMAND
    "${CMAKE_COMMAND}"
    --build "${CMAKE_BINARY_DIR}"
  )
endfunction()


function(_maud_append_to_verify)
  set(verify "${CMAKE_BINARY_DIR}/CMakeFiles/VerifyGlobs.cmake")
  if(NOT EXISTS "${verify}")
    return()
  endif()

  execute_process(
    COMMAND
    "${CMAKE_COMMAND}"
    -E touch -t
  )
endfunction()


function(_maud_maybe_regenerate)
  message(STATUS "oh yeah:
    injected verification")
endfunction()


function(_maud_setup_regenerate)
  # If the results of a scan would change at all, we need to regenerate.
  # We handle this with two operations in maybe_regenerate.cmake:
  # - test a glob for a changed file set
  # - rescan all source files on modification, comparing to the first scan
  # If either is detected, `cmake --regenerate-during-build`

  add_custom_target(
    _maud_maybe_regenerate
    COMMAND
    "${CMAKE_COMMAND}"
    -P "${MAUD_DIR}/maybe_regenerate.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )

  set(vars)
  foreach(
    var
    CMAKE_SOURCE_DIR
    CMAKE_BINARY_DIR
    CMAKE_MODULE_PATH
    CMAKE_MESSAGE_LOG_LEVEL
  )
    string(APPEND vars "set(${var} \"${${var}}\")\n")
  endforeach()

  file(
    WRITE "${MAUD_DIR}/maybe_regenerate.cmake"
    "${vars}\n"
    "cmake_policy(SET CMP0007 NEW)\n"
    "include(\"${_MAUD_SELF_DIR}/Maud.cmake\")\n"
    "_maud_do_regenerate_in_script()\n"
  )
endfunction()


function(_maud_do_regenerate_in_script)
  file(STRINGS "${MAUD_DIR}/globs" glob_lines)

  foreach(glob_line ${glob_lines})
    if("${glob_line}" MATCHES "^(.*) :MATCHED: (.*)$")
      set(pattern "${CMAKE_MATCH_1}")
      message(VERBOSE "checking for different matches to: ${pattern}")
      glob(files ${pattern})
      if("${files}" STREQUAL "${CMAKE_MATCH_2}")
        continue()
      endif()
      message(STATUS "change in matches to: ${pattern}, regenerating")
      message(VERBOSE "  (was: '${CMAKE_MATCH_2}')")
      message(VERBOSE "  (now: '${files}')")
      _maud_regenerate_during_build()
      return()
    else()
      message(FATAL_ERROR "corrupted '${MAUD_DIR}/globs' file!")
    endif()
  endforeach()

  file(READ "${MAUD_DIR}/source_files.list" source_files)
  foreach(source_file ${source_files})
    _maud_rescan("${source_file}" scan-results-differ)
    if(scan-results-differ)
      message(STATUS "change detected ${scan-results-differ}, regenerating")
      _maud_regenerate_during_build()
      return()
    endif()
  endforeach()

  message(VERBOSE "regeneration unnecessary")
endfunction()


function(_maud_setup)
  file(WRITE "${MAUD_DIR}/globs" "")

  if(NOT EXISTS "${MAUD_DIR}/rendered")
    file(MAKE_DIRECTORY "${MAUD_DIR}/rendered")
  endif()

  set_source_files_properties(
    "${_MAUD_SELF_DIR}/_executable.cxx"
    "${_MAUD_SELF_DIR}/_test_.cxx"
    PROPERTIES
    MAUD_TYPE INTERFACE
  )
endfunction()


function(_maud_cmake_modules)
  glob(module_dirs CONFIGURE_DEPENDS "(/|^)cmake_modules$")
  foreach(module_dir ${module_dirs})
    list(APPEND CMAKE_MODULE_PATH "${module_dir}")
    message(STATUS "Detected CMake module directory: ${module_dir}")
  endforeach()
  list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)
  set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)

  glob(
    auto_included
    CONFIGURE_DEPENDS
    "\\.cmake$"
    "!(/|^)cmake_modules/"
  )
  foreach(cmake_file ${auto_included})
    cmake_path(GET cmake_file PARENT_PATH dir)
    include("${cmake_file}")
  endforeach()
endfunction()


function(shim_script_as destination script)
  cmake_path(ABSOLUTE_PATH script)

  if("$ENV{PathExt}" MATCHES [[(^|;)\.bat($|;)]])
    message(
      VERBOSE
      "Windows cmd.exe detected, shimming ${script} -> ${destination}.bat"
    )
    file(
      WRITE "${destination}.bat"
      "@ECHO OFF\r\n"
      "\"${CMAKE_COMMAND}\" -P \"${script}\" -- %*\r\n"
    )
    return()
  endif()

  if("$ENV{SHELL}" STREQUAL "")
    find_program(env env NO_CACHE)
    if(NOT env)
      message(
        FATAL_ERROR
        "Could not infer shim script style, set \$ENV{SHELL}."
      )
    else()
      set(shebang "#!${env} -S sh -e")
    endif()
  else()
    set(shebang "#!$ENV{SHELL} -e")
  endif()

  message(
    VERBOSE
    "Posix-compatible `${shebang}` assumed, shimming ${script} -> ${destination}"
  )
  file(
    WRITE "${destination}"
    "${shebang}\n"
    "\"${CMAKE_COMMAND}\" -P \"${script}\" -- \"\$@\"\n"
  )
endfunction()


################################################################################
# DEBUG helpers
################################################################################
function(print_target_properties target)
  execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE properties)
  string(REGEX REPLACE ";" "\\\\;" properties "${properties}")
  string(REGEX REPLACE "\n" ";" properties "${properties}")
  list(REMOVE_DUPLICATES properties)

  foreach(property ${properties})
    # https://cmake.org/cmake/help/latest/policy/CMP0026.html
    if(property MATCHES "(^LOCATION$|^LOCATION_|_LOCATION$)")
      continue()
    endif()

    get_property(has-property TARGET ${target} PROPERTY ${property} SET)
    if(has-property)
      get_target_property(value ${target} ${property})
      message("${property} = ${value}")
    endif()
  endforeach()
endfunction()


function(print_directory_properties dir)
  execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE properties)
  string(REGEX REPLACE ";" "\\\\;" properties "${properties}")
  string(REGEX REPLACE "\n" ";" properties "${properties}")
  list(REMOVE_DUPLICATES properties)

  foreach(property ${properties})
    # https://cmake.org/cmake/help/latest/policy/CMP0026.html
    if(property MATCHES "(^LOCATION$|^LOCATION_|_LOCATION$)")
      continue()
    endif()

    get_property(has-property SOURCE "${dir}" PROPERTY ${property} SET)
    if(has-property)
      get_directory_property(value DIRECTORY "${dir}" ${property})
      message("${property} = ${value}")
    endif()
  endforeach()
endfunction()


function(print_source_file_properties src)
  execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE properties)
  string(REGEX REPLACE ";" "\\\\;" properties "${properties}")
  string(REGEX REPLACE "\n" ";" properties "${properties}")
  list(REMOVE_DUPLICATES properties)

  foreach(property ${properties})
    # https://cmake.org/cmake/help/latest/policy/CMP0026.html
    if(property MATCHES "(^LOCATION$|^LOCATION_|_LOCATION$)")
      continue()
    endif()

    get_property(has-property SOURCE "${src}" PROPERTY ${property} SET)
    if(has-property)
      get_source_file_property(value "${src}" ${property})
      message("${property} = ${value}")
    endif()
  endforeach()
endfunction()


function(print_target_sources target)
  set(source_property_names SOURCES)

  get_target_property(module_sets ${target} CXX_MODULE_SETS)
  if(NOT module_sets STREQUAL module_sets-NOTFOUND)
    foreach(module_set ${module_sets})
      list(APPEND source_property_names CXX_MODULE_SET_${module_set})
    endforeach()
  endif()

  foreach(prop ${source_property_names})
    get_target_property(sources ${target} ${prop})
    if(sources STREQUAL sources-NOTFOUND)
      continue()
    endif()
    foreach(source ${sources})
      get_source_file_property(type ${source} MAUD_TYPE)
      if(type STREQUAL type-NOTFOUND)
        set(type "NOT SCANNED")
      endif()
      message(VERBOSE "  ${source}: ${type}")
    endforeach()
  endforeach()
endfunction()


function(print_variables)
  get_cmake_property(variables VARIABLES)
  list(SORT variables)
  foreach (v ${variables})
    message(STATUS "${v}=${${v}}")
  endforeach()
endfunction()


function(add_generator_expression_display_target target_name str)
  # "$<INTERFACE_INCLUDE_DIRECTORIES:fmt::fmt-header-only>"
  add_custom_target(
    ${target_name}
    COMMAND "${CMAKE_COMMAND}" -E echo "${str}"
  )
endfunction()
