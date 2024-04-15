if(NOT BUILD_TESTING)
  return()
endif()

find_program(
  BASH_COMMAND bash
  REQUIRED
  DOC "path to bash interpreter"
)

glob(
  test_projects
  CONFIGURE_DEPENDS
  test_projects/.+\.bash$
  # (leading _ is automatically ignored)
)

message(STATUS "<test_projects> Setting up test projects")
file(
  WRITE
  "${MAUD_DIR}/test_projects/test_project.bash"
  [[
    set -e # bail on any error
    set -x # echo all commands
    export CXX=]] "${CMAKE_CXX_COMPILER}\n" [[
    export TEST_DIR=]] "${MAUD_DIR}/test_projects\n" [[
    rm -rf $TEST_DIR/usr
    mkdir $TEST_DIR/usr
    cmake --install ]] "${CMAKE_BINARY_DIR}" [[ --prefix $TEST_DIR/usr
    export PATH=$TEST_DIR/usr/bin:$PATH
    rm -rf ./*
  ]]
)

# FIXME ensure maud is shimmed for use here
foreach(test_project ${test_projects})
  cmake_path(GET test_project STEM name)
  file(WRITE "${MAUD_DIR}/test_projects/${name}/source/.mkdir-p" "")
  add_test(
    NAME test_project.${name}
    COMMAND "${BASH_COMMAND}" "${test_project}"
    WORKING_DIRECTORY "${MAUD_DIR}/test_projects/${name}/source"
  )
endforeach()

