include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Install Maud's cmake modules and special target sources
install(
  # TODO Make sure that we can invoke maud_cli from inside cmake
  # modules, to support generation of CMakeLists.txt in
  # ExternalProjects via CONFIGURE_COMMAND.
  FILES
  "${dir}/cmake_modules/Maud.cmake"
  "${dir}/cmake_modules/MaudTemplateFilters.cmake"
  "${dir}/cmake_modules/maud_cli.cmake"
  "${dir}/cmake_modules/_executable.cxx"
  "${dir}/cmake_modules/_test_.cxx"
  "${dir}/cmake_modules/_test_.hxx"
  "${dir}/cmake_modules/_test_main_.cxx"
  "${dir}/cmake_modules/sphinx_conf.py"
  DESTINATION
  "${CMAKE_INSTALL_LIBDIR}/cmake/Maud"
)

# Shim and install the Maud CLI
install(
  CODE
  "
  set(install_dir \"$<INSTALL_PREFIX>/${CMAKE_INSTALL_LIBDIR}/cmake/Maud\")
  include(\"\${install_dir}/Maud.cmake\")
  shim_script_as(\"${MAUD_DIR}/cli/maud\" \"\${install_dir}/maud_cli.cmake\")
  "
)

install(
  PROGRAMS "${MAUD_DIR}/cli/maud" "${MAUD_DIR}/cli/maud.bat"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  OPTIONAL
)
