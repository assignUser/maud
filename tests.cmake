add_compile_definitions(
  "BUILD_DIR=\"${CMAKE_BINARY_DIR}\""
  "CMAKE_CXX_COMPILER=\"${CMAKE_CXX_COMPILER}\""
)

add_test(
  NAME pytest.trike
  COMMAND
    "${CMAKE_BUILD_DIR}/documentation/venv/bin/pytest"
    "${CMAKE_SOURCE_DIR}/cmake_modules/trike"
)
