include_guard()

set(MAUD_DIR "${CMAKE_BINARY_DIR}/_maud")
set(_MAUD_SELF_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_MESSAGE_LOG_LEVEL)
  cmake_language(GET_MESSAGE_LOG_LEVEL CMAKE_MESSAGE_LOG_LEVEL)
  set(
    CMAKE_MESSAGE_LOG_LEVEL
    "${CMAKE_MESSAGE_LOG_LEVEL}"
    CACHE STRING
    "log level for cmake message() commands"
  )
endif()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
elseif(CMAKE_CXX_STANDARD LESS 20)
  message(FATAL_ERROR "Building with modules requires at least C++20")
endif()


if(NOT MAUD_CXX_SOURCE_EXTENSIONS)
  set(
    MAUD_CXX_SOURCE_EXTENSIONS
    .cxx .cxxm .ixx .mxx .cpp .cppm .cc .ccm .c++ .c++m
  )
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


function(math_assign var)
  list(JOIN ARGN " " half_expr)
  math(EXPR result "${${var}} ${half_expr}")
  set(${var} ${result} PARENT_SCOPE)
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
      math_assign(i - 1)
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
      "SHELL: $<IF:$<CXX_COMPILER_ID:MSVC>,/Fi,-include> ${_MAUD_SELF_DIR}/_test_.hxx"
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
        list(TRANSFORM src PREPEND "\nexport import :")
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

    if(TEST ${target})
      continue()
    endif()

    set(is_exe $<STREQUAL:$<TARGET_PROPERTY:${target},TYPE>,EXECUTABLE>)
    set(
      install_dir
      "$<IF:${is_exe},${CMAKE_INSTALL_BINDIR},${CMAKE_INSTALL_LIBDIR}>"
    )
    if(target MATCHES _$)
      set(junk_prefix "${MAUD_DIR}/junk/")
    else()
      set(junk_prefix "")
    endif()

    install(
      TARGETS ${target}
      EXPORT ${target}
      DESTINATION "${junk_prefix}${install_dir}"
      CXX_MODULES_BMI
      DESTINATION "${junk_prefix}${install_dir}/bmi/${CMAKE_CXX_COMPILER_ID}"
      FILE_SET module_providers
      DESTINATION "${junk_prefix}${install_dir}/module_interface/${target}"
    )
    install(
      EXPORT ${target}
      DESTINATION "${junk_prefix}${CMAKE_INSTALL_LIBDIR}/cmake"
      FILE ${target}.maud-config.cmake
      # TODO support injecting more cmake into maud-config.cmake
    )
  endforeach()
endfunction()


function(_maud_maybe_regenerate)
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
      file(TOUCH_NOCREATE "${CMAKE_BINARY_DIR}/CMakeFiles/cmake.verify_globs")
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
      file(TOUCH_NOCREATE "${CMAKE_BINARY_DIR}/CMakeFiles/cmake.verify_globs")
      return()
    endif()
  endforeach()

  message(VERBOSE "regeneration unnecessary")
endfunction()


function(_maud_setup_regenerate)
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

  file(WRITE "${MAUD_DIR}/configure_cache_variables.cmake" "${vars}")

  if("${_MAUD_INJECT_REGENERATE}" STREQUAL "")
    find_program(_MAUD_INJECT_REGENERATE maud_inject_regenerate REQUIRED)
  endif()

  # GLOB once to ensure VerifyGlobs will be generated
  file(GLOB _ CONFIGURE_DEPENDS "${MAUD_DIR}/empty/*")

  # FIXME windows...
  find_program(_MAUD_SETSID setsid REQUIRED)
  mark_as_advanced(_MAUD_SETSID)

  execute_process(
    COMMAND
    "${_MAUD_SETSID}" --fork
    "${_MAUD_INJECT_REGENERATE}"
    "${CMAKE_BINARY_DIR}"
    "${_MAUD_SELF_DIR}/Maud.cmake"
    # FIXME check that this file is empty somewhere
    OUTPUT_FILE "${MAUD_DIR}/maud_inject_regenerate.error"
    ERROR_FILE "${MAUD_DIR}/maud_inject_regenerate.error"
    COMMAND_ERROR_IS_FATAL ANY
  )
endfunction()


