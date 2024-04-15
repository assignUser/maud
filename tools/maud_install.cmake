include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Install Maud's cmake modules
install(
  # TODO Make sure that we can invoke maud_cli from inside cmake
  # modules, to support generation of CMakeLists.txt in
  # ExternalProjects via CONFIGURE_COMMAND.
  FILES
  "${dir}/cmake_modules/Maud.cmake"
  "${dir}/cmake_modules/maud_cli.cmake"
  "${dir}/cmake_modules/_executable.cxx"
  "${dir}/cmake_modules/_test_.cxx"
  "${dir}/cmake_modules/_test_.hxx"
  DESTINATION
  "${CMAKE_INSTALL_LIBDIR}/cmake/Maud"
)

# Install shim scripts for the Maud CLI
string(
  CONCAT shim_code
  [[
    set(install_dir "]] "$<INSTALL_PREFIX>/${CMAKE_INSTALL_LIBDIR}/cmake/Maud" [[")
    set(shim "]] "${MAUD_DIR}/cli/maud" [[")
    include("${install_dir}/Maud.cmake")
    shim_script_as("${shim}" "${install_dir}/maud_cli.cmake")
  ]]
)
install(CODE "${shim_code}")
install(PROGRAMS "${MAUD_DIR}/cli/maud" DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${MAUD_DIR}/cli/maud.bat" DESTINATION "${CMAKE_INSTALL_BINDIR}")