function(_maud_setup)
  file(WRITE "${MAUD_DIR}/globs" "")

  foreach(dir empty;junk;rendered)
    if(NOT EXISTS "${MAUD_DIR}/${dir}")
      file(MAKE_DIRECTORY "${MAUD_DIR}/${dir}")
    endif()
  endforeach()

  set_source_files_properties(
    "${_MAUD_SELF_DIR}/_executable.cxx"
    "${_MAUD_SELF_DIR}/_test_.cxx"
    PROPERTIES
    MAUD_TYPE INTERFACE
  )

  option(
    BOOL BUILD_SHARED_LIBS
    DEFAULT OFF
    HELP "Build shared libraries by default"
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


function(string_escape str out_var)
  string(JSON escaped_key ERROR_VARIABLE error SET "{}" "${str}" null)
  if(NOT error AND escaped_key MATCHES "(\".*\")")
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Couldn't escape `${str}`")
  endif()
endfunction()


function(option type name)
  # Options are considered to form a directed acyclic graph: each option may
  # declare a requirement on any other option so long as no cycles are formed.
  # Options with no requirements placed on them will have their default value.
  # Otherwise if there are requirements on the option's value then it is
  # assumed to be fixed (even if it happens to be fixed to the default).
  # New requirements can be placed on an already fixed option as long as they
  # are identical to the existing requirement; conflicting requirements will
  # result in failed configuration.
  #
  # Requirements may only be added to options through the REQUIRES argument of
  # an option(), and are the only guaranteed way to specify option values. This
  # includes user provided options (On the CLI with -DFOO=a, through ccmake,
  # etc.) which will be overridden by requirements if they would produce an
  # invalid configuration. To introduce requirements directly, use
  # `option(FORCE FOO 0)` to add a dummy option with the listed requirements.

  cmake_parse_arguments(
    "" # prefix
    "ADVANCED" # options
    "DEFAULT;HELP;VALIDATE" # single value arguments
    "ENUM;REQUIRES;FORCE" # multi value arguments
    ${ARGV}
  )

  set(types "BOOL;PATH;FILEPATH;STRING;ENUM;FORCE")
  if(NOT ("${type}" IN_LIST types))
    # Handle legacy signature
    set(type BOOL)
    set(name ${ARGV0})
    set(_HELP "${ARGV1}")
    if(ARGC GREATER 2)
      set(_DEFAULT "${ARGV2}")
    endif()
  elseif(type STREQUAL "ENUM")
    set(type STRING)
    list(POP_BACK _ENUM name)
    list(POP_BACK _ENUM r)
    list(POP_FRONT _ENUM l)
    if(NOT (l STREQUAL "(" AND r STREQUAL ")" AND _ENUM MATCHES ";"))
      message(FATAL_ERROR "ENUM option ${name} was improperly formatted")
    elseif("" IN_LIST _ENUM)
      message(FATAL_ERROR "ENUM option ${name} may not contain an empty string")
    elseif("${_DEFAULT}" STREQUAL "")
      list(GET _ENUM 0 _DEFAULT)
    endif()
  elseif(type STREQUAL "FORCE")
    set(_REQUIRES ${_FORCE})
    string(MAKE_C_IDENTIFIER "FORCE_${_REQUIRES}" name)
    set(type BOOL)
    set(_DEFAULT ON)
    set(_HELP "FORCE placeholder option")
  endif()

  if(name IN_LIST _MAUD_ALL_OPTIONS)
    return() # silently ignore duplicate declaration of the same option
  endif()
  set(_MAUD_ALL_OPTIONS ${_MAUD_ALL_OPTIONS} ${name} PARENT_SCOPE)

  if("${name}" STREQUAL "IF")
    message(FATAL_ERROR "IF is reserved and cannot be used for an option name")
  elseif(NOT (name MATCHES "[A-Z][A-Z0-9_]+"))
    message(FATAL_ERROR "Option name must be an all-caps identifier, got ${name}")
  endif()

  set(_MAUD_OPTION_GROUP_${name} "${OPTION_GROUP}" PARENT_SCOPE)

  # dedent and store lines of HELP (set(CACHE) only allows a one-line docstring)
  string(REGEX REPLACE "\n *" ";" _HELP "${_HELP}")
  set(too_long ${_HELP})
  list(
    FILTER too_long INCLUDE REGEX
    "......................................................................"
  )
  if(too_long)
    message(FATAL_ERROR "${name}'s help string exceeded the 70 char line limit")
  endif()

  # store the default
  if("${type}" STREQUAL "BOOL") # coerce BOOL to ON/OFF
    if(_DEFAULT)
      set(_DEFAULT ON)
    else()
      set(_DEFAULT OFF)
    endif()
  endif()
  set(_MAUD_DEFAULT_${name} "${_DEFAULT}" PARENT_SCOPE)

  # store requirements for this option
  set(condition ON) # as a special case, `IF ON` is implicit for BOOL options
  while(_REQUIRES)
    list(POP_FRONT _REQUIRES dependency value)
    if("${dependency}" STREQUAL "IF")
      set(condition "${value}")
      continue()
    endif()
    set(
      _MAUD_${name}_CONSTRAINS
      ${dependency} ${_MAUD_${name}_CONSTRAINS} PARENT_SCOPE
    )
    string(SHA512 req "${dependency}-${name}-${condition}")
    set(req "_MAUD_REQUIREMENT_${req}")
    set(${req} "${value}" PARENT_SCOPE)
  endwhile()

  if(NOT EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt" AND DEFINED CACHE{${name}})
    # this is a fresh build and the user has definitely configured this option
    set(_MAUD_DEFINITELY_USER_${name} "${${name}}" PARENT_SCOPE)
  endif()

  # declare the option's cache entry
  list(GET _HELP 0 help_first_line)
  set(${name} "${_DEFAULT}" CACHE ${type} "${help_first_line}")
  if(_ADVANCED)
    mark_as_advanced(${name})
  endif()

  # set the option's value from the environment if appropriate
  if(
    "${${name}}" STREQUAL "${_DEFAULT}" AND DEFINED ENV{${name}}
    AND NOT "$ENV{MAUD_DISABLE_ENVIRONMENT_OPTIONS}"
  )
    set_property(CACHE ${name} PROPERTY VALUE "$ENV{${name}}")
  endif()

  # store the enumeration of allowed STRING values
  if(NOT ("${_ENUM}" STREQUAL ""))
    set_property(CACHE ${name} PROPERTY STRINGS "${_ENUM}")
  endif()
  if(NOT ("${_VALIDATE}" STREQUAL ""))
    if(type STREQUAL "BOOL" OR NOT ("${_ENUM}" STREQUAL ""))
      message(
        FATAL_ERROR
        "${name} provided VALIDATE but this may not be used with BOOL or ENUM"
      )
    endif()
    set(_MAUD_VALIDATE_${name} ${_VALIDATE} PARENT_SCOPE)
  endif()
endfunction()


function(resolve_options)
  foreach(opt ${_MAUD_ALL_OPTIONS})
    set(${opt}_constraint_count 0)
    set(${opt}_actual_constraints)
  endforeach()

  set(constrained)
  foreach(opt ${_MAUD_ALL_OPTIONS})
    set(${opt}_constrains ${_MAUD_${opt}_CONSTRAINS})
    list(REMOVE_DUPLICATES ${opt}_constrains)
    foreach(dep ${${opt}_constrains})
      list(APPEND constrained ${dep})
      math_assign(${dep}_constraint_count + 1)
    endforeach()
  endforeach()

  list(REMOVE_DUPLICATES constrained)
  set(unconstrained ${_MAUD_ALL_OPTIONS})
  list(REMOVE_ITEM unconstrained ${constrained})

  set(resolve_ordered)
  while(unconstrained)
    list(POP_FRONT unconstrained opt)
    list(APPEND resolve_ordered ${opt})
    foreach(dep ${${opt}_constrains})
      math_assign(${dep}_constraint_count - 1)
      if("${${dep}_constraint_count}" EQUAL 0)
        # As long as there are no circular constraints then even in the worst
        # case of one long graph A -> B -> C -> D we can always pop at least
        # one unconstrained option off per iteration. (Kahn's algorithm)
        list(REMOVE_ITEM constrained ${dep})
        list(APPEND unconstrained ${dep})
      endif()
    endforeach()
  endwhile()
  
  if(constrained)
    message(FATAL_ERROR "Circular constraint between options ${constrained}")
  endif()

  foreach(opt ${resolve_ordered})
    foreach(dep ${${opt}_constrains})
      string(SHA512 req "${dep}-${opt}-${${opt}}")
      set(req "_MAUD_REQUIREMENT_${req}")
      if(NOT DEFINED "${req}")
        continue()
      endif()

      if("${${dep}_actual_constraints}" STREQUAL "")
        set_property(CACHE ${dep} PROPERTY VALUE "${${req}}")
      elseif(NOT ("${${dep}}" STREQUAL "${${req}}"))
        message(
          FATAL_ERROR
          "
          Option constraint conflict: ${dep} is constrained
          by ${${dep}_actual_constraints} to be
            \"${${dep}}\"
          but ${opt} requires it to be
            \"${${req}}\"
          "
        )
      endif()

      list(APPEND ${dep}_actual_constraints ${opt})
    endforeach()
  endforeach()

  set(cache_json "{}")

  set(defines "${MAUD_DIR}/options.h")
  file(WRITE "${defines}" "")

  set(group "")
  message(STATUS)
  foreach(opt ${_MAUD_ALL_OPTIONS})
    if(NOT (group STREQUAL "${_MAUD_OPTION_GROUP_${opt}}"))
      set(group "${_MAUD_OPTION_GROUP_${opt}}")
      message(STATUS "${group}:")
      message(STATUS)
      file(APPEND "${defines}" "\n/* ${group}: */\n\n")
    endif()

    if(NOT ("${${opt}_actual_constraints}" STREQUAL ""))
      list(JOIN ${opt}_actual_constraints ", " reason)
      set(reason "[constrained by ${reason}]")
    elseif(DEFINED ENV{${opt}} AND NOT "$ENV{MAUD_DISABLE_ENVIRONMENT_OPTIONS}")
      set(reason "[environment]")
    elseif("${${opt}}" STREQUAL "${_MAUD_DEFAULT_${opt}}")
      set(reason "[default]")
    else()
      set(reason "[user configured]")
    endif()

    get_property(type CACHE ${opt} PROPERTY TYPE)
    if(type STREQUAL "STRING")
      get_property(enum CACHE ${opt} PROPERTY STRINGS)
      if(enum)
        list(JOIN enum " " type)
        set(type "ENUM(${type})")
      endif()
    elseif(type STREQUAL "BOOL")
      set(enum OFF ON)
    else()
      set(enum)
    endif()

    string_escape("${${opt}}" quoted)
    if(type STREQUAL "BOOL")
      message(STATUS "${opt} = ${${opt}} ${reason}")
    elseif(enum)
      message(STATUS "${opt}: ${type} = ${${opt}} ${reason}")
    else()
      message(STATUS "${opt}: ${type} = ${quoted} ${reason}")
    endif()
 
    if(enum AND NOT ("${${opt}}" IN_LIST enum))
      message(FATAL_ERROR "ENUM option ${opt} must be one of ${enum}")
    elseif(DEFINED "_MAUD_VALIDATE_${opt}")
      cmake_language(EVAL CODE "${_MAUD_VALIDATE_${opt}}")
    endif()

    string(JSON cache_json SET "${cache_json}" ${opt} "${quoted}")

    get_property(help CACHE ${opt} PROPERTY HELPSTRING)
    file(APPEND "${defines}" "\n/*!\n")
    foreach(line ${help})
      message(STATUS "     ${line}")
      file(APPEND "${defines}" " *  ${line}\n")
    endforeach()
    file(APPEND "${defines}" " */\n")

    if(type STREQUAL "BOOL")
      if(${${opt}})
        file(APPEND "${defines}" "#define ${opt} 1\n")
      else()
        file(APPEND "${defines}" "#define ${opt} 0\n")
      endif()
    elseif(enum)
      foreach(e ${enum})
        if("${${opt}}" STREQUAL "${e}")
          file(APPEND "${defines}" "#define ${opt}_${e} 1\n")
        else()
          file(APPEND "${defines}" "#define ${opt}_${e} 0\n")
        endif()
      endforeach()
    else()
      file(APPEND "${defines}" "#define ${opt} ${quoted}")
    endif()

    if(
      DEFINED "_MAUD_DEFINITELY_USER_${opt}"
      AND NOT "${_MAUD_DEFINITELY_USER_${opt}}" STREQUAL "${${opt}}"
    )
      message(WARNING "Detected override of user-provided value for ${opt}")
    endif()
  endforeach()
  add_compile_options(
    "SHELL: $<IF:$<CXX_COMPILER_ID:MSVC>,/Fi,-include> ${defines}"
  )
  message(STATUS)

  set(configure_preset "{}")
  string(TIMESTAMP timestamp)
  string(
    JSON configure_preset SET "${configure_preset}"
    name "\"${timestamp}\""
  )
  string(
    JSON configure_preset SET "${configure_preset}"
    environment "{\"MAUD_DISABLE_ENVIRONMENT_OPTIONS\": \"ON\"}"
  )
  string(
    JSON configure_preset SET "${configure_preset}"
    generator "\"${CMAKE_GENERATOR}\""
  )
  string(
    JSON configure_preset SET "${configure_preset}"
    cacheVariables "${cache_json}"
  )

  if(EXISTS "${CMAKE_SOURCE_DIR}/CMakeUserPresets.json")
    file(READ "${CMAKE_SOURCE_DIR}/CMakeUserPresets.json" presets)
  else()
    set(
      presets
      [[{
          "version": 6,
          "cmakeMinimumRequired": {"major": 3, "minor": 28, "patch": 0},
          "configurePresets": []
      }]]
    )
  endif()
  string(JSON i LENGTH "${presets}" configurePresets)
  string(
    JSON presets SET "${presets}"
    configurePresets ${i} "${configure_preset}"
  )
  file(WRITE "${CMAKE_SOURCE_DIR}/CMakeUserPresets.json" "${presets}")
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


function(add_generator_expression_display_target target_name)
  # "$<INTERFACE_INCLUDE_DIRECTORIES:fmt::fmt-header-only>"
  list(JOIN ARGN "\\' \\'" str)
  add_custom_target(
    ${target_name}
    COMMAND "${CMAKE_COMMAND}" -E echo "\\'${str}\\'"
  )
endfunction()
